#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <set>
#include "app_manager.h"

enum class LeftoverType {
    Folder,
    File,
    RegistryKey,
    RegistryValue
};

struct LeftoverItem {
    LeftoverType type;
    std::wstring displayType;       // e.g. L"مجلد" or L"مفتاح ريجستري"
    std::wstring fullPath;          // Path in filesystem or Registry (e.g. HKCU\Software\XYZ)
    HKEY regRoot = nullptr;         // HKEY_CURRENT_USER or HKEY_LOCAL_MACHINE
    std::wstring regSubKey;         // Relative to root
    REGSAM regSam = 0;              // KEY_WOW64_64KEY or KEY_WOW64_32KEY
    std::wstring regValueName;      // If RegistryValue
    bool selected = true;           // Checked for deletion by default
    uint64_t sizeBytes = 0;         // Size if file or folder
};

#include <functional>
#include <atomic>

using ScanProgressCallback = std::function<void(const std::wstring& stage, const std::wstring& currentPath, int percent)>;

class Cleaner {
public:
    // Performs a comprehensive scan for leftover files, folders, and registry keys
    // with Sibling App Protection (e.g. Adobe vs Adobe Express) and Live Progress Reporting
    static std::vector<LeftoverItem> ScanLeftovers(
        const InstalledApp& app,
        const std::vector<InstalledApp>& allInstalledApps = {},
        ScanProgressCallback onProgress = nullptr,
        const std::atomic<bool>* cancelToken = nullptr
    );

    // Deletes selected leftover items safely (files sent to Recycle Bin)
    static bool DeleteSelectedItems(std::vector<LeftoverItem>& items, std::vector<std::wstring>& errors);

private:
    // Safety check: returns true if path or name is a protected Windows system component
    static bool IsSafeToScanOrDelete(const std::wstring& pathOrName);

    // Normalizes strings for comparison (lowercasing, removing special characters/versions)
    static std::wstring NormalizeForSearch(const std::wstring& input);

    // Extraction of keywords from app name and publisher
    static std::vector<std::wstring> GenerateSearchKeywords(const InstalledApp& app);

    // Extraction of distinct keywords belonging to sibling applications to prevent accidental collision
    static std::vector<std::wstring> GenerateSiblingExclusions(const InstalledApp& targetApp, 
                                                              const std::vector<InstalledApp>& allApps);

    // Checks whether a path contains any sibling app exclusion token
    static bool ContainsSiblingExclusion(const std::wstring& pathOrName, 
                                        const std::vector<std::wstring>& exclusions);

    // Scans known file system directories
    static void ScanFileSystem(const std::vector<std::wstring>& keywords, 
                               const std::vector<std::wstring>& siblingExclusions,
                               const std::wstring& installLocation,
                               std::vector<LeftoverItem>& results,
                               ScanProgressCallback onProgress = nullptr,
                               const std::atomic<bool>* cancelToken = nullptr);

    // Scans registry trees (HKCU and HKLM)
    static void ScanRegistry(const std::vector<std::wstring>& keywords,
                             const std::vector<std::wstring>& siblingExclusions,
                             const InstalledApp& app,
                             std::vector<LeftoverItem>& results,
                             ScanProgressCallback onProgress = nullptr,
                             const std::atomic<bool>* cancelToken = nullptr);

    // Helper to calculate folder size in bytes
    static uint64_t CalculateFolderSize(const std::wstring& folderPath);
};
