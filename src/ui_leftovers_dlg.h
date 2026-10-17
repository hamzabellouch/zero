#pragma once

#include <windows.h>
#include <vector>
#include "cleaner.h"
#include "app_manager.h"

class LeftoversDialog {
public:
    static bool Show(HWND hWndParent, const InstalledApp& app, std::vector<LeftoverItem>& leftovers);
};
