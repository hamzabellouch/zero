#pragma once

#include <windows.h>
#include <string>
#include <vector>

struct InstalledApp {
    std::wstring displayName;
    std::wstring displayVersion;
    std::wstring publisher;
    std::wstring installLocation;
    std::wstring uninstallString;
    std::wstring quietUninstallString;
    std::wstring installDate;
    std::wstring displayIcon;
    uint64_t estimatedSizeBytes = 0;
    std::wstring sizeFormatted;
    int iconIndex = -1;
    HICON hIcon = nullptr;
    
    // Registry details for tracking / removal
    HKEY rootKey = nullptr;             // HKEY_LOCAL_MACHINE or HKEY_CURRENT_USER
    std::wstring subKeyName;            // The key name under Uninstall
    std::wstring fullRegistryPath;      // Path e.g. HKLM\...\Uninstall\AppName
    bool is64Bit = false;
    bool show32BitTag = false;
    bool isMsi = false;

    // Windows Store (UWP / MSIX) App details
    bool isWindowsStoreApp = false;
    std::wstring packageFullName;       // e.g. Ookla.SpeedtestbyOokla_1.27.200.0_x64__43tkc6nmykmb6
    std::wstring packageFamilyName;     // e.g. Ookla.SpeedtestbyOokla_43tkc6nmykmb6
};

class AppManager {
public:
    // Enumerates traditional desktop apps installed manually from the web / installers
    static std::vector<InstalledApp> EnumerateDesktopApps();

    // Enumerates apps installed specifically through the Microsoft Store (UWP / MSIX)
    static std::vector<InstalledApp> EnumerateStoreApps();

    // Backwards compatible alias for desktop applications
    static std::vector<InstalledApp> EnumerateInstalledApps() { return EnumerateDesktopApps(); }

    // Runs the official uninstaller for desktop applications
    static bool LaunchStandardUninstall(const InstalledApp& app, HWND hWndParent, std::wstring& errorMessage, HANDLE* phProcess = nullptr);

    // Uninstalls a Windows Store App via PowerShell Remove-AppxPackage
    static bool UninstallStoreApp(const InstalledApp& app, std::wstring& errorMessage);

    // Removes only the uninstall registry key (force remove from list)
    static bool ForceRemoveRegistryEntry(const InstalledApp& app, std::wstring& errorMessage);

    // Extracts high-DPI, crisp, non-pixelated icon matching targetSize
    static HICON ExtractAppIcon(const InstalledApp& app, int targetSize = 20);
    
    // Format byte count to string e.g. 392 MB, 4.93 GB
    static std::wstring FormatSize(uint64_t bytes);

    // Helper to calculate directory size
    static uint64_t CalculateDirectorySize(const std::wstring& folderPath);

private:
    static void ScanRegistryNode(HKEY hRoot, const std::wstring& path, REGSAM samDesired, bool is64Bit, std::vector<InstalledApp>& apps);
    static std::wstring ReadRegString(HKEY hKey, const wchar_t* valueName);
    static DWORD ReadRegDword(HKEY hKey, const wchar_t* valueName, DWORD defaultValue = 0);
};
