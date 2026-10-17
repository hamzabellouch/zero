#include "../process_engine.h"

#if defined(__linux__) || !defined(_WIN32) && !defined(__APPLE__)

#include <fstream>
#include <sstream>
#include <filesystem>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <csignal>
#include <unistd.h>
#include <sys/types.h>

namespace fs = std::filesystem;

namespace ZeroCore {

    class LinuxProcessEngine : public IProcessEngine {
    public:
        LinuxProcessEngine() {
            m_numCores = sysconf(_SC_NPROCESSORS_ONLN);
            if (m_numCores <= 0) m_numCores = 1;
            m_clockTicks = sysconf(_SC_CLK_TCK);
            if (m_clockTicks <= 0) m_clockTicks = 100;
        }

        void UpdateSnapshot() override {
            CalculateSystemStats();

            auto now = std::chrono::steady_clock::now();
            uint64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

            struct RawProc {
                uint32_t pid;
                std::wstring name;
                std::wstring exeName;
                std::wstring fullPath;
                double cpuPercent = 0.0;
                uint64_t memoryBytes = 0;
                double diskBytesPerSec = 0.0;
                double netBytesPerSec = 0.0;
            };

            std::vector<RawProc> rawList;
            std::unordered_map<uint32_t, PrevData> newPrev;

            for (const auto& entry : fs::directory_iterator("/proc")) {
                if (!entry.is_directory()) continue;
                std::string stem = entry.path().filename().string();
                if (!std::all_of(stem.begin(), stem.end(), ::isdigit)) continue;

                uint32_t pid = static_cast<uint32_t>(std::stoul(stem));
                std::string procPath = "/proc/" + stem;

                RawProc proc;
                proc.pid = pid;

                // 1. Name & Full path
                char exeBuf[1024] = { 0 };
                ssize_t len = readlink((procPath + "/exe").c_str(), exeBuf, sizeof(exeBuf) - 1);
                if (len > 0) {
                    std::string full(exeBuf, len);
                    proc.fullPath = std::wstring(full.begin(), full.end());
                    std::string exe = fs::path(full).filename().string();
                    proc.exeName = std::wstring(exe.begin(), exe.end());
                    proc.name = proc.exeName;
                } else {
                    // Fallback to /proc/[pid]/comm
                    std::ifstream commFile(procPath + "/comm");
                    std::string comm;
                    if (commFile >> comm) {
                        proc.name = std::wstring(comm.begin(), comm.end());
                        proc.exeName = proc.name;
                    }
                }
                if (proc.name.empty()) continue;

                // 2. Memory (VmRSS from /proc/[pid]/status)
                std::ifstream statusFile(procPath + "/status");
                std::string line;
                while (std::getline(statusFile, line)) {
                    if (line.rfind("VmRSS:", 0) == 0) {
                        std::stringstream ss(line.substr(6));
                        uint64_t kb = 0;
                        if (ss >> kb) {
                            proc.memoryBytes = kb * 1024;
                        }
                        break;
                    }
                }

                // 3. CPU (utime + stime from /proc/[pid]/stat)
                std::ifstream statFile(procPath + "/stat");
                std::string statStr;
                uint64_t totalTicks = 0;
                if (std::getline(statFile, statStr)) {
                    auto rparen = statStr.rfind(')');
                    if (rparen != std::string::npos && rparen + 2 < statStr.length()) {
                        std::stringstream ss(statStr.substr(rparen + 2));
                        char state;
                        int ppid, pgrp, session, tty_nr, tpgid;
                        unsigned flags;
                        uint64_t minflt, cminflt, majflt, cmajflt, utime, stime;
                        if (ss >> state >> ppid >> pgrp >> session >> tty_nr >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt >> utime >> stime) {
                            totalTicks = utime + stime;
                        }
                    }
                }

                // 4. Disk I/O from /proc/[pid]/io
                uint64_t diskTotalBytes = 0;
                std::ifstream ioFile(procPath + "/io");
                while (std::getline(ioFile, line)) {
                    if (line.rfind("read_bytes:", 0) == 0) {
                        diskTotalBytes += std::stoull(line.substr(11));
                    } else if (line.rfind("write_bytes:", 0) == 0) {
                        diskTotalBytes += std::stoull(line.substr(12));
                    }
                }

                PrevData pd;
                pd.cpuTicks = totalTicks;
                pd.diskBytes = diskTotalBytes;
                pd.timestampMs = nowMs;

                auto itPrev = m_prevData.find(pid);
                if (itPrev != m_prevData.end() && nowMs > itPrev->second.timestampMs) {
                    double deltaSec = static_cast<double>(nowMs - itPrev->second.timestampMs) / 1000.0;
                    if (deltaSec > 0.05) {
                        if (totalTicks >= itPrev->second.cpuTicks) {
                            uint64_t deltaTicks = totalTicks - itPrev->second.cpuTicks;
                            double procSeconds = static_cast<double>(deltaTicks) / static_cast<double>(m_clockTicks);
                            proc.cpuPercent = std::clamp((procSeconds / deltaSec / static_cast<double>(m_numCores)) * 100.0, 0.0, 100.0);
                        }
                        if (diskTotalBytes >= itPrev->second.diskBytes) {
                            proc.diskBytesPerSec = static_cast<double>(diskTotalBytes - itPrev->second.diskBytes) / deltaSec;
                        }
                    }
                }

                newPrev[pid] = pd;
                rawList.push_back(proc);
            }

            m_prevData = std::move(newPrev);

            // Grouping by name
            std::unordered_map<std::wstring, size_t> groupMap;
            std::vector<ProcessInfo> grouped;

            for (const auto& r : rawList) {
                auto it = groupMap.find(r.name);
                if (it != groupMap.end()) {
                    size_t idx = it->second;
                    grouped[idx].instanceCount++;
                    grouped[idx].pids.push_back(r.pid);
                    grouped[idx].cpuPercent += r.cpuPercent;
                    grouped[idx].memoryBytes += r.memoryBytes;
                    grouped[idx].diskBytesPerSec += r.diskBytesPerSec;
                } else {
                    ProcessInfo item;
                    item.pid = r.pid;
                    item.pids.push_back(r.pid);
                    item.name = r.name;
                    item.exeName = r.exeName;
                    item.fullPath = r.fullPath;
                    item.instanceCount = 1;
                    item.cpuPercent = r.cpuPercent;
                    item.memoryBytes = r.memoryBytes;
                    item.diskBytesPerSec = r.diskBytesPerSec;
                    groupMap[r.name] = grouped.size();
                    grouped.push_back(item);
                }
            }

            for (auto& item : grouped) {
                item.cpuText = FormatPercent(item.cpuPercent);
                item.memoryText = FormatMemory(item.memoryBytes);
                item.diskText = FormatDiskRate(item.diskBytesPerSec);
                item.networkText = FormatNetworkRate(item.netBytesPerSec);
            }

            m_processes = std::move(grouped);
        }

        std::vector<ProcessInfo> GetProcesses() const override { return m_processes; }
        SystemStats GetSystemStats() const override { return m_sysStats; }

        bool TerminateProcessGroup(const ProcessInfo& proc) override {
            bool anyKilled = false;
            for (uint32_t pid : proc.pids) {
                if (kill(static_cast<pid_t>(pid), SIGTERM) == 0) {
                    anyKilled = true;
                }
            }
            return anyKilled;
        }

    private:
        struct PrevData {
            uint64_t cpuTicks = 0;
            uint64_t diskBytes = 0;
            uint64_t timestampMs = 0;
        };
        std::unordered_map<uint32_t, PrevData> m_prevData;
        std::vector<ProcessInfo> m_processes;
        SystemStats m_sysStats;
        long m_numCores = 1;
        long m_clockTicks = 100;
        uint64_t m_prevSysTotal = 0;
        uint64_t m_prevSysIdle = 0;

        void CalculateSystemStats() {
            // CPU
            std::ifstream statFile("/proc/stat");
            std::string line;
            if (std::getline(statFile, line) && line.rfind("cpu ", 0) == 0) {
                std::stringstream ss(line.substr(4));
                uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
                if (ss >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal) {
                    uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;
                    uint64_t idleTotal = idle + iowait;

                    if (m_prevSysTotal != 0 && total > m_prevSysTotal) {
                        uint64_t dTotal = total - m_prevSysTotal;
                        uint64_t dIdle = idleTotal - m_prevSysIdle;
                        if (dTotal >= dIdle) {
                            m_sysStats.totalCpuPercent = std::clamp((double)(dTotal - dIdle) / (double)dTotal * 100.0, 0.0, 100.0);
                        }
                    }
                    m_prevSysTotal = total;
                    m_prevSysIdle = idleTotal;
                }
            }

            // Memory
            std::ifstream memFile("/proc/meminfo");
            uint64_t memTotalKb = 0, memAvailKb = 0;
            while (std::getline(memFile, line)) {
                if (line.rfind("MemTotal:", 0) == 0) {
                    std::stringstream ss(line.substr(9));
                    ss >> memTotalKb;
                } else if (line.rfind("MemAvailable:", 0) == 0) {
                    std::stringstream ss(line.substr(13));
                    ss >> memAvailKb;
                }
            }
            if (memTotalKb > 0) {
                m_sysStats.totalPhysMemBytes = memTotalKb * 1024;
                m_sysStats.usedPhysMemBytes = (memTotalKb - memAvailKb) * 1024;
                m_sysStats.totalMemoryPercent = ((double)m_sysStats.usedPhysMemBytes / (double)m_sysStats.totalPhysMemBytes) * 100.0;
            }
        }
    };

    std::wstring IProcessEngine::FormatPercent(double val) {
        if (val < 0.05) return L"0%";
        wchar_t buf[32];
        swprintf(buf, 32, L"%.1f%%", val);
        return buf;
    }

    std::wstring IProcessEngine::FormatMemory(uint64_t bytes) {
        const double MB = 1024.0 * 1024.0;
        const double GB = 1024.0 * 1024.0 * 1024.0;
        wchar_t buf[64];
        if (bytes >= GB) swprintf(buf, 64, L"%.1f GB", bytes / GB);
        else swprintf(buf, 64, L"%.1f MB", bytes / MB);
        return buf;
    }

    std::wstring IProcessEngine::FormatDiskRate(double bytesPerSec) {
        const double KB = 1024.0;
        const double MB = 1024.0 * 1024.0;
        wchar_t buf[64];
        if (bytesPerSec < 1024.0) return L"0 MB/s";
        if (bytesPerSec >= MB) swprintf(buf, 64, L"%.1f MB/s", bytesPerSec / MB);
        else swprintf(buf, 64, L"%.1f KB/s", bytesPerSec / KB);
        return buf;
    }

    std::wstring IProcessEngine::FormatNetworkRate(double bytesPerSec) {
        const double Mbps = 1000.0 * 1000.0 / 8.0;
        const double Kbps = 1000.0 / 8.0;
        wchar_t buf[64];
        if (bytesPerSec < 100.0) return L"0 Mbps";
        if (bytesPerSec >= Mbps) swprintf(buf, 64, L"%.1f Mbps", bytesPerSec / Mbps);
        else swprintf(buf, 64, L"%.1f Kbps", bytesPerSec / Kbps);
        return buf;
    }

    std::unique_ptr<IProcessEngine> IProcessEngine::CreatePlatformEngine() {
        return std::make_unique<LinuxProcessEngine>();
    }

} // namespace ZeroCore

#endif
