#include "cleaner.h"
#include <filesystem>
#include <shlobj.h>
#include <shellapi.h>
#include <algorithm>
#include <regex>
#include <cwctype>
#include <sstream>

#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;

namespace {
    std::wstring ToLower(const std::wstring& str) {
        std::wstring out = str;
        std::transform(out.begin(), out.end(), out.begin(), ::towlower);
        return out;
    }

    void Trim(std::wstring& s) {
        size_t start = s.find_first_not_of(L" \t\r\n");
        if (start == std::wstring::npos) {
            s.clear();
            return;
        }
        size_t end = s.find_last_not_of(L" \t\r\n");
        s = s.substr(start, end - start + 1);
    }

    // Critical system keywords, directories, and protected roots that must NEVER be touched
    const std::vector<std::wstring> G_PROTECTED_NAMES = {
        L"windows", L"microsoft", L"system32", L"syswow64", L"winsxs",
        L"system volume information", L"recovery", L"$recycle.bin",
        L"appdata", L"local", L"locallow", L"roaming", L"program files",
        L"program files (x86)", L"programdata", L"users", L"default",
        L"all users", L"public", L"desktop", L"documents", L"downloads",
        L"pictures", L"music", L"videos", L"common files", L"windowsapps",
        L"internet explorer", L"windows nt", L"windows defender",
        L"windowspowershell", L"dotnet", L"directx", L"intel", L"amd",
        L"nvidia", L"realtek", L"driver", L"drivers", L"driverstore",
        L"classes", L"policies", L"registeredapplications", L"temp", L"tmp",
        L"microsoft shared", L"windows security", L"bonjour", L"hyper-v",
        L"edge", L"onedrive", L"catroot", L"catroot2", L"installer",
        L"package cache", L"servicing", L"assembly", L"boot", L"efi",
        L"config.msi", L"msocache", L"fonts", L"cursors", L"media",
        L"prefetch", L"minidump", L"tasks", L"start menu", L"startup",
        L"wow6432node", L"clients"
    };

    // Shared top-level vendor roots that must NEVER be deleted directly as a whole folder
    const std::vector<std::wstring> G_SHARED_VENDOR_ROOTS = {
        L"adobe", L"google", L"apple", L"apple computer", L"oracle",
        L"mozilla", L"valve", L"steam", L"epic games", L"electronic arts",
        L"ea games", L"ubisoft", L"battle.net", L"blizzard", L"gog galaxy",
        L"jetbrains", L"autodesk", L"vmware", L"corsair", L"logitech",
        L"razer", L"steelseries", L"dell", L"hp", L"lenovo", L"asus", L"msi"
    };

    // Stop-words that should never be used as standalone keywords
    const std::vector<std::wstring> G_STOP_WORDS = {
        L"the", L"app", L"tool", L"tools", L"free", L"open", L"visual",
        L"total", L"universal", L"fast", L"super", L"media", L"player",
        L"client", L"server", L"suite", L"pro", L"plus", L"installer",
        L"setup", L"update", L"manager", L"studio", L"desktop", L"helper"
    };
}

bool Cleaner::IsSafeToScanOrDelete(const std::wstring& pathOrName) {
    std::wstring lower = ToLower(pathOrName);
    Trim(lower);

    if (lower.empty()) {
        return false;
    }

    // 1. Prohibit drive roots like C:\ or C:
    if (lower.length() <= 3 && lower[1] == L':') {
        return false;
    }

    // 2. Prohibit protected names directly
    for (const auto& item : G_PROTECTED_NAMES) {
        if (lower == item) {
            return false;
        }
    }

    for (const auto& vendor : G_SHARED_VENDOR_ROOTS) {
        if (lower == vendor) {
            return false;
        }
    }

    // 3. Prohibit UserProfile root (e.g. C:\Users\<Username>)
    wchar_t userProfile[MAX_PATH] = { 0 };
    if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
        if (_wcsicmp(lower.c_str(), ToLower(userProfile).c_str()) == 0) {
            return false;
        }
    }

    // 4. Check path hierarchy components
    try {
        fs::path p(pathOrName);
        std::wstring filename = ToLower(p.filename().wstring());

        for (const auto& item : G_PROTECTED_NAMES) {
            if (filename == item) {
                return false;
            }
        }

        for (const auto& vendor : G_SHARED_VENDOR_ROOTS) {
            if (filename == vendor) {
                return false;
            }
        }
    } catch (...) {}

    return true;
}

std::wstring Cleaner::NormalizeForSearch(const std::wstring& input) {
    std::wstring s = input;

    // Safely remove version patterns following spaces or 'v': e.g. " 3.0.18" or " v2.1"
    std::wregex rxVersion(L"(?:\\s+v?\\d+(?:\\.\\d+)*\\b)", std::regex_constants::icase);
    s = std::regex_replace(s, rxVersion, L"");

    // Remove architecture tags like (x64), (x86), 64-bit, 32-bit
    std::wregex rxArch(L"\\s*\\((?:x64|x86|32-bit|64-bit|win32|win64)\\)", std::regex_constants::icase);
    s = std::regex_replace(s, rxArch, L"");

    // Remove corporate suffixes from publishers
    std::wregex rxCorp(L"\\s*(LLC|Inc\\.?|Corporation|Corp\\.?|GmbH|Co\\.?|Ltd\\.?|Technologies|Software)\\s*$", std::regex_constants::icase);
    s = std::regex_replace(s, rxCorp, L"");

    Trim(s);
    return s;
}

std::vector<std::wstring> Cleaner::GenerateSearchKeywords(const InstalledApp& app) {
    std::vector<std::wstring> keywords;
    std::set<std::wstring> seen;

    auto AddKeyword = [&](const std::wstring& raw) {
        std::wstring norm = NormalizeForSearch(raw);
        std::wstring lower = ToLower(norm);
        Trim(lower);

        if (lower.length() < 3 || !IsSafeToScanOrDelete(lower)) {
            return;
        }

        // Check against stop words
        for (const auto& stopWord : G_STOP_WORDS) {
            if (lower == stopWord) return;
        }

        if (seen.find(lower) == seen.end()) {
            seen.insert(lower);
            keywords.push_back(lower);
        }
    };

    // Add normalized full name
    AddKeyword(app.displayName);

    // If app name has multiple words, add multi-word prefix if distinctive
    std::wstring normName = NormalizeForSearch(app.displayName);
    size_t firstSpace = normName.find(L' ');
    if (firstSpace != std::wstring::npos) {
        std::wstring firstWord = normName.substr(0, firstSpace);
        // Only add if it's distinctive (length >= 5 and not a vendor root or stop word)
        std::wstring lowerFirst = ToLower(firstWord);
        bool isVendor = false;
        for (const auto& v : G_SHARED_VENDOR_ROOTS) {
            if (lowerFirst == v) { isVendor = true; break; }
        }
        if (!isVendor && firstWord.length() >= 5) {
            AddKeyword(firstWord);
        }
    }

    return keywords;
}

std::vector<std::wstring> Cleaner::GenerateSiblingExclusions(const InstalledApp& targetApp, 
                                                            const std::vector<InstalledApp>& allApps) {
    std::vector<std::wstring> exclusions;
    std::set<std::wstring> seen;

    std::wstring targetNorm = ToLower(NormalizeForSearch(targetApp.displayName));
    std::wstring targetPub = ToLower(NormalizeForSearch(targetApp.publisher));

    for (const auto& otherApp : allApps) {
        // Skip self
        if (_wcsicmp(otherApp.displayName.c_str(), targetApp.displayName.c_str()) == 0) {
            continue;
        }

        std::wstring otherNorm = ToLower(NormalizeForSearch(otherApp.displayName));
        std::wstring otherPub = ToLower(NormalizeForSearch(otherApp.publisher));

        // Check if otherApp shares the same publisher or starts with targetApp name
        // (e.g. target = "Adobe", other = "Adobe Express" or "Adobe Firefly")
        bool isSibling = false;
        if (!targetPub.empty() && targetPub == otherPub) {
            isSibling = true;
        } else if (!targetNorm.empty() && otherNorm.find(targetNorm) != std::wstring::npos) {
            isSibling = true;
        }

        if (isSibling) {
            // Add full other app name as exclusion
            if (otherNorm.length() >= 3 && seen.find(otherNorm) == seen.end()) {
                seen.insert(otherNorm);
                exclusions.push_back(otherNorm);
            }

            // Extract the differential words from otherApp that are not in targetApp
            // E.g. otherApp "Adobe Express" vs targetApp "Adobe" -> extract "express"
            std::wstringstream ss(otherNorm);
            std::wstring word;
            while (ss >> word) {
                Trim(word);
                if (word.length() >= 3 && targetNorm.find(word) == std::wstring::npos) {
                    bool isStop = false;
                    for (const auto& sw : G_STOP_WORDS) {
                        if (word == sw) { isStop = true; break; }
                    }
                    if (!isStop && seen.find(word) == seen.end()) {
                        seen.insert(word);
                        exclusions.push_back(word);
                    }
                }
            }
        }
    }

    return exclusions;
}

bool Cleaner::ContainsSiblingExclusion(const std::wstring& pathOrName, 
                                      const std::vector<std::wstring>& exclusions) {
    if (exclusions.empty()) return false;
    std::wstring lower = ToLower(pathOrName);

    for (const auto& excl : exclusions) {
        if (lower == excl) return true;
        // Boundary match: contains word or separated by slash/space/dash
        size_t pos = lower.find(excl);
        if (pos != std::wstring::npos) {
            bool leftBound = (pos == 0) || !iswalnum(lower[pos - 1]);
            bool rightBound = (pos + excl.length() == lower.length()) || !iswalnum(lower[pos + excl.length()]);
            if (leftBound && rightBound) {
                return true;
            }
        }
    }
    return false;
}

uint64_t Cleaner::CalculateFolderSize(const std::wstring& folderPath) {
    uint64_t totalSize = 0;
    std::error_code ec;
    try {
        if (!fs::exists(folderPath, ec) || !fs::is_directory(folderPath, ec)) {
            return 0;
        }
        for (const auto& entry : fs::recursive_directory_iterator(folderPath, fs::directory_options::skip_permission_denied, ec)) {
            if (ec) break;
            if (entry.is_regular_file(ec)) {
                totalSize += entry.file_size(ec);
            }
        }
    } catch (...) {}
    return totalSize;
}

namespace {
    bool MatchFolderName(const std::wstring& folderName, const std::vector<std::wstring>& keywords) {
        std::wstring lower = ToLower(folderName);
        for (const auto& kw : keywords) {
            // Exact match
            if (lower == kw) return true;
            // Prefix match with word boundary
            if (lower.rfind(kw + L" ", 0) == 0 || lower.rfind(kw + L"-", 0) == 0 || lower.rfind(kw + L"_", 0) == 0) {
                return true;
            }
        }
        return false;
    }
}

void Cleaner::ScanFileSystem(const std::vector<std::wstring>& keywords, 
                             const std::vector<std::wstring>& siblingExclusions,
                             const std::wstring& installLocation,
                             std::vector<LeftoverItem>& results,
                             ScanProgressCallback onProgress,
                             const std::atomic<bool>* cancelToken) {
    std::vector<std::wstring> baseFolders;

    auto QueryKnownFolder = [&](REFKNOWNFOLDERID rfid) {
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(rfid, 0, nullptr, &path))) {
            baseFolders.push_back(path);
            CoTaskMemFree(path);
        }
    };

    QueryKnownFolder(FOLDERID_ProgramData);
    QueryKnownFolder(FOLDERID_LocalAppData);
    QueryKnownFolder(FOLDERID_RoamingAppData);
    QueryKnownFolder(FOLDERID_ProgramFiles);
    QueryKnownFolder(FOLDERID_ProgramFilesX86);
    QueryKnownFolder(FOLDERID_LocalAppDataLow);

    std::set<std::wstring> addedPaths;

    // 1. Direct Install Location Check
    if (!installLocation.empty()) {
        std::error_code ec;
        if (fs::exists(installLocation, ec) && fs::is_directory(installLocation, ec)) {
            if (IsSafeToScanOrDelete(installLocation)) {
                LeftoverItem item;
                item.type = LeftoverType::Folder;
                item.displayType = L"Installation Folder";
                item.fullPath = installLocation;
                item.sizeBytes = CalculateFolderSize(installLocation);
                item.selected = true;
                results.push_back(item);
                addedPaths.insert(ToLower(installLocation));
            }
        }
    }

    // 2. Scan each base folder for matching directories
    int folderIndex = 0;
    int totalBase = static_cast<int>(baseFolders.size());

    for (const auto& base : baseFolders) {
        if (cancelToken && cancelToken->load()) return;

        folderIndex++;
        int pct = 10 + (folderIndex * 40 / (totalBase > 0 ? totalBase : 1));

        std::error_code ec;
        if (!fs::exists(base, ec) || !fs::is_directory(base, ec)) {
            continue;
        }

        for (const auto& entry : fs::directory_iterator(base, fs::directory_options::skip_permission_denied, ec)) {
            if (cancelToken && cancelToken->load()) return;
            if (ec) break;
            if (!entry.is_directory(ec)) continue;

            std::wstring folderName = entry.path().filename().wstring();
            std::wstring lowerFolderName = ToLower(folderName);

            // Never delete protected system folders
            bool isProtected = false;
            for (const auto& p : G_PROTECTED_NAMES) {
                if (lowerFolderName == p) { isProtected = true; break; }
            }
            if (isProtected) continue;

            // Sibling App Protection: If folder belongs to a sibling application (e.g. Adobe Express vs Adobe), skip it!
            if (ContainsSiblingExclusion(folderName, siblingExclusions)) {
                continue;
            }

            if (onProgress) {
                onProgress(L"Scanning File System...", entry.path().wstring(), pct);
            }

            // Check if Level 1 folder is a shared vendor root (e.g. Adobe, Google)
            bool isSharedVendor = false;
            for (const auto& vendor : G_SHARED_VENDOR_ROOTS) {
                if (lowerFolderName == vendor) {
                    isSharedVendor = true;
                    break;
                }
            }

            if (!isSharedVendor && MatchFolderName(folderName, keywords)) {
                std::wstring fullPath = entry.path().wstring();
                if (IsSafeToScanOrDelete(fullPath) && addedPaths.find(ToLower(fullPath)) == addedPaths.end()) {
                    LeftoverItem item;
                    item.type = LeftoverType::Folder;
                    item.displayType = L"Application Data";
                    item.fullPath = fullPath;
                    item.sizeBytes = CalculateFolderSize(fullPath);
                    item.selected = true;
                    results.push_back(item);
                    addedPaths.insert(ToLower(fullPath));
                }
            } else {
                // If it's a vendor folder or non-matching parent, inspect Level 2 subfolders
                std::error_code subEc;
                for (const auto& subEntry : fs::directory_iterator(entry.path(), fs::directory_options::skip_permission_denied, subEc)) {
                    if (subEc) break;
                    if (!subEntry.is_directory(subEc)) continue;

                    std::wstring subName = subEntry.path().filename().wstring();

                    // Sibling App Protection at Level 2
                    if (ContainsSiblingExclusion(subName, siblingExclusions)) {
                        continue;
                    }

                    if (MatchFolderName(subName, keywords)) {
                        std::wstring subPath = subEntry.path().wstring();
                        if (IsSafeToScanOrDelete(subPath) && addedPaths.find(ToLower(subPath)) == addedPaths.end()) {
                            LeftoverItem item;
                            item.type = LeftoverType::Folder;
                            item.displayType = L"Program Subfolder";
                            item.fullPath = subPath;
                            item.sizeBytes = CalculateFolderSize(subPath);
                            item.selected = true;
                            results.push_back(item);
                            addedPaths.insert(ToLower(subPath));
                        }
                    }
                }
            }
        }
    }
}

void Cleaner::ScanRegistry(const std::vector<std::wstring>& keywords,
                           const std::vector<std::wstring>& siblingExclusions,
                           const InstalledApp& app,
                           std::vector<LeftoverItem>& results,
                           ScanProgressCallback onProgress,
                           const std::atomic<bool>* cancelToken) {
    struct RegNode {
        HKEY root;
        std::wstring rootName;
        std::wstring subPath;
        REGSAM samDesired;
    };

    // Notice: For WOW64 32-bit registry, open "Software" with KEY_WOW64_32KEY (Windows automatically redirects to WOW6432Node)
    std::vector<RegNode> nodes = {
        { HKEY_CURRENT_USER, L"HKCU", L"Software", 0 },
        { HKEY_LOCAL_MACHINE, L"HKLM", L"Software", KEY_WOW64_64KEY },
        { HKEY_LOCAL_MACHINE, L"HKLM", L"Software", KEY_WOW64_32KEY }
    };

    std::set<std::wstring> addedKeys;
    int nodeIdx = 0;

    for (const auto& node : nodes) {
        if (cancelToken && cancelToken->load()) return;

        nodeIdx++;
        int pct = 50 + (nodeIdx * 45 / 3);

        HKEY hKey = nullptr;
        if (RegOpenKeyExW(node.root, node.subPath.c_str(), 0, KEY_READ | node.samDesired, &hKey) != ERROR_SUCCESS) {
            continue;
        }

        DWORD index = 0;
        wchar_t keyName[256];
        DWORD keyLen = sizeof(keyName) / sizeof(keyName[0]);

        while (RegEnumKeyExW(hKey, index++, keyName, &keyLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            if (cancelToken && cancelToken->load()) {
                RegCloseKey(hKey);
                return;
            }

            std::wstring subKeyStr = keyName;
            std::wstring lowerSubKey = ToLower(subKeyStr);

            if (onProgress) {
                onProgress(L"Scanning Windows Registry...", node.rootName + L"\\" + node.subPath + L"\\" + subKeyStr, pct);
            }

            // Sibling App Protection in Registry
            if (ContainsSiblingExclusion(subKeyStr, siblingExclusions)) {
                keyLen = sizeof(keyName) / sizeof(keyName[0]);
                continue;
            }

            if (IsSafeToScanOrDelete(lowerSubKey)) {
                // Check if this subkey is a shared vendor root
                bool isSharedVendor = false;
                for (const auto& vendor : G_SHARED_VENDOR_ROOTS) {
                    if (lowerSubKey == vendor) {
                        isSharedVendor = true;
                        break;
                    }
                }

                if (!isSharedVendor && MatchFolderName(subKeyStr, keywords)) {
                    std::wstring relativePath = node.subPath + L"\\" + subKeyStr;
                    std::wstring fullDisplayPath = node.rootName + L"\\" + relativePath;

                    if (addedKeys.find(ToLower(fullDisplayPath)) == addedKeys.end()) {
                        LeftoverItem item;
                        item.type = LeftoverType::RegistryKey;
                        item.displayType = L"Registry Key";
                        item.fullPath = fullDisplayPath;
                        item.regRoot = node.root;
                        item.regSubKey = relativePath;
                        item.regSam = node.samDesired;
                        item.selected = true;
                        results.push_back(item);
                        addedKeys.insert(ToLower(fullDisplayPath));
                    }
                } else {
                    // Check subkeys under vendor key (e.g. Software\Vendor\App)
                    HKEY hSubKey = nullptr;
                    if (RegOpenKeyExW(hKey, subKeyStr.c_str(), 0, KEY_READ | node.samDesired, &hSubKey) == ERROR_SUCCESS) {
                        DWORD subIndex = 0;
                        wchar_t childKeyName[256];
                        DWORD childKeyLen = sizeof(childKeyName) / sizeof(childKeyName[0]);

                        while (RegEnumKeyExW(hSubKey, subIndex++, childKeyName, &childKeyLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                            if (cancelToken && cancelToken->load()) {
                                RegCloseKey(hSubKey);
                                RegCloseKey(hKey);
                                return;
                            }

                            std::wstring childStr = childKeyName;

                            // Sibling App Protection for subkeys
                            if (ContainsSiblingExclusion(childStr, siblingExclusions)) {
                                childKeyLen = sizeof(childKeyName) / sizeof(childKeyName[0]);
                                continue;
                            }

                            if (MatchFolderName(childStr, keywords)) {
                                std::wstring childRelative = node.subPath + L"\\" + subKeyStr + L"\\" + childStr;
                                std::wstring childDisplay = node.rootName + L"\\" + childRelative;

                                if (addedKeys.find(ToLower(childDisplay)) == addedKeys.end()) {
                                    LeftoverItem item;
                                    item.type = LeftoverType::RegistryKey;
                                    item.displayType = L"Registry Subkey";
                                    item.fullPath = childDisplay;
                                    item.regRoot = node.root;
                                    item.regSubKey = childRelative;
                                    item.regSam = node.samDesired;
                                    item.selected = true;
                                    results.push_back(item);
                                    addedKeys.insert(ToLower(childDisplay));
                                }
                            }
                            childKeyLen = sizeof(childKeyName) / sizeof(childKeyName[0]);
                        }
                        RegCloseKey(hSubKey);
                    }
                }
            }
            keyLen = sizeof(keyName) / sizeof(keyName[0]);
        }
        RegCloseKey(hKey);
    }

    // Check the uninstall registry entry itself
    if (app.rootKey != nullptr && !app.fullRegistryPath.empty()) {
        std::wstring rootName = (app.rootKey == HKEY_LOCAL_MACHINE) ? L"HKLM" : L"HKCU";
        std::wstring fullDisplay = rootName + L"\\" + app.fullRegistryPath;

        HKEY hCheck = nullptr;
        REGSAM samDesired = KEY_READ;
        if (app.is64Bit) samDesired |= KEY_WOW64_64KEY;
        else samDesired |= KEY_WOW64_32KEY;

        if (RegOpenKeyExW(app.rootKey, app.fullRegistryPath.c_str(), 0, samDesired, &hCheck) == ERROR_SUCCESS) {
            RegCloseKey(hCheck);
            if (addedKeys.find(ToLower(fullDisplay)) == addedKeys.end()) {
                LeftoverItem item;
                item.type = LeftoverType::RegistryKey;
                item.displayType = L"Uninstall Registry Key";
                item.fullPath = fullDisplay;
                item.regRoot = app.rootKey;
                item.regSubKey = app.fullRegistryPath;
                item.regSam = samDesired;
                item.selected = true;
                results.push_back(item);
                addedKeys.insert(ToLower(fullDisplay));
            }
        }
    }
}

std::vector<LeftoverItem> Cleaner::ScanLeftovers(const InstalledApp& app,
                                                const std::vector<InstalledApp>& allInstalledApps,
                                                ScanProgressCallback onProgress,
                                                const std::atomic<bool>* cancelToken) {
    std::vector<LeftoverItem> results;

    // Dedicated Leftover Scan for Windows Store (UWP / MSIX) Apps
    if (app.isWindowsStoreApp) {
        if (onProgress) {
            onProgress(L"Scanning Store App Traces...", app.displayName, 20);
        }

        // 1. Package install root in WindowsApps
        if (!app.installLocation.empty()) {
            std::error_code ec;
            if (fs::exists(app.installLocation, ec)) {
                LeftoverItem item;
                item.type = LeftoverType::Folder;
                item.displayType = L"WindowsApps Package";
                item.fullPath = app.installLocation;
                item.sizeBytes = CalculateFolderSize(app.installLocation);
                item.selected = true;
                results.push_back(item);
            }
        }

        if (cancelToken && cancelToken->load()) return results;
        if (onProgress) {
            onProgress(L"Scanning Store App Local Data...", app.displayName, 50);
        }

        // 2. LocalAppData Packages folder (settings, cache, local state)
        if (!app.packageFamilyName.empty()) {
            PWSTR localAppData = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData))) {
                std::wstring pkgDataPath = std::wstring(localAppData) + L"\\Packages\\" + app.packageFamilyName;
                CoTaskMemFree(localAppData);
                std::error_code ec;
                if (fs::exists(pkgDataPath, ec)) {
                    LeftoverItem item;
                    item.type = LeftoverType::Folder;
                    item.displayType = L"Application Data & Cache";
                    item.fullPath = pkgDataPath;
                    item.sizeBytes = CalculateFolderSize(pkgDataPath);
                    item.selected = true;
                    results.push_back(item);
                }
            }
        }

        if (cancelToken && cancelToken->load()) return results;
        if (onProgress) {
            onProgress(L"Scanning Store App Registry...", app.displayName, 80);
        }

        // 3. Registry Package Registration
        if (!app.packageFullName.empty()) {
            std::wstring regPkg = L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages\\" + app.packageFullName;
            HKEY hCheck = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, regPkg.c_str(), 0, KEY_READ, &hCheck) == ERROR_SUCCESS) {
                RegCloseKey(hCheck);
                LeftoverItem item;
                item.type = LeftoverType::RegistryKey;
                item.displayType = L"AppModel Registry Key";
                item.fullPath = L"HKCU\\" + regPkg;
                item.regRoot = HKEY_CURRENT_USER;
                item.regSubKey = regPkg;
                item.selected = true;
                results.push_back(item);
            }
        }

        // 4. Registry SystemAppData
        if (!app.packageFamilyName.empty()) {
            std::wstring regSys = L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\SystemAppData\\" + app.packageFamilyName;
            HKEY hCheck = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, regSys.c_str(), 0, KEY_READ, &hCheck) == ERROR_SUCCESS) {
                RegCloseKey(hCheck);
                LeftoverItem item;
                item.type = LeftoverType::RegistryKey;
                item.displayType = L"SystemAppData Registry Key";
                item.fullPath = L"HKCU\\" + regSys;
                item.regRoot = HKEY_CURRENT_USER;
                item.regSubKey = regSys;
                item.selected = true;
                results.push_back(item);
            }
        }

        if (onProgress) {
            onProgress(L"Finalizing Scan...", L"Complete", 100);
        }

        return results;
    }

    std::vector<std::wstring> keywords = GenerateSearchKeywords(app);
    std::vector<std::wstring> siblingExclusions = GenerateSiblingExclusions(app, allInstalledApps);

    if (keywords.empty()) {
        return results;
    }

    if (onProgress) {
        onProgress(L"Initializing Deep Scan...", app.displayName, 5);
    }

    if (cancelToken && cancelToken->load()) return results;

    // 1. Scan filesystem (5% to 50%)
    ScanFileSystem(keywords, siblingExclusions, app.installLocation, results, onProgress, cancelToken);

    if (cancelToken && cancelToken->load()) return results;

    // 2. Scan registry (50% to 95%)
    ScanRegistry(keywords, siblingExclusions, app, results, onProgress, cancelToken);

    if (cancelToken && cancelToken->load()) return results;

    if (onProgress) {
        onProgress(L"Finalizing Scan...", L"Analyzing trace results", 100);
    }

    return results;
}

namespace {
    bool SafeDeleteToRecycleBin(const std::wstring& path) {
        // Double-null terminated string required for SHFileOperationW
        std::vector<wchar_t> fromBuf(path.length() + 2, 0);
        wcsncpy_s(fromBuf.data(), fromBuf.size(), path.c_str(), path.length());

        SHFILEOPSTRUCTW fileOp = { 0 };
        fileOp.wFunc = FO_DELETE;
        fileOp.pFrom = fromBuf.data();
        fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;

        int res = SHFileOperationW(&fileOp);
        return (res == 0 && !fileOp.fAnyOperationsAborted);
    }
}

bool Cleaner::DeleteSelectedItems(std::vector<LeftoverItem>& items, std::vector<std::wstring>& errors) {
    bool allSuccess = true;

    for (auto& item : items) {
        if (!item.selected) continue;

        if (item.type == LeftoverType::Folder || item.type == LeftoverType::File) {
            std::error_code ec;
            if (!fs::exists(item.fullPath, ec)) {
                continue; // Already removed
            }

            // Remove read-only attribute if set
            SetFileAttributesW(item.fullPath.c_str(), FILE_ATTRIBUTE_NORMAL);

            // Attempt safe removal to Recycle Bin first
            if (!SafeDeleteToRecycleBin(item.fullPath)) {
                // Fallback to direct removal
                if (item.type == LeftoverType::Folder) {
                    fs::remove_all(item.fullPath, ec);
                } else {
                    fs::remove(item.fullPath, ec);
                }

                if (ec) {
                    errors.push_back(L"Failed to delete file/folder: " + item.fullPath + L" (" + std::to_wstring(ec.value()) + L")");
                    allSuccess = false;
                }
            }
        } else if (item.type == LeftoverType::RegistryKey) {
            if (item.regRoot != nullptr && !item.regSubKey.empty()) {
                // Find parent and child key name
                size_t lastSlash = item.regSubKey.find_last_of(L'\\');
                if (lastSlash != std::wstring::npos) {
                    std::wstring parentPath = item.regSubKey.substr(0, lastSlash);
                    std::wstring subKeyName = item.regSubKey.substr(lastSlash + 1);

                    HKEY hParent = nullptr;
                    REGSAM accessSam = KEY_ALL_ACCESS;
                    if (item.regSam != 0) {
                        accessSam |= item.regSam;
                    }

                    LONG openRes = RegOpenKeyExW(item.regRoot, parentPath.c_str(), 0, accessSam, &hParent);
                    if (openRes == ERROR_SUCCESS) {
                        LONG delRes = RegDeleteTreeW(hParent, subKeyName.c_str());
                        RegCloseKey(hParent);

                        if (delRes != ERROR_SUCCESS && delRes != ERROR_FILE_NOT_FOUND) {
                            errors.push_back(L"Failed to delete registry key: " + item.fullPath + L" (Error: " + std::to_wstring(delRes) + L")");
                            allSuccess = false;
                        }
                    } else {
                        // Try direct deletion
                        LONG status = RegDeleteTreeW(item.regRoot, item.regSubKey.c_str());
                        if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
                            errors.push_back(L"Failed to delete registry key: " + item.fullPath + L" (Error: " + std::to_wstring(status) + L")");
                            allSuccess = false;
                        }
                    }
                }
            }
        }
    }

    return allSuccess;
}
