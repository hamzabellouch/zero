#include "../process_engine.h"

#if defined(__APPLE__)

#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <chrono>
#include <csignal>
#include <unistd.h>
#include <libproc.h>
#include <sys/sysctl.h>
#include <mach/mach.h>

namespace ZeroCore {

    class MacProcessEngine : public IProcessEngine {
    public:
        MacProcessEngine() {
            int mib[2] = { CTL_HW, HW_NCPU };
            int ncpu = 1;
            size_t len = sizeof(ncpu);
            if (sysctl(mib, 2, &ncpu, &len, nullptr, 0) == 0 && ncpu > 0) {
                m_numCores = ncpu;
            }
        }

        void UpdateSnapshot() override {
            CalculateSystemStats();

            auto now = std::chrono::steady_clock::now();
            uint64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

            int numPids = proc_listallpids(nullptr, 0);
            if (numPids <= 0) return;

            std::vector<pid_t> pids(numPids + 64);
            int bytesReturned = proc_listallpids(pids.data(), static_cast<int>(pids.size() * sizeof(pid_t)));
            int count = bytesReturned / sizeof(pid_t);

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

            for (int i = 0; i < count; ++i) {
                pid_t pid = pids[i];
                if (pid <= 0) continue;

                char pathBuf[PROC_PIDPATHINFO_MAXSIZE] = { 0 };
                char nameBuf[256] = { 0 };

                proc_pidpath(pid, pathBuf, sizeof(pathBuf));
                proc_name(pid, nameBuf, sizeof(nameBuf));

                std::string sName = (nameBuf[0] != '\0') ? nameBuf : "";
                if (sName.empty() && pathBuf[0] != '\0') {
                    std::string p(pathBuf);
                    auto pos = p.rfind('/');
                    sName = (pos != std::string::npos) ? p.substr(pos + 1) : p;
                }
                if (sName.empty()) continue;

                struct proc_taskinfo ti;
                uint64_t totalTimeNs = 0;
                uint64_t rssBytes = 0;

                if (proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &ti, sizeof(ti)) == sizeof(ti)) {
                    totalTimeNs = ti.pti_total_user + ti.pti_total_system;
                    rssBytes = ti.pti_resident_size;
                }

                RawProc proc;
                proc.pid = static_cast<uint32_t>(pid);
                proc.name = std::wstring(sName.begin(), sName.end());
                proc.exeName = proc.name;
                std::string pStr(pathBuf);
                proc.fullPath = std::wstring(pStr.begin(), pStr.end());
                proc.memoryBytes = rssBytes;

                PrevData pd;
                pd.cpuTimeNs = totalTimeNs;
                pd.timestampMs = nowMs;

                auto itPrev = m_prevData.find(proc.pid);
                if (itPrev != m_prevData.end() && nowMs > itPrev->second.timestampMs) {
                    double deltaSec = static_cast<double>(nowMs - itPrev->second.timestampMs) / 1000.0;
                    if (deltaSec > 0.05 && totalTimeNs >= itPrev->second.cpuTimeNs) {
                        uint64_t deltaNs = totalTimeNs - itPrev->second.cpuTimeNs;
                        double cpuSec = static_cast<double>(deltaNs) / 1e9;
                        proc.cpuPercent = std::clamp((cpuSec / deltaSec / static_cast<double>(m_numCores)) * 100.0, 0.0, 100.0);
                    }
                }

                newPrev[proc.pid] = pd;
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
            uint64_t cpuTimeNs = 0;
            uint64_t timestampMs = 0;
        };
        std::unordered_map<uint32_t, PrevData> m_prevData;
        std::vector<ProcessInfo> m_processes;
        SystemStats m_sysStats;
        int m_numCores = 1;

        void CalculateSystemStats() {
            // Memory stats via host_statistics64
            mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
            vm_statistics64_data_t vm_stat;
            if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm_stat, &count) == KERN_SUCCESS) {
                int mib[2] = { CTL_HW, HW_MEMSIZE };
                uint64_t memsize = 0;
                size_t len = sizeof(memsize);
                sysctl(mib, 2, &memsize, &len, nullptr, 0);

                if (memsize > 0) {
                    m_sysStats.totalPhysMemBytes = memsize;
                    uint64_t pageSize = vm_kernel_page_size;
                    uint64_t used = (vm_stat.active_count + vm_stat.wire_count) * pageSize;
                    m_sysStats.usedPhysMemBytes = used;
                    m_sysStats.totalMemoryPercent = ((double)used / (double)memsize) * 100.0;
                }
            }
        }
    };

    std::unique_ptr<IProcessEngine> IProcessEngine::CreatePlatformEngine() {
        return std::make_unique<MacProcessEngine>();
    }

} // namespace ZeroCore

#endif
