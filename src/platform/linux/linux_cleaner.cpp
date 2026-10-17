#include "../leftover_cleaner.h"

#if defined(__linux__) || !defined(_WIN32) && !defined(__APPLE__)

#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>

namespace fs = std::filesystem;

namespace ZeroCore {

    class LinuxLeftoverCleaner : public ILeftoverCleaner {
    public:
        std::vector<LeftoverItem> ScanLeftovers(const AppInfo& app) override {
            std::vector<LeftoverItem> leftovers;
            if (app.displayName.empty()) return leftovers;

            std::string appNameStr(app.displayName.begin(), app.displayName.end());
            std::string lowerName = appNameStr;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

            const char* home = getenv("HOME");
            if (!home) {
                struct passwd* pw = getpwuid(getuid());
                if (pw) home = pw->pw_dir;
            }
            if (!home) return leftovers;

            std::vector<std::string> baseScanDirs = {
                std::string(home) + "/.config",
                std::string(home) + "/.local/share",
                std::string(home) + "/.cache",
                std::string(home) + "/.var/app" // Flatpak app data
            };

            for (const auto& base : baseScanDirs) {
                if (!fs::exists(base)) continue;

                try {
                    for (const auto& entry : fs::directory_iterator(base)) {
                        std::string fn = entry.path().filename().string();
                        std::string lowerFn = fn;
                        std::transform(lowerFn.begin(), lowerFn.end(), lowerFn.begin(), ::tolower);

                        if (lowerFn == lowerName || lowerFn.find(lowerName) != std::string::npos) {
                            LeftoverItem item;
                            item.type = entry.is_directory() ? LeftoverType::Folder : LeftoverType::File;
                            std::string p = entry.path().string();
                            item.path = std::wstring(p.begin(), p.end());
                            item.detail = entry.is_directory() ? L"Leftover Configuration / Data Directory" : L"Leftover Configuration File";
                            item.selected = true;

                            if (entry.is_regular_file()) {
                                item.sizeBytes = fs::file_size(entry.path());
                            } else if (entry.is_directory()) {
                                item.sizeBytes = ComputeDirSize(entry.path());
                            }
                            leftovers.push_back(item);
                        }
                    }
                } catch (...) {
                    // Ignore permission errors
                }
            }

            return leftovers;
        }

        bool CleanItems(const std::vector<LeftoverItem>& items, std::string& errorMsg) override {
            bool allSuccess = true;
            for (const auto& item : items) {
                if (!item.selected) continue;
                std::string path(item.path.begin(), item.path.end());
                try {
                    if (fs::exists(path)) {
                        fs::remove_all(path);
                    }
                } catch (const std::exception& e) {
                    allSuccess = false;
                    errorMsg += "Failed to remove " + path + ": " + e.what() + "\n";
                }
            }
            return allSuccess;
        }

    private:
        uint64_t ComputeDirSize(const fs::path& p) {
            uint64_t total = 0;
            try {
                for (const auto& e : fs::recursive_directory_iterator(p, fs::directory_options::skip_permission_denied)) {
                    if (e.is_regular_file()) {
                        total += e.file_size();
                    }
                }
            } catch (...) {}
            return total;
        }
    };

    std::unique_ptr<ILeftoverCleaner> ILeftoverCleaner::CreatePlatformCleaner() {
        return std::make_unique<LinuxLeftoverCleaner>();
    }

} // namespace ZeroCore

#endif
