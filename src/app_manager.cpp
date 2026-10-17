#include "app_manager.h"
#include <filesystem>
#include <algorithm>
#include <shlwapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <sstream>
#include <fstream>
#include <set>
#include <gdiplus.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "gdiplus.lib")

namespace fs = std::filesystem;

namespace {
    std::wstring ToLower(const std::wstring& s) {
        std::wstring out = s;
        std::transform(out.begin(), out.end(), out.begin(), ::towlower);
        return out;
    }

    std::wstring FormatDate(const std::wstring& rawDate) {
        // Month names: Oct 01, 2026
        static const wchar_t* const MONTHS[] = {
            L"", L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun",
            L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"
        };
        if (rawDate.length() == 8 && iswdigit(rawDate[0]) && iswdigit(rawDate[4])) {
            try {
                int month = std::stoi(rawDate.substr(4, 2));
                if (month >= 1 && month <= 12) {
                    std::wstring day = rawDate.substr(6, 2);
                    std::wstring year = rawDate.substr(0, 4);
                    return std::wstring(MONTHS[month]) + L" " + day + L", " + year;
                }
            } catch (...) {}
        }
        return rawDate;
    }
}

uint64_t AppManager::CalculateDirectorySize(const std::wstring& folderPath) {
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

std::wstring AppManager::FormatSize(uint64_t bytes) {
    if (bytes == 0) return L"";
    const double KB = 1024.0;
    const double MB = 1024.0 * KB;
    const double GB = 1024.0 * MB;

    wchar_t buf[64];
    if (bytes >= GB) {
        swprintf_s(buf, L"%.2f GB", bytes / GB);
    } else if (bytes >= 100 * MB) {
        swprintf_s(buf, L"%llu MB", static_cast<unsigned long long>(bytes / MB));
    } else if (bytes >= MB) {
        swprintf_s(buf, L"%.1f MB", bytes / MB);
    } else {
        swprintf_s(buf, L"%llu KB", static_cast<unsigned long long>(bytes / KB));
    }
    return buf;
}

std::wstring AppManager::ReadRegString(HKEY hKey, const wchar_t* valueName) {
    DWORD type = 0;
    DWORD bytes = 0;
    
    LONG status = RegQueryValueExW(hKey, valueName, nullptr, &type, nullptr, &bytes);
    if (status != ERROR_SUCCESS || bytes == 0) {
        return L"";
    }

    if (type != REG_SZ && type != REG_EXPAND_SZ) {
        return L"";
    }

    std::vector<wchar_t> buffer((bytes / sizeof(wchar_t)) + 2, 0);
    status = RegQueryValueExW(hKey, valueName, nullptr, &type, reinterpret_cast<LPBYTE>(buffer.data()), &bytes);
    if (status != ERROR_SUCCESS) {
        return L"";
    }

    std::wstring result = buffer.data();
    if (type == REG_EXPAND_SZ) {
        DWORD expBytes = ExpandEnvironmentStringsW(result.c_str(), nullptr, 0);
        if (expBytes > 0) {
            std::vector<wchar_t> expBuf(expBytes, 0);
            ExpandEnvironmentStringsW(result.c_str(), expBuf.data(), expBytes);
            result = expBuf.data();
        }
    }
    return result;
}

DWORD AppManager::ReadRegDword(HKEY hKey, const wchar_t* valueName, DWORD defaultValue) {
    DWORD type = 0;
    DWORD value = 0;
    DWORD bytes = sizeof(value);
    LONG status = RegQueryValueExW(hKey, valueName, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &bytes);
    if (status == ERROR_SUCCESS && type == REG_DWORD) {
        return value;
    }
    return defaultValue;
}

void AppManager::ScanRegistryNode(HKEY hRoot, const std::wstring& path, REGSAM samDesired, bool is64Bit, std::vector<InstalledApp>& apps) {
    HKEY hUninstallKey = nullptr;
    LONG status = RegOpenKeyExW(hRoot, path.c_str(), 0, KEY_READ | samDesired, &hUninstallKey);
    if (status != ERROR_SUCCESS) {
        return;
    }

    DWORD index = 0;
    wchar_t subKeyName[256];
    DWORD subKeyLen = sizeof(subKeyName) / sizeof(subKeyName[0]);

    while (RegEnumKeyExW(hUninstallKey, index++, subKeyName, &subKeyLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        HKEY hAppKey = nullptr;
        if (RegOpenKeyExW(hUninstallKey, subKeyName, 0, KEY_READ | samDesired, &hAppKey) == ERROR_SUCCESS) {
            std::wstring displayName = ReadRegString(hAppKey, L"DisplayName");
            DWORD systemComponent = ReadRegDword(hAppKey, L"SystemComponent", 0);
            std::wstring parentKeyName = ReadRegString(hAppKey, L"ParentKeyName");

            // Filter out empty names, system components, and update patches
            if (!displayName.empty() && systemComponent == 0 && parentKeyName.empty()) {
                InstalledApp app;
                app.displayName = displayName;
                app.displayVersion = ReadRegString(hAppKey, L"DisplayVersion");
                app.publisher = ReadRegString(hAppKey, L"Publisher");
                app.installLocation = ReadRegString(hAppKey, L"InstallLocation");
                app.uninstallString = ReadRegString(hAppKey, L"UninstallString");
                app.quietUninstallString = ReadRegString(hAppKey, L"QuietUninstallString");
                app.installDate = FormatDate(ReadRegString(hAppKey, L"InstallDate"));
                app.displayIcon = ReadRegString(hAppKey, L"DisplayIcon");

                DWORD estKB = ReadRegDword(hAppKey, L"EstimatedSize", 0);
                if (estKB > 0) {
                    app.estimatedSizeBytes = static_cast<uint64_t>(estKB) * 1024ULL;
                    app.sizeFormatted = FormatSize(app.estimatedSizeBytes);
                }
                
                app.rootKey = hRoot;
                app.subKeyName = subKeyName;
                app.fullRegistryPath = path + L"\\" + subKeyName;
                app.is64Bit = is64Bit;
                app.isMsi = (ReadRegDword(hAppKey, L"WindowsInstaller", 0) == 1);

                // Only tag (32-bit) if strictly from 32-bit node and name doesn't already say it
                if (!is64Bit && (samDesired & KEY_WOW64_32KEY)) {
                    std::wstring lower = ToLower(app.displayName);
                    if (lower.find(L"32-bit") == std::wstring::npos && lower.find(L"x86") == std::wstring::npos) {
                        app.show32BitTag = true;
                    }
                }

                // Trim trailing backslash from installLocation
                while (!app.installLocation.empty() && (app.installLocation.back() == L'\\' || app.installLocation.back() == L'/')) {
                    app.installLocation.pop_back();
                }

                // Deduplication check
                bool alreadyAdded = false;
                std::wstring lowerName = ToLower(app.displayName);
                for (const auto& existing : apps) {
                    if (ToLower(existing.displayName) == lowerName && existing.displayVersion == app.displayVersion) {
                        alreadyAdded = true;
                        break;
                    }
                }

                if (!alreadyAdded) {
                    apps.push_back(app);
                }
            }
            RegCloseKey(hAppKey);
        }
        subKeyLen = sizeof(subKeyName) / sizeof(subKeyName[0]);
    }

    RegCloseKey(hUninstallKey);
}

std::vector<InstalledApp> AppManager::EnumerateDesktopApps() {
    std::vector<InstalledApp> apps;
    const std::wstring uninstallPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";

    // 1. HKLM 64-bit
    ScanRegistryNode(HKEY_LOCAL_MACHINE, uninstallPath, KEY_WOW64_64KEY, true, apps);

    // 2. HKLM 32-bit (WOW6432Node)
    ScanRegistryNode(HKEY_LOCAL_MACHINE, uninstallPath, KEY_WOW64_32KEY, false, apps);

    // 3. HKCU
    ScanRegistryNode(HKEY_CURRENT_USER, uninstallPath, 0, true, apps);

    // Sort alphabetically by DisplayName
    std::sort(apps.begin(), apps.end(), [](const InstalledApp& a, const InstalledApp& b) {
        return _wcsicmp(a.displayName.c_str(), b.displayName.c_str()) < 0;
    });

    return apps;
}

std::vector<InstalledApp> AppManager::EnumerateStoreApps() {
    std::vector<InstalledApp> apps;
    const std::wstring regPath = L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages";

    HKEY hPackages = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, regPath.c_str(), 0, KEY_READ, &hPackages) != ERROR_SUCCESS) {
        return apps;
    }

    DWORD index = 0;
    wchar_t subKeyName[512];
    DWORD subKeyLen = sizeof(subKeyName) / sizeof(subKeyName[0]);
    FILETIME ftLastWrite;

    std::set<std::wstring> seenPackages;

    while (RegEnumKeyExW(hPackages, index++, subKeyName, &subKeyLen, nullptr, nullptr, nullptr, &ftLastWrite) == ERROR_SUCCESS) {
        std::wstring pkgFullName = subKeyName;
        subKeyLen = sizeof(subKeyName) / sizeof(subKeyName[0]);

        // Filter out known internal frameworks & runtimes
        std::wstring lowerPkg = ToLower(pkgFullName);
        if (lowerPkg.find(L"vclibs") != std::wstring::npos ||
            lowerPkg.find(L"native.runtime") != std::wstring::npos ||
            lowerPkg.find(L"native.framework") != std::wstring::npos ||
            lowerPkg.find(L"ui.xaml") != std::wstring::npos ||
            lowerPkg.find(L"winappruntime") != std::wstring::npos ||
            lowerPkg.find(L"languageexperiencepack") != std::wstring::npos ||
            lowerPkg.find(L"directx") != std::wstring::npos) {
            continue;
        }

        HKEY hPkg = nullptr;
        if (RegOpenKeyExW(hPackages, pkgFullName.c_str(), 0, KEY_READ, &hPkg) != ERROR_SUCCESS) {
            continue;
        }

        std::wstring rootFolder = ReadRegString(hPkg, L"PackageRootFolder");
        std::wstring rawDisplayName = ReadRegString(hPkg, L"DisplayName");
        std::wstring packageId = ReadRegString(hPkg, L"PackageID");
        RegCloseKey(hPkg);

        // Ensure it is a user-installed Store App (located in C:\Program Files\WindowsApps)
        if (rootFolder.empty() || _wcsnicmp(rootFolder.c_str(), L"C:\\Program Files\\WindowsApps", 26) != 0) {
            continue;
        }

        if (seenPackages.find(lowerPkg) != seenPackages.end()) {
            continue;
        }
        seenPackages.insert(lowerPkg);

        InstalledApp app;
        app.isWindowsStoreApp = true;
        app.packageFullName = pkgFullName;
        app.installLocation = rootFolder;
        app.fullRegistryPath = regPath + L"\\" + pkgFullName;
        app.rootKey = HKEY_CURRENT_USER;
        app.subKeyName = pkgFullName;

        // Resolve PackageFamilyName from PackageFullName:
        // PackageFullName format: <Name>_<Version>_<Arch>_<ResourceID>_<PublisherId>
        // PackageFamilyName format: <Name>_<PublisherId>
        size_t firstUnderscore = pkgFullName.find(L'_');
        size_t lastUnderscore = pkgFullName.rfind(L'_');
        if (firstUnderscore != std::wstring::npos && lastUnderscore != std::wstring::npos && lastUnderscore > firstUnderscore) {
            std::wstring baseName = pkgFullName.substr(0, firstUnderscore);
            std::wstring pubId = pkgFullName.substr(lastUnderscore + 1);
            app.packageFamilyName = baseName + L"_" + pubId;
        }

        // Clean & Resolve DisplayName
        std::wstring dispName = rawDisplayName;
        if (dispName.find(L"ms-resource:") != std::wstring::npos || dispName.find(L"@{") != std::wstring::npos) {
            // Try resolving through SHLoadIndirectString
            wchar_t outBuf[512] = { 0 };
            if (SUCCEEDED(SHLoadIndirectString(dispName.c_str(), outBuf, 512, nullptr)) && outBuf[0] != L'\0' && wcsstr(outBuf, L"ms-resource:") == nullptr) {
                dispName = outBuf;
            } else {
                // Try reading AppxManifest.xml
                std::wstring manifestPath = rootFolder + L"\\AppxManifest.xml";
                std::wifstream mFile(manifestPath);
                bool foundManifestName = false;
                if (mFile.is_open()) {
                    std::wstring line;
                    while (std::getline(mFile, line)) {
                        size_t descPos = line.find(L"Description=\"");
                        if (descPos != std::wstring::npos) {
                            size_t start = descPos + 13;
                            size_t end = line.find(L'\"', start);
                            if (end != std::wstring::npos) {
                                std::wstring descVal = line.substr(start, end - start);
                                if (descVal.find(L"ms-resource:") == std::wstring::npos && descVal.length() > 1) {
                                    dispName = descVal;
                                    foundManifestName = true;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (!foundManifestName || dispName.find(L"ms-resource:") != std::wstring::npos) {
                    // Friendly formatting from Package Name
                    if (firstUnderscore != std::wstring::npos) {
                        std::wstring rawName = pkgFullName.substr(0, firstUnderscore);
                        size_t dotPos = rawName.rfind(L'.');
                        if (dotPos != std::wstring::npos && dotPos + 1 < rawName.length()) {
                            rawName = rawName.substr(dotPos + 1);
                        }
                        std::wstring formatted;
                        for (size_t c = 0; c < rawName.length(); ++c) {
                            if (c > 0 && iswupper(rawName[c]) && !iswupper(rawName[c - 1])) {
                                formatted += L' ';
                            } else if (c > 0 && iswdigit(rawName[c]) && !iswdigit(rawName[c - 1])) {
                                formatted += L' ';
                            }
                            formatted += rawName[c];
                        }
                        dispName = formatted;
                    }
                }
            }
        }

        if (dispName.empty()) {
            dispName = pkgFullName;
        }
        app.displayName = dispName;

        // Extract Publisher from package or manifest
        if (firstUnderscore != std::wstring::npos) {
            std::wstring rawName = pkgFullName.substr(0, firstUnderscore);
            size_t dotPos = rawName.find(L'.');
            if (dotPos != std::wstring::npos) {
                app.publisher = rawName.substr(0, dotPos);
            } else {
                app.publisher = L"Microsoft Store";
            }
        }

        // Install Date from Registry LastWriteTime
        SYSTEMTIME st;
        if (FileTimeToSystemTime(&ftLastWrite, &st)) {
            wchar_t dateBuf[64] = { 0 };
            GetDateFormatW(LOCALE_USER_DEFAULT, 0, &st, L"MMM dd, yyyy", dateBuf, 64);
            app.installDate = dateBuf;
        }

        // Calculate size from PackageRootFolder
        app.estimatedSizeBytes = CalculateDirectorySize(rootFolder);
        app.sizeFormatted = FormatSize(app.estimatedSizeBytes);

        apps.push_back(app);
    }

    RegCloseKey(hPackages);

    // Sort alphabetically by displayName
    std::sort(apps.begin(), apps.end(), [](const InstalledApp& a, const InstalledApp& b) {
        return _wcsicmp(a.displayName.c_str(), b.displayName.c_str()) < 0;
    });

    return apps;
}

bool AppManager::LaunchStandardUninstall(const InstalledApp& app, HWND hWndParent, std::wstring& errorMessage, HANDLE* phProcess) {
    if (app.isWindowsStoreApp) {
        return UninstallStoreApp(app, errorMessage);
    }

    std::wstring cmd = app.uninstallString;
    if (cmd.empty()) {
        cmd = app.quietUninstallString;
    }

    if (cmd.empty()) {
        errorMessage = L"Uninstall command not found for this program in the Windows registry.";
        return false;
    }

    // Expand any environment variables
    DWORD expLen = ExpandEnvironmentStringsW(cmd.c_str(), nullptr, 0);
    if (expLen > 0) {
        std::vector<wchar_t> expBuf(expLen, 0);
        ExpandEnvironmentStringsW(cmd.c_str(), expBuf.data(), expLen);
        cmd = expBuf.data();
    }

    // Smart parse executable and arguments
    std::wstring file;
    std::wstring params;

    // Check if command is wrapped in quotes: "C:\path\unins.exe" /arg
    if (cmd.front() == L'\"') {
        size_t closeQuote = cmd.find(L'\"', 1);
        if (closeQuote != std::wstring::npos) {
            file = cmd.substr(1, closeQuote - 1);
            if (closeQuote + 1 < cmd.length()) {
                params = cmd.substr(closeQuote + 1);
            }
        } else {
            file = cmd;
        }
    } else {
        // Unquoted path with spaces: find .exe position if present
        size_t exePos = ToLower(cmd).find(L".exe");
        if (exePos != std::wstring::npos) {
            file = cmd.substr(0, exePos + 4);
            if (exePos + 4 < cmd.length()) {
                params = cmd.substr(exePos + 4);
            }
        } else {
            size_t firstSpace = cmd.find(L' ');
            if (firstSpace != std::wstring::npos) {
                file = cmd.substr(0, firstSpace);
                params = cmd.substr(firstSpace + 1);
            } else {
                file = cmd;
            }
        }
    }

    // Trim leading whitespace from parameters
    size_t paramStart = params.find_first_not_of(L" \t");
    if (paramStart != std::wstring::npos) {
        params = params.substr(paramStart);
    } else {
        params.clear();
    }

    // MSI /I -> /X conversion so it launches uninstall rather than modify/repair
    if (ToLower(file).find(L"msiexec") != std::wstring::npos) {
        size_t pos = ToLower(params).find(L"/i");
        if (pos != std::wstring::npos) {
            params.replace(pos, 2, L"/X");
        }
    }

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = hWndParent;
    sei.lpVerb = L"open";
    sei.lpFile = file.c_str();
    sei.lpParameters = params.empty() ? nullptr : params.c_str();
    if (!app.installLocation.empty()) {
        sei.lpDirectory = app.installLocation.c_str();
    }
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&sei)) {
        DWORD err = GetLastError();
        wchar_t buf[256];
        swprintf_s(buf, L"Failed to start official uninstaller (Error code: %lu)", err);
        errorMessage = buf;
        return false;
    }

    if (sei.hProcess) {
        if (phProcess) {
            *phProcess = sei.hProcess;
        } else {
            CloseHandle(sei.hProcess);
        }
    }

    return true;
}

bool AppManager::UninstallStoreApp(const InstalledApp& app, std::wstring& errorMessage) {
    if (app.packageFullName.empty()) {
        errorMessage = L"Invalid Windows Store package identifier.";
        return false;
    }

    std::wstring cmd = L"powershell.exe -NoProfile -NonInteractive -WindowStyle Hidden -Command \"Remove-AppxPackage -Package '" + app.packageFullName + L"'\"";

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = { 0 };

    std::vector<wchar_t> cmdBuf(cmd.length() + 1, 0);
    wcsncpy_s(cmdBuf.data(), cmdBuf.size(), cmd.c_str(), cmd.length());

    if (!CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        errorMessage = L"Failed to launch PowerShell to uninstall Windows Store package.";
        return false;
    }

    WaitForSingleObject(pi.hProcess, 45000); // Wait up to 45 seconds for completion
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (exitCode != 0) {
        errorMessage = L"Remove-AppxPackage returned error code: " + std::to_wstring(exitCode);
        return false;
    }

    return true;
}

bool AppManager::ForceRemoveRegistryEntry(const InstalledApp& app, std::wstring& errorMessage) {
    if (app.rootKey == nullptr || app.subKeyName.empty()) {
        errorMessage = L"Invalid registry key data.";
        return false;
    }

    if (app.isWindowsStoreApp) {
        // Remove AppModel registry key
        HKEY hParent = nullptr;
        std::wstring parentPath = L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages";
        if (RegOpenKeyExW(HKEY_CURRENT_USER, parentPath.c_str(), 0, KEY_ALL_ACCESS, &hParent) == ERROR_SUCCESS) {
            RegDeleteTreeW(hParent, app.subKeyName.c_str());
            RegCloseKey(hParent);
            return true;
        }
        errorMessage = L"Failed to remove AppModel package registration key.";
        return false;
    }

    const std::wstring parentPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
    HKEY hParent = nullptr;
    REGSAM samDesired = KEY_ALL_ACCESS;
    if (app.is64Bit) {
        samDesired |= KEY_WOW64_64KEY;
    } else {
        samDesired |= KEY_WOW64_32KEY;
    }

    LONG status = RegOpenKeyExW(app.rootKey, parentPath.c_str(), 0, samDesired, &hParent);
    if (status != ERROR_SUCCESS) {
        errorMessage = L"Failed to open registry Uninstall path with write access.";
        return false;
    }

    status = RegDeleteTreeW(hParent, app.subKeyName.c_str());
    RegCloseKey(hParent);

    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
        wchar_t buf[256];
        swprintf_s(buf, L"Failed to delete registry key (Error code: %ld)", status);
        errorMessage = buf;
        return false;
    }

    return true;
}

HICON AppManager::ExtractAppIcon(const InstalledApp& app, int targetSize) {
    if (targetSize <= 0) targetSize = 20;

    HICON hResult = nullptr;

    auto TryExtractPath = [&](const std::wstring& rawPath, int iconIdx) -> bool {
        if (rawPath.empty()) return false;
        std::wstring clean = rawPath;
        if (clean.front() == L'\"') {
            size_t endQuote = clean.find(L'\"', 1);
            if (endQuote != std::wstring::npos) {
                clean = clean.substr(1, endQuote - 1);
            }
        }
        DWORD expLen = ExpandEnvironmentStringsW(clean.c_str(), nullptr, 0);
        if (expLen > 0) {
            std::vector<wchar_t> expBuf(expLen, 0);
            ExpandEnvironmentStringsW(clean.c_str(), expBuf.data(), expLen);
            clean = expBuf.data();
        }

        // 1. Try PrivateExtractIconsW (extracts exact targetSize from high-res ICO/PE resources)
        UINT iconId = 0;
        if (PrivateExtractIconsW(clean.c_str(), iconIdx, targetSize, targetSize, &hResult, &iconId, 1, LR_LOADFROMFILE) > 0 && hResult != nullptr) {
            return true;
        }

        // 2. Try SHGetFileInfoW with Large Icon downscaled smoothly
        SHFILEINFOW sfi = { 0 };
        if (SHGetFileInfoW(clean.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON) && sfi.hIcon != nullptr) {
            hResult = sfi.hIcon;
            return true;
        }

        return false;
    };

    // For Windows Store apps, check for .exe or Assets PNG logo
    if (app.isWindowsStoreApp && !app.installLocation.empty()) {
        // A. Look for .exe in installLocation
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW((app.installLocation + L"\\*.exe").c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                std::wstring exePath = app.installLocation + L"\\" + fd.cFileName;
                if (TryExtractPath(exePath, 0)) {
                    FindClose(hFind);
                    return hResult;
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }

        // B. Look for PNG logo in Assets folder using GDI+
        std::wstring assetsDir = app.installLocation + L"\\Assets";
        if (GetFileAttributesW(assetsDir.c_str()) != INVALID_FILE_ATTRIBUTES) {
            const wchar_t* logoPatterns[] = {
                L"\\*Square44x44Logo*.png",
                L"\\*Square150x150Logo*.png",
                L"\\*Logo*.png",
                L"\\*icon*.png",
                L"\\*.png"
            };
            for (const auto& pat : logoPatterns) {
                HANDLE hFindPng = FindFirstFileW((assetsDir + pat).c_str(), &fd);
                if (hFindPng != INVALID_HANDLE_VALUE) {
                    std::wstring pngPath = assetsDir + L"\\" + fd.cFileName;
                    FindClose(hFindPng);

                    Gdiplus::Bitmap bmp(pngPath.c_str());
                    if (bmp.GetLastStatus() == Gdiplus::Ok) {
                        bmp.GetHICON(&hResult);
                        if (hResult) return hResult;
                    }
                }
            }
        }
    }

    // 1. Try DisplayIcon
    if (!app.displayIcon.empty()) {
        std::wstring iconPath = app.displayIcon;
        int index = 0;
        size_t commaPos = iconPath.find_last_of(L',');
        if (commaPos != std::wstring::npos) {
            try {
                index = std::stoi(iconPath.substr(commaPos + 1));
                iconPath = iconPath.substr(0, commaPos);
            } catch (...) {}
        }
        if (TryExtractPath(iconPath, index)) {
            return hResult;
        }
    }

    // 2. Try InstallLocation
    if (!app.installLocation.empty()) {
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW((app.installLocation + L"\\*.exe").c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            std::wstring exePath = app.installLocation + L"\\" + fd.cFileName;
            FindClose(hFind);
            if (TryExtractPath(exePath, 0)) {
                return hResult;
            }
        }
    }

    // 3. Fallback to default application icon
    return LoadIconW(nullptr, IDI_APPLICATION);
}
