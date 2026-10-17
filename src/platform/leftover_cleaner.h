#pragma once

#include "platform_defs.h"
#include <memory>
#include <vector>
#include <string>

namespace ZeroCore {

    class ILeftoverCleaner {
    public:
        virtual ~ILeftoverCleaner() = default;

        // Perform deep scan for residual files, directories, and configuration settings
        virtual std::vector<LeftoverItem> ScanLeftovers(const AppInfo& app) = 0;

        // Permanently delete or clean selected leftover items
        virtual bool CleanItems(const std::vector<LeftoverItem>& items, std::string& errorMsg) = 0;

        // Factory function returning leftover cleaner for the current platform
        static std::unique_ptr<ILeftoverCleaner> CreatePlatformCleaner();
    };

} // namespace ZeroCore
