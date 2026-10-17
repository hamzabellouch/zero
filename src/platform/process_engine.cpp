#include "process_engine.h"
#include <cwchar>
#include <cstdio>
#include <algorithm>

namespace ZeroCore {

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

} // namespace ZeroCore
