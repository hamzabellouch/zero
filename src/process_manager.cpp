#include "process_manager.h"

#include <tlhelp32.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <iphlpapi.h>
#include <algorithm>
#include <cwchar>
#include <cmath>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#ifndef AF_INET
#define AF_INET 2
#endif

ProcessManager::ProcessManager() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    m_numProcessors = (si.dwNumberOfProcessors > 0) ? si.dwNumberOfProcessors : 1;
    m_hDefaultIcon = LoadIcon(nullptr, IDI_APPLICATION);
}

ProcessManager::~ProcessManager() {
    for (auto& pair : m_iconCache) {
        if (pair.second && pair.second != m_hDefaultIcon) {
            DestroyIcon(pair.second);
        }
    }
    m_iconCache.clear();
}

void ProcessManager::CalculateSystemMetrics() {
    // 1. System CPU Percentage
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        ULARGE_INTEGER uIdle, uKernel, uUser;
        uIdle.LowPart = idleTime.dwLowDateTime; uIdle.HighPart = idleTime.dwHighDateTime;
        uKernel.LowPart = kernelTime.dwLowDateTime; uKernel.HighPart = kernelTime.dwHighDateTime;
        uUser.LowPart = userTime.dwLowDateTime; uUser.HighPart = userTime.dwHighDateTime;

        if (m_prevSysTimestampMs != 0) {
            ULARGE_INTEGER uPrevIdle, uPrevKernel, uPrevUser;
            uPrevIdle.LowPart = m_prevSysIdle.dwLowDateTime; uPrevIdle.HighPart = m_prevSysIdle.dwHighDateTime;
            uPrevKernel.LowPart = m_prevSysKernel.dwLowDateTime; uPrevKernel.HighPart = m_prevSysKernel.dwHighDateTime;
            uPrevUser.LowPart = m_prevSysUser.dwLowDateTime; uPrevUser.HighPart = m_prevSysUser.dwHighDateTime;

            uint64_t sysDiff = (uKernel.QuadPart - uPrevKernel.QuadPart) + (uUser.QuadPart - uPrevUser.QuadPart);
            uint64_t idleDiff = uIdle.QuadPart - uPrevIdle.QuadPart;

            if (sysDiff > 0 && sysDiff >= idleDiff) {
                double busy = (double)(sysDiff - idleDiff) / (double)sysDiff;
                m_sysStats.totalCpuPercent = std::clamp(busy * 100.0, 0.0, 100.0);
            }
        }

        m_prevSysIdle = idleTime;
        m_prevSysKernel = kernelTime;
        m_prevSysUser = userTime;
        m_prevSysTimestampMs = GetTickCount64();
    }

    // 2. System Memory Percentage and Total/Used bytes
    MEMORYSTATUSEX ms = { sizeof(ms) };
    if (GlobalMemoryStatusEx(&ms)) {
        m_sysStats.totalMemoryPercent = static_cast<double>(ms.dwMemoryLoad);
        m_sysStats.totalPhysMemBytes = ms.ullTotalPhys;
        m_sysStats.usedPhysMemBytes = ms.ullTotalPhys - ms.ullAvailPhys;
    }
}

HICON ProcessManager::GetOrLoadIcon(const std::wstring& fullPath) {
    if (fullPath.empty()) return m_hDefaultIcon;

    auto it = m_iconCache.find(fullPath);
    if (it != m_iconCache.end()) {
        return it->second;
    }

    SHFILEINFOW sfi = { 0 };
    HICON hIcon = nullptr;
    if (SHGetFileInfoW(fullPath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON)) {
        hIcon = sfi.hIcon;
    }

    if (!hIcon) {
        hIcon = m_hDefaultIcon;
    }

    m_iconCache[fullPath] = hIcon;
    return hIcon;
}

std::wstring ProcessManager::GetFriendlyName(const std::wstring& fullPath, const std::wstring& exeName) {
    if (fullPath.empty()) {
        // Fallback: strip .exe
        if (exeName.length() > 4 && _wcsicmp(exeName.c_str() + exeName.length() - 4, L".exe") == 0) {
            return exeName.substr(0, exeName.length() - 4);
        }
        return exeName;
    }

    // Try reading FileDescription from version information
    DWORD dummy = 0;
    DWORD size = GetFileVersionInfoSizeW(fullPath.c_str(), &dummy);
    if (size > 0) {
        std::vector<BYTE> data(size);
        if (GetFileVersionInfoW(fullPath.c_str(), 0, size, data.data())) {
            struct LANGANDCODEPAGE {
                WORD wLanguage;
                WORD wCodePage;
            } *lpTranslate = nullptr;
            UINT cbTranslate = 0;

            if (VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", (LPVOID*)&lpTranslate, &cbTranslate) &&
                cbTranslate >= sizeof(LANGANDCODEPAGE) && lpTranslate) {
                wchar_t subBlock[128];
                swprintf_s(subBlock, L"\\StringFileInfo\\%04x%04x\\FileDescription", lpTranslate[0].wLanguage, lpTranslate[0].wCodePage);
                LPWSTR pDesc = nullptr;
                UINT descLen = 0;
                if (VerQueryValueW(data.data(), subBlock, (LPVOID*)&pDesc, &descLen) && pDesc && wcslen(pDesc) > 0) {
                    return pDesc;
                }
            }

            // Fallback translation: 040904b0 (US English Unicode)
            LPWSTR pDesc = nullptr;
            UINT descLen = 0;
            if (VerQueryValueW(data.data(), L"\\StringFileInfo\\040904b0\\FileDescription", (LPVOID*)&pDesc, &descLen) && pDesc && wcslen(pDesc) > 0) {
                return pDesc;
            }
        }
    }

    // Common system process friendly names override
    if (_wcsicmp(exeName.c_str(), L"explorer.exe") == 0) return L"Windows Explorer";
    if (_wcsicmp(exeName.c_str(), L"taskmgr.exe") == 0) return L"Task Manager";
    if (_wcsicmp(exeName.c_str(), L"msmpeng.exe") == 0) return L"Microsoft Defender Antimalware Service";
    if (_wcsicmp(exeName.c_str(), L"system") == 0) return L"System";
    if (_wcsicmp(exeName.c_str(), L"registry") == 0) return L"Registry";

    // If exe has lowercase or exact tool name like language_server.exe, keep clean
    if (exeName.find(L'_') != std::wstring::npos) {
        return exeName;
    }

    if (exeName.length() > 4 && _wcsicmp(exeName.c_str() + exeName.length() - 4, L".exe") == 0) {
        return exeName.substr(0, exeName.length() - 4);
    }

    return exeName;
}

void ProcessManager::UpdateSnapshot() {
    CalculateSystemMetrics();

    uint64_t nowMs = GetTickCount64();

    // Query active TCP table to identify processes with network sockets
    std::unordered_map<DWORD, int> procNetSockets;
    DWORD tcpTableSize = 0;
    if (GetExtendedTcpTable(nullptr, &tcpTableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == ERROR_INSUFFICIENT_BUFFER) {
        std::vector<BYTE> buffer(tcpTableSize);
        if (GetExtendedTcpTable(buffer.data(), &tcpTableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
            auto* pTcpTable = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());
            if (pTcpTable) {
                for (DWORD i = 0; i < pTcpTable->dwNumEntries; ++i) {
                    procNetSockets[pTcpTable->table[i].dwOwningPid]++;
                }
            }
        }
    }

    // Take snapshot of running processes
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        return;
    }

    PROCESSENTRY32W pe = { sizeof(pe) };
    if (!Process32FirstW(hSnap, &pe)) {
        CloseHandle(hSnap);
        return;
    }

    // Map to group instances by friendly name / base exe path
    struct RawProcess {
        DWORD pid;
        std::wstring exeName;
        std::wstring fullPath;
        std::wstring friendlyName;
        double cpuPercent = 0.0;
        uint64_t memoryBytes = 0;
        double diskBytesPerSec = 0.0;
        double netBytesPerSec = 0.0;
        HICON hIcon = nullptr;
    };

    std::vector<RawProcess> rawList;
    std::unordered_map<DWORD, ProcessPrevData> newPrevData;

    double totalDiskActivity = 0.0;
    double totalNetActivity = 0.0;

    do {
        // Skip System Idle Process (PID 0)
        if (pe.th32ProcessID == 0) continue;

        RawProcess proc;
        proc.pid = pe.th32ProcessID;
        proc.exeName = pe.szExeFile;

        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, proc.pid);
        if (!hProc) {
            hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, proc.pid);
        }

        if (hProc) {
            // Full image path
            wchar_t pathBuf[MAX_PATH] = { 0 };
            DWORD pathLen = MAX_PATH;
            if (QueryFullProcessImageNameW(hProc, 0, pathBuf, &pathLen) && pathLen > 0) {
                proc.fullPath = pathBuf;
            }

            // Memory Working Set
            PROCESS_MEMORY_COUNTERS_EX pmc = { sizeof(pmc) };
            if (GetProcessMemoryInfo(hProc, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
                proc.memoryBytes = pmc.WorkingSetSize;
            }

            // CPU calculation via process times
            FILETIME ftCreate, ftExit, ftKernel, ftUser;
            if (GetProcessTimes(hProc, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
                ULARGE_INTEGER uK, uU;
                uK.LowPart = ftKernel.dwLowDateTime; uK.HighPart = ftKernel.dwHighDateTime;
                uU.LowPart = ftUser.dwLowDateTime; uU.HighPart = ftUser.dwHighDateTime;
                uint64_t cpuTime100ns = uK.QuadPart + uU.QuadPart;

                auto itPrev = m_prevData.find(proc.pid);
                if (itPrev != m_prevData.end() && nowMs > itPrev->second.timestampMs) {
                    uint64_t timeDeltaMs = nowMs - itPrev->second.timestampMs;
                    if (cpuTime100ns >= itPrev->second.cpuTime100ns && timeDeltaMs > 0) {
                        uint64_t cpuDiff100ns = cpuTime100ns - itPrev->second.cpuTime100ns;
                        double cpuDiffMs = static_cast<double>(cpuDiff100ns) / 10000.0;
                        double pct = (cpuDiffMs / static_cast<double>(timeDeltaMs)) / static_cast<double>(m_numProcessors) * 100.0;
                        proc.cpuPercent = std::clamp(pct, 0.0, 100.0);
                    }
                }

                // Store for next tick
                ProcessPrevData pd;
                pd.cpuTime100ns = cpuTime100ns;
                pd.timestampMs = nowMs;

                // Disk I/O counters
                IO_COUNTERS io;
                if (GetProcessIoCounters(hProc, &io)) {
                    uint64_t currentDiskBytes = io.ReadTransferCount + io.WriteTransferCount;
                    pd.diskBytes = currentDiskBytes;

                    if (itPrev != m_prevData.end() && nowMs > itPrev->second.timestampMs) {
                        uint64_t timeDeltaMs = nowMs - itPrev->second.timestampMs;
                        if (currentDiskBytes >= itPrev->second.diskBytes && timeDeltaMs > 0) {
                            uint64_t deltaDisk = currentDiskBytes - itPrev->second.diskBytes;
                            proc.diskBytesPerSec = static_cast<double>(deltaDisk) / (static_cast<double>(timeDeltaMs) / 1000.0);
                        }
                    }

                    // Network estimation via socket I/O
                    if (procNetSockets.find(proc.pid) != procNetSockets.end()) {
                        uint64_t currentNetBytes = io.OtherTransferCount;
                        pd.netBytes = currentNetBytes;
                        if (itPrev != m_prevData.end() && nowMs > itPrev->second.timestampMs) {
                            uint64_t timeDeltaMs = nowMs - itPrev->second.timestampMs;
                            if (currentNetBytes >= itPrev->second.netBytes && timeDeltaMs > 0) {
                                uint64_t deltaNet = currentNetBytes - itPrev->second.netBytes;
                                proc.netBytesPerSec = static_cast<double>(deltaNet) / (static_cast<double>(timeDeltaMs) / 1000.0);
                            }
                        }
                    }
                }

                newPrevData[proc.pid] = pd;
            }

            CloseHandle(hProc);
        } else {
            // Minimal information fallback (e.g. system tasks without open permissions)
            if (proc.pid == 4) {
                proc.friendlyName = L"System";
                proc.exeName = L"ntoskrnl.exe";
            }
        }

        proc.friendlyName = GetFriendlyName(proc.fullPath, proc.exeName);
        proc.hIcon = GetOrLoadIcon(proc.fullPath);

        totalDiskActivity += proc.diskBytesPerSec;
        totalNetActivity += proc.netBytesPerSec;

        rawList.push_back(proc);
    } while (Process32NextW(hSnap, &pe));

    CloseHandle(hSnap);
    m_prevData = std::move(newPrevData);

    // Grouping identical processes by (friendlyName + exeName)
    std::unordered_map<std::wstring, size_t> groupMap;
    std::vector<ProcessItem> groupedList;

    for (const auto& raw : rawList) {
        std::wstring key = raw.friendlyName + L"|" + raw.exeName;
        auto it = groupMap.find(key);
        if (it != groupMap.end()) {
            size_t idx = it->second;
            groupedList[idx].instanceCount++;
            groupedList[idx].pids.push_back(raw.pid);
            groupedList[idx].cpuPercent += raw.cpuPercent;
            groupedList[idx].memoryBytes += raw.memoryBytes;
            groupedList[idx].diskBytesPerSec += raw.diskBytesPerSec;
            groupedList[idx].netBytesPerSec += raw.netBytesPerSec;
            if (!groupedList[idx].hIcon && raw.hIcon) {
                groupedList[idx].hIcon = raw.hIcon;
            }
        } else {
            ProcessItem item;
            item.pid = raw.pid;
            item.pids.push_back(raw.pid);
            item.name = raw.friendlyName;
            item.exeName = raw.exeName;
            item.fullPath = raw.fullPath;
            item.instanceCount = 1;
            item.hIcon = raw.hIcon;
            item.cpuPercent = raw.cpuPercent;
            item.memoryBytes = raw.memoryBytes;
            item.diskBytesPerSec = raw.diskBytesPerSec;
            item.netBytesPerSec = raw.netBytesPerSec;

            groupMap[key] = groupedList.size();
            groupedList.push_back(item);
        }
    }

    // Format string values for display
    for (auto& item : groupedList) {
        item.cpuText = FormatPercent(item.cpuPercent);
        item.memoryText = FormatMemory(item.memoryBytes);
        item.diskText = FormatDiskRate(item.diskBytesPerSec);
        item.networkText = FormatNetworkRate(item.netBytesPerSec);
    }

    // Update global disk/net activity indicator
    m_sysStats.totalDiskPercent = std::clamp(totalDiskActivity / (50.0 * 1024.0 * 1024.0) * 100.0, 0.0, 100.0);
    m_sysStats.totalNetPercent = std::clamp(totalNetActivity / (100.0 * 1024.0 * 1024.0 / 8.0) * 100.0, 0.0, 100.0);

    m_processes = std::move(groupedList);
}

bool ProcessManager::TerminateProcessGroup(const ProcessItem& item) {
    bool anySuccess = false;
    for (DWORD pid : item.pids) {
        HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProc) {
            if (TerminateProcess(hProc, 1)) {
                anySuccess = true;
            }
            CloseHandle(hProc);
        }
    }
    return anySuccess;
}

std::wstring ProcessManager::FormatPercent(double val) {
    if (val < 0.05) {
        return L"0%";
    }
    wchar_t buf[32];
    swprintf_s(buf, L"%.1f%%", val);
    return buf;
}

std::wstring ProcessManager::FormatMemory(uint64_t bytes) {
    if (bytes == 0) return L"0 MB";

    double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    if (mb >= 1024.0) {
        double gb = mb / 1024.0;
        wchar_t buf[32];
        swprintf_s(buf, L"%.1f GB", gb);
        return buf;
    }

    wchar_t buf[32];
    swprintf_s(buf, L"%.1f MB", mb);
    return buf;
}

std::wstring ProcessManager::FormatDiskRate(double bytesPerSec) {
    if (bytesPerSec < 1024.0) {
        return L"0 MB/s";
    }

    double mbPerSec = bytesPerSec / (1024.0 * 1024.0);
    wchar_t buf[32];
    if (mbPerSec < 0.05) {
        return L"0 MB/s";
    }
    swprintf_s(buf, L"%.1f MB/s", mbPerSec);
    return buf;
}

std::wstring ProcessManager::FormatNetworkRate(double bytesPerSec) {
    if (bytesPerSec < 1024.0) {
        return L"0 Mbps";
    }

    double mbps = (bytesPerSec * 8.0) / (1000.0 * 1000.0);
    if (mbps < 0.05) {
        return L"0 Mbps";
    }
    wchar_t buf[32];
    swprintf_s(buf, L"%.1f Mbps", mbps);
    return buf;
}
