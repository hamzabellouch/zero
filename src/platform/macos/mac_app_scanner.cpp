#include "../app_scanner.h"

#if defined(__APPLE__)

#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>

namespace fs = std::filesystem;

namespace ZeroCore {

    class MacAppScanner : public IAppScanner {
    public:
        std::vector<AppInfo> EnumerateDesktopApps() override {
            std::vector<AppInfo> apps;
            std::vector<std::string> searchDirs = {
                "/Applications",
                "/System/Applications"
            };

            const char* home = getenv("HOME");
            if (!home) {
                struct passwd* pw = getpwuid(getuid());
                if (pw) home = pw->pw_dir;
            }
            if (home) {
                searchDirs.push_back(std::string(home) + "/Applications");
            }

            for (const auto& dir : searchDirs) {
                if (!fs::exists(dir)) continue;

                try {
                    for (const auto& entry : fs::directory_iterator(dir)) {
                        if (entry.is_directory() && entry.path().extension() == ".app") {
                            AppInfo info;
                            if (ParseAppBundle(entry.path(), info)) {
                                apps.push_back(info);
                            }
                        }
                    }
                } catch (...) {}
            }

            return apps;
        }

        std::vector<AppInfo> EnumerateStorePackages() override {
            std::vector<AppInfo> packages;

            // Homebrew Casks
            std::vector<std::string> brewCaskDirs = {
                "/opt/homebrew/Caskroom",
                "/usr/local/Caskroom"
            };

            for (const auto& caskDir : brewCaskDirs) {
                if (!fs::exists(caskDir)) continue;
                try {
                    for (const auto& entry : fs::directory_iterator(caskDir)) {
                        if (entry.is_directory()) {
                            std::string caskName = entry.path().filename().string();
                            AppInfo info;
                            info.id = caskName;
                            info.displayName = std::wstring(caskName.begin(), caskName.end());
                            info.publisher = L"Homebrew Cask";
                            info.type = AppType::SystemPackage;
                            packages.push_back(info);
                        }
                    }
                } catch (...) {}
            }

            return packages;
        }

        bool LaunchUninstall(const AppInfo& app, std::string& errorMsg) override {
            std::string appPath(app.installLocation.begin(), app.installLocation.end());
            if (app.type == AppType::SystemPackage && app.publisher == L"Homebrew Cask") {
                std::string cmd = "brew uninstall --cask " + app.id;
                return (system(cmd.c_str()) == 0);
            }

            // Move to Trash using AppleScript (macOS Standard)
            if (!appPath.empty() && fs::exists(appPath)) {
                std::string script = "osascript -e 'tell application \"Finder\" to delete POSIX file \"" + appPath + "\"'";
                int res = system(script.c_str());
                if (res != 0) {
                    // Fallback to direct removal
                    try {
                        fs::remove_all(appPath);
                        return true;
                    } catch (const std::exception& e) {
                        errorMsg = e.what();
                        return false;
                    }
                }
                return true;
            }

            errorMsg = "Application path does not exist.";
            return false;
        }

        bool ForceRemove(const AppInfo& app, std::string& errorMsg) override {
            std::string appPath(app.installLocation.begin(), app.installLocation.end());
            if (!appPath.empty() && fs::exists(appPath)) {
                try {
                    fs::remove_all(appPath);
                    return true;
                } catch (const std::exception& e) {
                    errorMsg = e.what();
                    return false;
                }
            }
            return false;
        }

    private:
        bool ParseAppBundle(const fs::path& appBundlePath, AppInfo& outInfo) {
            fs::path plistPath = appBundlePath / "Contents" / "Info.plist";
            std::string appName = appBundlePath.stem().string();

            outInfo.id = appName;
            outInfo.displayName = std::wstring(appName.begin(), appName.end());
            std::string pStr = appBundlePath.string();
            outInfo.installLocation = std::wstring(pStr.begin(), pStr.end());
            outInfo.type = AppType::Desktop;

            // Simple XML property list parser for CFBundleName / CFBundleShortVersionString
            if (fs::exists(plistPath)) {
                std::ifstream f(plistPath);
                std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                auto extractKey = [&](const std::string& key) -> std::string {
                    std::string tag = "<key>" + key + "</key>";
                    auto pos = content.find(tag);
                    if (pos != std::string::npos) {
                        auto valPos = content.find("<string>", pos);
                        auto endVal = content.find("</string>", valPos);
                        if (valPos != std::string::npos && endVal != std::string::npos) {
                            return content.substr(valPos + 8, endVal - (valPos + 8));
                        }
                    }
                    return "";
                };

                std::string bName = extractKey("CFBundleName");
                if (!bName.empty()) {
                    outInfo.displayName = std::wstring(bName.begin(), bName.end());
                }
                std::string ver = extractKey("CFBundleShortVersionString");
                if (!ver.empty()) {
                    outInfo.version = std::wstring(ver.begin(), ver.end());
                }
            }

            return true;
        }
    };

    std::unique_ptr<IAppScanner> IAppScanner::CreatePlatformScanner() {
        return std::make_unique<MacAppScanner>();
    }

} // namespace ZeroCore

#endif
