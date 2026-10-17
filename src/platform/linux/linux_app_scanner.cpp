#include "../app_scanner.h"

#if defined(__linux__) || !defined(_WIN32) && !defined(__APPLE__)

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

namespace fs = std::filesystem;

namespace ZeroCore {

    class LinuxAppScanner : public IAppScanner {
    public:
        std::vector<AppInfo> EnumerateDesktopApps() override {
            std::vector<AppInfo> apps;
            std::vector<std::string> searchDirs = {
                "/usr/share/applications",
                "/usr/local/share/applications",
                "/var/lib/flatpak/exports/share/applications",
                "/var/lib/snapd/desktop/applications"
            };

            const char* home = getenv("HOME");
            if (!home) {
                struct passwd* pw = getpwuid(getuid());
                if (pw) home = pw->pw_dir;
            }
            if (home) {
                searchDirs.push_back(std::string(home) + "/.local/share/applications");
            }

            for (const auto& dir : searchDirs) {
                if (!fs::exists(dir)) continue;

                for (const auto& entry : fs::directory_iterator(dir)) {
                    if (entry.path().extension() == ".desktop") {
                        AppInfo info;
                        if (ParseDesktopFile(entry.path().string(), info)) {
                            apps.push_back(info);
                        }
                    }
                }
            }

            // Deduplicate by displayName
            std::vector<AppInfo> uniqueApps;
            for (const auto& a : apps) {
                bool exists = false;
                for (const auto& u : uniqueApps) {
                    if (u.displayName == a.displayName) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) uniqueApps.push_back(a);
            }

            return uniqueApps;
        }

        std::vector<AppInfo> EnumerateStorePackages() override {
            std::vector<AppInfo> packages;

            // 1. Flatpak apps
            if (fs::exists("/usr/bin/flatpak")) {
                FILE* fp = popen("flatpak list --app --columns=application,name,version 2>/dev/null", "r");
                if (fp) {
                    char line[512];
                    while (fgets(line, sizeof(line), fp)) {
                        std::string s(line);
                        std::stringstream ss(s);
                        std::string id, name, ver;
                        if (ss >> id >> name) {
                            ss >> ver;
                            AppInfo info;
                            info.id = id;
                            info.displayName = std::wstring(name.begin(), name.end());
                            info.version = std::wstring(ver.begin(), ver.end());
                            info.publisher = L"Flatpak";
                            info.type = AppType::Sandboxed;
                            packages.push_back(info);
                        }
                    }
                    pclose(fp);
                }
            }

            // 2. Snap apps
            if (fs::exists("/usr/bin/snap")) {
                FILE* fp = popen("snap list 2>/dev/null", "r");
                if (fp) {
                    char line[512];
                    bool first = true;
                    while (fgets(line, sizeof(line), fp)) {
                        if (first) { first = false; continue; }
                        std::string s(line);
                        std::stringstream ss(s);
                        std::string name, ver;
                        if (ss >> name >> ver) {
                            if (name == "core" || name == "snapd" || name == "bare") continue;
                            AppInfo info;
                            info.id = name;
                            info.displayName = std::wstring(name.begin(), name.end());
                            info.version = std::wstring(ver.begin(), ver.end());
                            info.publisher = L"Snap";
                            info.type = AppType::Sandboxed;
                            packages.push_back(info);
                        }
                    }
                    pclose(fp);
                }
            }

            return packages;
        }

        bool LaunchUninstall(const AppInfo& app, std::string& errorMsg) override {
            if (app.type == AppType::Sandboxed) {
                if (app.publisher == L"Flatpak") {
                    std::string cmd = "flatpak uninstall -y " + app.id;
                    int res = system(cmd.c_str());
                    return (res == 0);
                }
                if (app.publisher == L"Snap") {
                    std::string cmd = "pkexec snap remove " + app.id;
                    int res = system(cmd.c_str());
                    return (res == 0);
                }
            }

            // Fallback for native package manager
            if (fs::exists("/usr/bin/apt-get")) {
                std::string cmd = "pkexec apt-get remove -y " + app.id;
                return (system(cmd.c_str()) == 0);
            }
            if (fs::exists("/usr/bin/dnf")) {
                std::string cmd = "pkexec dnf remove -y " + app.id;
                return (system(cmd.c_str()) == 0);
            }
            if (fs::exists("/usr/bin/pacman")) {
                std::string cmd = "pkexec pacman -R --noconfirm " + app.id;
                return (system(cmd.c_str()) == 0);
            }

            errorMsg = "No suitable uninstaller found for package.";
            return false;
        }

        bool ForceRemove(const AppInfo& app, std::string& errorMsg) override {
            return LaunchUninstall(app, errorMsg);
        }

    private:
        bool ParseDesktopFile(const std::string& path, AppInfo& outInfo) {
            std::ifstream file(path);
            if (!file.is_open()) return false;

            std::string line;
            bool isDesktopEntry = false;
            std::string name, exec, icon, version, noDisplay;

            while (std::getline(file, line)) {
                // Trim leading/trailing whitespace
                if (line.empty() || line[0] == '#') continue;
                if (line[0] == '[') {
                    isDesktopEntry = (line == "[Desktop Entry]");
                    continue;
                }
                if (!isDesktopEntry) continue;

                auto eqPos = line.find('=');
                if (eqPos == std::string::npos) continue;

                std::string key = line.substr(0, eqPos);
                std::string val = line.substr(eqPos + 1);

                if (key == "Name" && name.empty()) name = val;
                else if (key == "Exec" && exec.empty()) exec = val;
                else if (key == "Icon" && icon.empty()) icon = val;
                else if (key == "Version" && version.empty()) version = val;
                else if (key == "NoDisplay") noDisplay = val;
            }

            if (noDisplay == "true" || name.empty()) return false;

            outInfo.id = fs::path(path).stem().string();
            outInfo.displayName = std::wstring(name.begin(), name.end());
            outInfo.version = std::wstring(version.begin(), version.end());
            outInfo.iconPath = std::wstring(icon.begin(), icon.end());
            outInfo.installLocation = std::wstring(path.begin(), path.end());
            outInfo.type = AppType::Desktop;
            return true;
        }
    };

    std::unique_ptr<IAppScanner> IAppScanner::CreatePlatformScanner() {
        return std::make_unique<LinuxAppScanner>();
    }

} // namespace ZeroCore

#endif
