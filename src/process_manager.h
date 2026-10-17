#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

struct ProcessItem {
    DWORD pid = 0;
    std::vector<DWORD> pids;
    std::wstring name;          // Friendly description or exe name
    std::wstring exeName;       // Base file name (e.g. "explorer.exe")
    std::wstring fullPath;      // Full executable path
    int instanceCount = 1;
    HICON hIcon = nullptr;

    double cpuPercent = 0.0;
    uint64_t memoryBytes = 0;
    double diskBytesPerSec = 0.0;
    double netBytesPerSec = 0.0;

    std::wstring cpuText;
    std::wstring memoryText;
    std::wstring diskText;
    std::wstring networkText;
};

struct SystemUsageStats {
    double totalCpuPercent = 0.0;
    double totalMemoryPercent = 0.0;
    double totalDiskPercent = 0.0;
    double totalNetPercent = 0.0;
    uint64_t totalPhysMemBytes = 0;
    uint64_t usedPhysMemBytes = 0;
};

class ProcessManager {
public:
    ProcessManager();
    ~ProcessManager();

    // Query running processes, compute metrics (CPU, RAM, Disk, Net), and group identical processes
    void UpdateSnapshot();

    const std::vector<ProcessItem>& GetProcesses() const { return m_processes; }
    const SystemUsageStats& GetSystemStats() const { return m_sysStats; }

    // Terminate single process or all instances in a group
    bool TerminateProcessGroup(const ProcessItem& item);

    // Helpers for formatting
    static std::wstring FormatPercent(double val);
    static std::wstring FormatMemory(uint64_t bytes);
    static std::wstring FormatDiskRate(double bytesPerSec);
    static std::wstring FormatNetworkRate(double bytesPerSec);

private:
    std::vector<ProcessItem> m_processes;
    SystemUsageStats m_sysStats;

    struct ProcessPrevData {
        uint64_t cpuTime100ns = 0;
        uint64_t diskBytes = 0;
        uint64_t netBytes = 0;
        uint64_t timestampMs = 0;
    };
    std::unordered_map<DWORD, ProcessPrevData> m_prevData;

    std::unordered_map<std::wstring, HICON> m_iconCache;
    HICON m_hDefaultIcon = nullptr;

    FILETIME m_prevSysIdle = {};
    FILETIME m_prevSysKernel = {};
    FILETIME m_prevSysUser = {};
    uint64_t m_prevSysTimestampMs = 0;
    DWORD m_numProcessors = 1;

    void CalculateSystemMetrics();
    HICON GetOrLoadIcon(const std::wstring& fullPath);
    std::wstring GetFriendlyName(const std::wstring& fullPath, const std::wstring& exeName);
};
