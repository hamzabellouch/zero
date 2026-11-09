#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstdlib>

#if defined(_WIN32)
    #include <wchar.h>
#else
    #include <cwchar>
    #include <wctype.h>
    #ifndef _wcsicmp
    #define _wcsicmp wcscasecmp
    #endif
#endif

namespace ZeroCore {

    inline std::wstring Utf8ToWstring(const std::string& str) {
        if (str.empty()) return L"";
        std::vector<wchar_t> buf(str.size() + 1);
        size_t outLen = mbstowcs(buf.data(), str.c_str(), str.size());
        if (outLen != (size_t)-1) {
            return std::wstring(buf.data(), outLen);
        }
        return std::wstring(str.begin(), str.end());
    }

    inline std::string WstringToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        std::vector<char> buf(wstr.size() * 4 + 1);
        size_t outLen = wcstombs(buf.data(), wstr.c_str(), buf.size());
        if (outLen != (size_t)-1) {
            return std::string(buf.data(), outLen);
        }
        return std::string(wstr.begin(), wstr.end());
    }

    enum class OSType {
        Windows,
        Linux,
        macOS
    };

    enum class AppType {
        Desktop,
        SystemPackage,    // MSIX / Windows Store, APT/RPM/Pacman, PKG
        Sandboxed         // Flatpak, Snap, Mac App Store
    };

    struct AppInfo {
        std::string id;
        std::wstring displayName;
        std::wstring version;
        std::wstring publisher;
        std::wstring installDate;
        uint64_t installDateSortKey = 0;
        std::wstring installLocation;
        uint64_t estimatedSizeBytes = 0;
        std::wstring sizeFormatted;
        std::wstring iconPath;
        AppType type = AppType::Desktop;
        std::wstring uninstallString;
        bool isStoreApp = false;
        bool show32BitTag = false;
    };

    struct ProcessInfo {
        uint32_t pid = 0;
        std::vector<uint32_t> pids;
        std::wstring name;
        std::wstring exeName;
        std::wstring fullPath;
        int instanceCount = 1;
        std::wstring iconPath;

        double cpuPercent = 0.0;
        uint64_t memoryBytes = 0;
        double diskBytesPerSec = 0.0;
        double netBytesPerSec = 0.0;

        std::wstring cpuText;
        std::wstring memoryText;
        std::wstring diskText;
        std::wstring networkText;
    };

    struct SystemStats {
        double totalCpuPercent = 0.0;
        double totalMemoryPercent = 0.0;
        double totalDiskPercent = 0.0;
        double totalNetPercent = 0.0;
        uint64_t totalPhysMemBytes = 0;
        uint64_t usedPhysMemBytes = 0;
    };

    enum class LeftoverType {
        File,
        Folder,
        RegistryKey,
        Configuration
    };

    struct LeftoverItem {
        LeftoverType type = LeftoverType::File;
        std::wstring path;
        std::wstring detail;
        uint64_t sizeBytes = 0;
        bool selected = true;
    };

} // namespace ZeroCore
