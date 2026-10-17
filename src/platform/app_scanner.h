#pragma once

#include "platform_defs.h"
#include <memory>
#include <vector>
#include <string>

namespace ZeroCore {

    class IAppScanner {
    public:
        virtual ~IAppScanner() = default;

        // Enumerate installed desktop applications
        virtual std::vector<AppInfo> EnumerateDesktopApps() = 0;

        // Enumerate system/store/sandboxed packages (Windows Store / Flatpak / Snap / Homebrew)
        virtual std::vector<AppInfo> EnumerateStorePackages() = 0;

        // Execute uninstall process for an application
        virtual bool LaunchUninstall(const AppInfo& app, std::string& errorMsg) = 0;

        // Perform direct force removal
        virtual bool ForceRemove(const AppInfo& app, std::string& errorMsg) = 0;

        // Factory function returning scanner for the current platform
        static std::unique_ptr<IAppScanner> CreatePlatformScanner();
    };

} // namespace ZeroCore
