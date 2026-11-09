#pragma once

#include <windows.h>
#include <string>

struct ReleaseInfo {
    std::wstring tagName;
    std::wstring releaseTitle;
    std::wstring releaseNotes;
    std::wstring htmlUrl;
    std::wstring downloadUrl;
    std::wstring assetName;
    bool isUpdateAvailable = false;
    bool checkSucceeded = false;
    std::wstring errorMessage;
};

class Updater {
public:
    // Check for updates asynchronously and display the Midnight Navy dark update dialog
    static void CheckForUpdates(HWND hWndParent, bool silentIfLatest = false);

    // Metadata constants
    static const wchar_t* GetCurrentVersion();
    static const wchar_t* GetRepoUrl();
    static const wchar_t* GetReleasesUrl();
};
