#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace ZeroCore {

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
