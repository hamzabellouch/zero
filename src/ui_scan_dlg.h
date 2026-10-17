#pragma once

#include <windows.h>
#include <vector>
#include "cleaner.h"
#include "app_manager.h"

class ScanProgressDialog {
public:
    // Shows deep scanning modal progress dialog.
    // Runs scan on background thread with live path updates and animated progress bar.
    // Returns true if scan completed normally (not cancelled), and fills leftovers.
    static bool Show(HWND hWndParent, 
                     const InstalledApp& app, 
                     const std::vector<InstalledApp>& allApps, 
                     std::vector<LeftoverItem>& leftovers);
};
