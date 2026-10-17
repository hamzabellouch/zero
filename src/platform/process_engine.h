#pragma once

#include "platform_defs.h"
#include <memory>
#include <vector>
#include <string>

namespace ZeroCore {

    class IProcessEngine {
    public:
        virtual ~IProcessEngine() = default;

        // Take snapshot of running processes and update metrics
        virtual void UpdateSnapshot() = 0;

        // Retrieve current process list
        virtual std::vector<ProcessInfo> GetProcesses() const = 0;

        // Retrieve overall system metrics (CPU, RAM, Disk, Net)
        virtual SystemStats GetSystemStats() const = 0;

        // Terminate a process or process group
        virtual bool TerminateProcessGroup(const ProcessInfo& proc) = 0;

        // Formatting utilities
        static std::wstring FormatPercent(double val);
        static std::wstring FormatMemory(uint64_t bytes);
        static std::wstring FormatDiskRate(double bytesPerSec);
        static std::wstring FormatNetworkRate(double bytesPerSec);

        // Factory function returning process engine for the current platform
        static std::unique_ptr<IProcessEngine> CreatePlatformEngine();
    };

} // namespace ZeroCore
