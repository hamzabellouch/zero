#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <objbase.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <gdiplus.h>
#include "resource.h"
#include "app_manager.h"
#include "cleaner.h"
#include "ui_leftovers_dlg.h"
#include "ui_scan_dlg.h"
#include "process_manager.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace {
    enum class AppViewMode {
        Desktop,
        WindowsStore,
        Processes
    };
    AppViewMode g_currentViewMode = AppViewMode::Desktop;

    // Colors matching Geek Uninstaller dark theme exactly
    const COLORREF CLR_BG_EVEN        = RGB(25, 25, 25);     // #191919
    const COLORREF CLR_BG_ODD         = RGB(33, 33, 33);     // #212121
    const COLORREF CLR_BG_SELECTED    = RGB(224, 86, 46);    // #E0562E Vivid Coral/Orange
    const COLORREF CLR_TEXT_NORMAL    = RGB(224, 224, 224);  // Light Gray
    const COLORREF CLR_TEXT_SELECTED  = RGB(255, 255, 255);  // Pure White
    const COLORREF CLR_TEXT_MUTED     = RGB(140, 140, 140);  // Dimmed Gray for secondary info
    const COLORREF CLR_HEADER_BG      = RGB(26, 26, 26);     // #1A1A1A (Exact match with CLR_BOTTOM_BG)
    const COLORREF CLR_HEADER_TEXT    = RGB(200, 200, 200);  // #C8C8C8
    const COLORREF CLR_BOTTOM_BG      = RGB(26, 26, 26);     // #1A1A1A
    const COLORREF CLR_BOTTOM_BORDER  = RGB(45, 45, 45);     // #2D2D2D
    const COLORREF CLR_EDIT_BG        = RGB(38, 38, 38);     // #262626

    HINSTANCE g_hInstance = nullptr;
    HWND g_hWndMain = nullptr;
    HWND g_hTopMenuBar = nullptr;
    HWND g_hListView = nullptr;
    HWND g_hHeader = nullptr;
    HWND g_hBottomBar = nullptr;
    HWND g_hEditSearch = nullptr;
    RECT g_rcCapsule = { 0, 0, 0, 0 };
    bool g_searchHasFocus = false;
    bool g_searchIsHovered = false;
    bool g_searchClearHovered = false;
    bool g_bottomBarTrackingMouse = false;

    constexpr int TOP_MENU_COUNT = 5;
    int g_topMenuHoverIndex = -1;
    int g_topMenuOpenIndex = -1;
    bool g_topMenuTrackingMouse = false;
    const wchar_t* g_topMenuNames[TOP_MENU_COUNT] = { L"File", L"Action", L"Processes", L"View", L"Help" };

    HFONT g_hFont = nullptr;
    HFONT g_hFontBold = nullptr;
    HFONT g_hFontHeader = nullptr;
    HFONT g_hFontStatus = nullptr;

    HBRUSH g_hBrushBottom = nullptr;
    HBRUSH g_hBrushEdit = nullptr;
    HIMAGELIST g_hImageList = nullptr;

    std::vector<InstalledApp> g_allApps;
    std::vector<InstalledApp> g_displayedApps;
    uint64_t g_totalSizeBytes = 0;

    ProcessManager g_processManager;
    std::vector<ProcessItem> g_allProcesses;
    std::vector<ProcessItem> g_displayedProcesses;
    SystemUsageStats g_sysStats;
    int g_processSortColumn = 2; // Default sort by Memory (matching mock image)
    bool g_processSortAscending = false;

    UINT GetWindowDpi(HWND hWnd) {
        typedef UINT (WINAPI *GetDpiForWindowProc)(HWND);
        static GetDpiForWindowProc pGetDpi = (GetDpiForWindowProc)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
        if (pGetDpi && hWnd) {
            UINT dpi = pGetDpi(hWnd);
            if (dpi != 0) return dpi;
        }
        HDC hdc = GetDC(hWnd);
        UINT dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(hWnd, hdc);
        return (dpi != 0) ? dpi : 96;
    }

    int ScaleDpi(int val, UINT dpi) {
        return MulDiv(val, dpi, 96);
    }

    float ScaleDpi(float val, UINT dpi) {
        return (val * (float)dpi) / 96.0f;
    }

    void RecreateFonts(HWND hWnd) {
        UINT dpi = GetWindowDpi(hWnd);

        if (g_hFont) DeleteObject(g_hFont);
        if (g_hFontBold) DeleteObject(g_hFontBold);
        if (g_hFontHeader) DeleteObject(g_hFontHeader);
        if (g_hFontStatus) DeleteObject(g_hFontStatus);

        // Font sizes scaled 10% smaller for compact, sleek appearance matching Geek Uninstaller
        int fontSize = MulDiv(-15, dpi, 96);
        int headerSize = MulDiv(-14, dpi, 96);
        int statusSize = MulDiv(-13, dpi, 96);

        g_hFont = CreateFontW(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        g_hFontBold = CreateFontW(fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        g_hFontHeader = CreateFontW(headerSize, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        g_hFontStatus = CreateFontW(statusSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        if (g_hListView) SendMessage(g_hListView, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        if (g_hEditSearch) SendMessage(g_hEditSearch, WM_SETFONT, (WPARAM)g_hFontStatus, TRUE);
    }

    std::wstring ToLower(const std::wstring& str) {
        std::wstring out = str;
        std::transform(out.begin(), out.end(), out.begin(), ::towlower);
        return out;
    }

    std::wstring FormatTotalSize(uint64_t bytes) {
        const double GB = 1024.0 * 1024.0 * 1024.0;
        const double MB = 1024.0 * 1024.0;
        wchar_t buf[64];
        if (bytes >= GB) {
            swprintf_s(buf, L"%.1f GB", bytes / GB);
        } else {
            swprintf_s(buf, L"%.1f MB", bytes / MB);
        }
        return buf;
    }

    const InstalledApp* GetSelectedApp() {
        int sel = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
        if (sel >= 0 && sel < static_cast<int>(g_displayedApps.size())) {
            return &g_displayedApps[sel];
        }
        return nullptr;
    }

    const ProcessItem* GetSelectedProcess() {
        int sel = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
        if (sel >= 0 && sel < static_cast<int>(g_displayedProcesses.size())) {
            return &g_displayedProcesses[sel];
        }
        return nullptr;
    }

    void UpdateBottomBarText() {
        if (!g_hBottomBar) return;
        InvalidateRect(g_hBottomBar, nullptr, TRUE);
    }

    void PopulateMainListView() {
        if (!g_hListView) return;

        SendMessage(g_hListView, WM_SETREDRAW, FALSE, 0);
        ListView_DeleteAllItems(g_hListView);

        LVITEMW lvi = { 0 };
        lvi.mask = LVIF_TEXT | LVIF_IMAGE;

        if (g_currentViewMode == AppViewMode::Processes) {
            for (size_t i = 0; i < g_displayedProcesses.size(); ++i) {
                const auto& proc = g_displayedProcesses[i];

                lvi.iItem = static_cast<int>(i);
                lvi.iSubItem = 0;
                lvi.iImage = -1;
                lvi.pszText = const_cast<LPWSTR>(proc.name.c_str());
                ListView_InsertItem(g_hListView, &lvi);

                ListView_SetItemText(g_hListView, static_cast<int>(i), 1, const_cast<LPWSTR>(proc.cpuText.c_str()));
                ListView_SetItemText(g_hListView, static_cast<int>(i), 2, const_cast<LPWSTR>(proc.memoryText.c_str()));
                ListView_SetItemText(g_hListView, static_cast<int>(i), 3, const_cast<LPWSTR>(proc.diskText.c_str()));
                ListView_SetItemText(g_hListView, static_cast<int>(i), 4, const_cast<LPWSTR>(proc.networkText.c_str()));
            }
        } else {
            for (size_t i = 0; i < g_displayedApps.size(); ++i) {
                const auto& app = g_displayedApps[i];

                lvi.iItem = static_cast<int>(i);
                lvi.iSubItem = 0;
                lvi.iImage = app.iconIndex;
                lvi.pszText = const_cast<LPWSTR>(app.displayName.c_str());
                ListView_InsertItem(g_hListView, &lvi);

                ListView_SetItemText(g_hListView, static_cast<int>(i), 1, const_cast<LPWSTR>(app.sizeFormatted.c_str()));
                ListView_SetItemText(g_hListView, static_cast<int>(i), 2, const_cast<LPWSTR>(app.installDate.c_str()));
            }
        }

        SendMessage(g_hListView, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(g_hListView, nullptr, TRUE);
        UpdateBottomBarText();
    }

    void SortProcesses() {
        std::sort(g_displayedProcesses.begin(), g_displayedProcesses.end(), [](const ProcessItem& a, const ProcessItem& b) {
            if (g_processSortColumn == 0) { // Name
                int cmp = _wcsicmp(a.name.c_str(), b.name.c_str());
                return g_processSortAscending ? (cmp < 0) : (cmp > 0);
            }
            if (g_processSortColumn == 1) { // CPU
                if (fabs(a.cpuPercent - b.cpuPercent) > 0.01) {
                    return g_processSortAscending ? (a.cpuPercent < b.cpuPercent) : (a.cpuPercent > b.cpuPercent);
                }
                return a.memoryBytes > b.memoryBytes;
            }
            if (g_processSortColumn == 2) { // Memory
                if (a.memoryBytes != b.memoryBytes) {
                    return g_processSortAscending ? (a.memoryBytes < b.memoryBytes) : (a.memoryBytes > b.memoryBytes);
                }
                return a.cpuPercent > b.cpuPercent;
            }
            if (g_processSortColumn == 3) { // Disk
                if (fabs(a.diskBytesPerSec - b.diskBytesPerSec) > 1.0) {
                    return g_processSortAscending ? (a.diskBytesPerSec < b.diskBytesPerSec) : (a.diskBytesPerSec > b.diskBytesPerSec);
                }
                return a.memoryBytes > b.memoryBytes;
            }
            if (g_processSortColumn == 4) { // Network
                if (fabs(a.netBytesPerSec - b.netBytesPerSec) > 1.0) {
                    return g_processSortAscending ? (a.netBytesPerSec < b.netBytesPerSec) : (a.netBytesPerSec > b.netBytesPerSec);
                }
                return a.memoryBytes > b.memoryBytes;
            }
            return false;
        });
    }

    void FilterProcesses() {
        wchar_t searchBuf[256] = { 0 };
        GetWindowTextW(g_hEditSearch, searchBuf, sizeof(searchBuf) / sizeof(searchBuf[0]));
        std::wstring query = ToLower(searchBuf);

        g_displayedProcesses.clear();
        if (query.empty()) {
            g_displayedProcesses = g_allProcesses;
        } else {
            for (const auto& proc : g_allProcesses) {
                if (ToLower(proc.name).find(query) != std::wstring::npos ||
                    ToLower(proc.exeName).find(query) != std::wstring::npos) {
                    g_displayedProcesses.push_back(proc);
                }
            }
        }
        SortProcesses();
        PopulateMainListView();
    }

    void ReloadProcesses() {
        HCURSOR hOldCursor = SetCursor(LoadCursor(nullptr, IDC_WAIT));
        g_processManager.UpdateSnapshot();
        g_allProcesses = g_processManager.GetProcesses();
        g_sysStats = g_processManager.GetSystemStats();
        SetCursor(hOldCursor);

        FilterProcesses();
        if (g_hHeader) InvalidateRect(g_hHeader, nullptr, FALSE);
    }

    void UpdateProcessesInPlace() {
        if (!g_hListView || g_currentViewMode != AppViewMode::Processes) return;

        std::wstring selectedName;
        const ProcessItem* pSel = GetSelectedProcess();
        if (pSel) selectedName = pSel->name;

        g_processManager.UpdateSnapshot();
        g_allProcesses = g_processManager.GetProcesses();
        g_sysStats = g_processManager.GetSystemStats();

        wchar_t searchBuf[256] = { 0 };
        GetWindowTextW(g_hEditSearch, searchBuf, sizeof(searchBuf) / sizeof(searchBuf[0]));
        std::wstring query = ToLower(searchBuf);

        g_displayedProcesses.clear();
        if (query.empty()) {
            g_displayedProcesses = g_allProcesses;
        } else {
            for (const auto& proc : g_allProcesses) {
                if (ToLower(proc.name).find(query) != std::wstring::npos ||
                    ToLower(proc.exeName).find(query) != std::wstring::npos) {
                    g_displayedProcesses.push_back(proc);
                }
            }
        }
        SortProcesses();

        int currentCount = ListView_GetItemCount(g_hListView);
        if (currentCount != static_cast<int>(g_displayedProcesses.size())) {
            PopulateMainListView();
            if (!selectedName.empty()) {
                for (size_t i = 0; i < g_displayedProcesses.size(); ++i) {
                    if (g_displayedProcesses[i].name == selectedName) {
                        ListView_SetItemState(g_hListView, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                        break;
                    }
                }
            }
        } else {
            for (size_t i = 0; i < g_displayedProcesses.size(); ++i) {
                const auto& proc = g_displayedProcesses[i];
                ListView_SetItemText(g_hListView, static_cast<int>(i), 0, const_cast<LPWSTR>(proc.name.c_str()));
                ListView_SetItemText(g_hListView, static_cast<int>(i), 1, const_cast<LPWSTR>(proc.cpuText.c_str()));
                ListView_SetItemText(g_hListView, static_cast<int>(i), 2, const_cast<LPWSTR>(proc.memoryText.c_str()));
                ListView_SetItemText(g_hListView, static_cast<int>(i), 3, const_cast<LPWSTR>(proc.diskText.c_str()));
                ListView_SetItemText(g_hListView, static_cast<int>(i), 4, const_cast<LPWSTR>(proc.networkText.c_str()));
            }
            InvalidateRect(g_hListView, nullptr, FALSE);
        }

        if (g_hHeader) InvalidateRect(g_hHeader, nullptr, FALSE);
        UpdateBottomBarText();
    }

    void SetupListViewColumns(AppViewMode mode);
    void SwitchToViewMode(AppViewMode newMode);
    void LayoutWindow(int width, int height);

    void PerformEndTask() {
        const ProcessItem* pProc = GetSelectedProcess();
        if (!pProc) return;

        std::wstring msg = L"Are you sure you want to end process '" + pProc->name + L"'";
        if (pProc->instanceCount > 1) {
            msg += L" (" + std::to_wstring(pProc->instanceCount) + L" instances)";
        }
        msg += L"?";

        if (MessageBoxW(g_hWndMain, msg.c_str(), L"End Task - Zero", MB_YESNO | MB_ICONWARNING) == IDYES) {
            g_processManager.TerminateProcessGroup(*pProc);
            ReloadProcesses();
        }
    }

    void OpenProcessFileLocation() {
        const ProcessItem* pProc = GetSelectedProcess();
        if (!pProc || pProc->fullPath.empty()) return;

        std::wstring param = L"/select,\"" + pProc->fullPath + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOW);
    }

    void ShowProcessProperties() {
        const ProcessItem* pProc = GetSelectedProcess();
        if (!pProc || pProc->fullPath.empty()) return;

        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_INVOKEIDLIST;
        sei.lpVerb = L"properties";
        sei.lpFile = pProc->fullPath.c_str();
        sei.nShow = SW_SHOW;
        ShellExecuteExW(&sei);
    }

    void SearchProcessOnline() {
        const ProcessItem* pProc = GetSelectedProcess();
        if (!pProc) return;

        std::wstring query = L"https://www.google.com/search?q=" + pProc->exeName + L"+process";
        ShellExecuteW(nullptr, L"open", query.c_str(), nullptr, nullptr, SW_SHOW);
    }

    void FilterApplications() {
        wchar_t searchBuf[256] = { 0 };
        GetWindowTextW(g_hEditSearch, searchBuf, sizeof(searchBuf) / sizeof(searchBuf[0]));
        std::wstring query = ToLower(searchBuf);

        g_displayedApps.clear();
        if (query.empty()) {
            g_displayedApps = g_allApps;
        } else {
            for (const auto& app : g_allApps) {
                if (ToLower(app.displayName).find(query) != std::wstring::npos ||
                    ToLower(app.publisher).find(query) != std::wstring::npos) {
                    g_displayedApps.push_back(app);
                }
            }
        }
        PopulateMainListView();
    }

    void ReloadApplications() {
        HCURSOR hOldCursor = SetCursor(LoadCursor(nullptr, IDC_WAIT));
        
        UINT dpi = GetWindowDpi(g_hWndMain);
        int iconSize = ScaleDpi(18, dpi);    // Crisp high-resolution icon (10% smaller)
        int rowHeight = ScaleDpi(32, dpi);   // 10% smaller row height (sleek & compact)

        // Free previous icons to prevent GDI handle leak
        for (auto& app : g_allApps) {
            if (app.hIcon) {
                DestroyIcon(app.hIcon);
                app.hIcon = nullptr;
            }
        }

        // Clear previous icon list
        if (g_hImageList) {
            ImageList_Destroy(g_hImageList);
        }
        g_hImageList = ImageList_Create(iconSize, rowHeight, ILC_COLOR32 | ILC_MASK, 32, 32);

        if (g_currentViewMode == AppViewMode::Desktop) {
            g_allApps = AppManager::EnumerateDesktopApps();
        } else {
            g_allApps = AppManager::EnumerateStoreApps();
        }

        g_totalSizeBytes = 0;

        for (auto& app : g_allApps) {
            app.hIcon = AppManager::ExtractAppIcon(app, iconSize);
            if (app.hIcon) {
                app.iconIndex = ImageList_AddIcon(g_hImageList, app.hIcon);
            } else {
                app.iconIndex = -1;
            }
            g_totalSizeBytes += app.estimatedSizeBytes;
        }

        ListView_SetImageList(g_hListView, g_hImageList, LVSIL_SMALL);

        SetCursor(hOldCursor);
        FilterApplications();
    }

    void SetupListViewColumns(AppViewMode mode) {
        if (!g_hListView) return;

        while (ListView_DeleteColumn(g_hListView, 0)) {}

        UINT dpi = GetWindowDpi(g_hWndMain);
        RECT rcClient;
        GetClientRect(g_hListView, &rcClient);
        int width = rcClient.right;
        if (width <= 0) width = 800;

        LVCOLUMNW col = { 0 };
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;

        if (mode == AppViewMode::Processes) {
            int col1Width = ScaleDpi(90, dpi);  // CPU
            int col2Width = ScaleDpi(130, dpi); // Memory
            int col3Width = ScaleDpi(110, dpi); // Disk
            int col4Width = ScaleDpi(110, dpi); // Network
            int col0Width = width - col1Width - col2Width - col3Width - col4Width - GetSystemMetrics(SM_CXVSCROLL) - 2;
            if (col0Width < 220) col0Width = 220;

            col.fmt = LVCFMT_LEFT;
            col.cx = col0Width;
            col.pszText = const_cast<LPWSTR>(L"Name");
            ListView_InsertColumn(g_hListView, 0, &col);

            col.fmt = LVCFMT_RIGHT;
            col.cx = col1Width;
            col.pszText = const_cast<LPWSTR>(L"CPU");
            ListView_InsertColumn(g_hListView, 1, &col);

            col.fmt = LVCFMT_RIGHT;
            col.cx = col2Width;
            col.pszText = const_cast<LPWSTR>(L"Memory");
            ListView_InsertColumn(g_hListView, 2, &col);

            col.fmt = LVCFMT_RIGHT;
            col.cx = col3Width;
            col.pszText = const_cast<LPWSTR>(L"Disk");
            ListView_InsertColumn(g_hListView, 3, &col);

            col.fmt = LVCFMT_RIGHT;
            col.cx = col4Width;
            col.pszText = const_cast<LPWSTR>(L"Network");
            ListView_InsertColumn(g_hListView, 4, &col);
        } else {
            int col1Width = ScaleDpi(130, dpi); // Size
            int col2Width = ScaleDpi(160, dpi); // Installed On
            int col0Width = width - col1Width - col2Width - GetSystemMetrics(SM_CXVSCROLL) - 2;
            if (col0Width < 250) col0Width = 250;

            col.fmt = LVCFMT_LEFT;
            col.cx = col0Width;
            col.pszText = const_cast<LPWSTR>(L"Name");
            ListView_InsertColumn(g_hListView, 0, &col);

            col.fmt = LVCFMT_RIGHT;
            col.cx = col1Width;
            col.pszText = const_cast<LPWSTR>(L"Size");
            ListView_InsertColumn(g_hListView, 1, &col);

            col.fmt = LVCFMT_RIGHT;
            col.cx = col2Width;
            col.pszText = const_cast<LPWSTR>(L"Installed On");
            ListView_InsertColumn(g_hListView, 2, &col);
        }
    }

    void SwitchToViewMode(AppViewMode newMode) {
        if (g_currentViewMode == newMode) return;

        g_currentViewMode = newMode;
        if (g_hEditSearch) {
            SetWindowTextW(g_hEditSearch, L"");
        }

        SetupListViewColumns(newMode);

        if (newMode == AppViewMode::Processes) {
            if (g_hEditSearch) {
                SendMessage(g_hEditSearch, EM_SETCUEBANNER, TRUE, (LPARAM)L"Type to find a process");
            }
            SetTimer(g_hWndMain, IDT_PROCESS_REFRESH_TIMER, 1500, nullptr);
            ReloadProcesses();
        } else {
            KillTimer(g_hWndMain, IDT_PROCESS_REFRESH_TIMER);
            if (g_hEditSearch) {
                SendMessage(g_hEditSearch, EM_SETCUEBANNER, TRUE, (LPARAM)L"Type to find a program");
            }
            ReloadApplications();
        }

        if (g_hWndMain) {
            RECT rc;
            GetClientRect(g_hWndMain, &rc);
            LayoutWindow(rc.right, rc.bottom);
        }

        if (g_hHeader) {
            SetWindowPos(g_hHeader, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            InvalidateRect(g_hHeader, nullptr, TRUE);
        }
        if (g_hTopMenuBar) InvalidateRect(g_hTopMenuBar, nullptr, FALSE);
        if (g_hBottomBar) InvalidateRect(g_hBottomBar, nullptr, FALSE);
    }

    void PerformStandardUninstall() {
        const InstalledApp* pApp = GetSelectedApp();
        if (!pApp) return;

        if (pApp->isWindowsStoreApp) {
            std::wstring prompt = L"Do you want to uninstall the Windows Store App:\n" + pApp->displayName + L"?";
            if (MessageBoxW(g_hWndMain, prompt.c_str(), L"Uninstall Store App", MB_YESNO | MB_ICONQUESTION) != IDYES) {
                return;
            }

            HCURSOR hOldCursor = SetCursor(LoadCursor(nullptr, IDC_WAIT));
            std::wstring err;
            bool ok = AppManager::UninstallStoreApp(*pApp, err);
            SetCursor(hOldCursor);

            if (!ok) {
                MessageBoxW(g_hWndMain, err.c_str(), L"Error", MB_OK | MB_ICONERROR);
                return;
            }

            // Ask for leftovers scan after Store app uninstall
            std::wstring askClean = L"Store App uninstalled.\n\nWould you like to perform a Deep Scan for leftover cache files and registry keys?";
            if (MessageBoxW(g_hWndMain, askClean.c_str(), L"Scan Leftovers", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                std::vector<LeftoverItem> leftovers;
                bool completed = ScanProgressDialog::Show(g_hWndMain, *pApp, g_allApps, leftovers);
                if (completed) {
                    if (LeftoversDialog::Show(g_hWndMain, *pApp, leftovers)) {
                        ReloadApplications();
                    }
                }
            } else {
                ReloadApplications();
            }
            return;
        }

        std::wstring prompt = L"Do you want to run standard uninstaller for:\n" + pApp->displayName + L"?";
        if (MessageBoxW(g_hWndMain, prompt.c_str(), L"Standard Uninstall", MB_YESNO | MB_ICONQUESTION) != IDYES) {
            return;
        }

        std::wstring err;
        HANDLE hProcess = nullptr;
        if (!AppManager::LaunchStandardUninstall(*pApp, g_hWndMain, err, &hProcess)) {
            MessageBoxW(g_hWndMain, err.c_str(), L"Error", MB_OK | MB_ICONERROR);
            return;
        }

        // Wait for uninstaller process to complete cleanly before scanning leftovers
        if (hProcess) {
            EnableWindow(g_hWndMain, FALSE);
            MSG waitMsg;
            while (MsgWaitForMultipleObjects(1, &hProcess, FALSE, INFINITE, QS_ALLINPUT) != WAIT_OBJECT_0) {
                while (PeekMessageW(&waitMsg, nullptr, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&waitMsg);
                    DispatchMessageW(&waitMsg);
                }
            }
            CloseHandle(hProcess);
            EnableWindow(g_hWndMain, TRUE);
            SetActiveWindow(g_hWndMain);
        }

        // Ask for leftovers scan after uninstaller has finished
        std::wstring askClean = L"Uninstaller finished.\n\nWould you like to perform a Deep Scan for leftover files and registry keys?";
        if (MessageBoxW(g_hWndMain, askClean.c_str(), L"Scan Leftovers", MB_YESNO | MB_ICONQUESTION) == IDYES) {
            std::vector<LeftoverItem> leftovers;
            bool completed = ScanProgressDialog::Show(g_hWndMain, *pApp, g_allApps, leftovers);
            if (completed) {
                if (LeftoversDialog::Show(g_hWndMain, *pApp, leftovers)) {
                    ReloadApplications();
                }
            }
        }
    }

    void PerformForceRemoval() {
        const InstalledApp* pApp = GetSelectedApp();
        if (!pApp) return;

        std::wstring confirmMsg;
        if (pApp->isWindowsStoreApp) {
            confirmMsg = L"Are you sure you want to perform Force Removal of Windows Store App:\n" + pApp->displayName +
                L"\n\nZero will deeply scan and remove all leftover packages, local data, and registry entries.";
        } else {
            confirmMsg = L"Are you sure you want to perform Force Removal of:\n" + pApp->displayName + 
                L"\n\nZero will deeply scan and remove all leftover files, folders, and registry entries.";
        }

        if (MessageBoxW(g_hWndMain, confirmMsg.c_str(), L"Force Removal", MB_YESNO | MB_ICONWARNING) != IDYES) {
            return;
        }

        std::vector<LeftoverItem> leftovers;
        bool completed = ScanProgressDialog::Show(g_hWndMain, *pApp, g_allApps, leftovers);
        if (!completed) {
            return; // User cancelled the scan
        }

        if (LeftoversDialog::Show(g_hWndMain, *pApp, leftovers)) {
            ReloadApplications();
        }
    }

    void PerformRemoveEntry() {
        const InstalledApp* pApp = GetSelectedApp();
        if (!pApp) return;

        std::wstring msg = L"Are you sure you want to remove the registry entry for:\n" + pApp->displayName + 
            L"\n(This will only remove it from the list without uninstalling files)?";
        if (MessageBoxW(g_hWndMain, msg.c_str(), L"Remove Entry", MB_YESNO | MB_ICONWARNING) != IDYES) {
            return;
        }

        std::wstring err;
        if (AppManager::ForceRemoveRegistryEntry(*pApp, err)) {
            ReloadApplications();
        } else {
            MessageBoxW(g_hWndMain, err.c_str(), L"Error", MB_OK | MB_ICONERROR);
        }
    }

    void OpenInstallLocation() {
        const InstalledApp* pApp = GetSelectedApp();
        if (!pApp || pApp->installLocation.empty()) return;
        ShellExecuteW(nullptr, L"open", pApp->installLocation.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    void OpenRegistryKey() {
        const InstalledApp* pApp = GetSelectedApp();
        if (!pApp || pApp->fullRegistryPath.empty()) return;

        std::wstring rootStr = (pApp->rootKey == HKEY_LOCAL_MACHINE) ? L"HKEY_LOCAL_MACHINE" : L"HKEY_CURRENT_USER";
        std::wstring fullKey = rootStr + L"\\" + pApp->fullRegistryPath;

        // Set LastKey in Regedit so it opens at this key
        HKEY hRegKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit", 0, KEY_SET_VALUE, &hRegKey) == ERROR_SUCCESS) {
            RegSetValueExW(hRegKey, L"LastKey", 0, REG_SZ, reinterpret_cast<const BYTE*>(fullKey.c_str()), static_cast<DWORD>((fullKey.length() + 1) * sizeof(wchar_t)));
            RegCloseKey(hRegKey);
        }

        ShellExecuteW(nullptr, L"open", L"regedit.exe", nullptr, nullptr, SW_SHOWNORMAL);
    }

    void SearchGoogle() {
        const InstalledApp* pApp = GetSelectedApp();
        if (!pApp) return;
        std::wstring url = L"https://www.google.com/search?q=" + pApp->displayName;
        ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    void ExportToHTML() {
        wchar_t szFile[MAX_PATH] = L"Installed_Programs.html";
        OPENFILENAMEW ofn = { sizeof(ofn) };
        ofn.hwndOwner = g_hWndMain;
        ofn.lpstrFilter = L"HTML Files (*.html)\0*.html\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        ofn.lpstrDefExt = L"html";

        if (GetSaveFileNameW(&ofn)) {
            std::wofstream out(szFile);
            if (out.is_open()) {
                out << L"<!DOCTYPE html><html><head><meta charset='utf-8'><title>Installed Programs</title>"
                    << L"<style>body{font-family:Segoe UI,sans-serif;background:#1e1e1e;color:#ddd;padding:20px;}"
                    << L"table{border-collapse:collapse;width:100%;}th,td{padding:10px 14px;border:1px solid #333;text-align:left;font-size:14px;}"
                    << L"th{background:#2a2a2a;}tr:nth-child(even){background:#252525;}</style></head><body>"
                    << L"<h2>Installed Programs (" << g_allApps.size() << L")</h2><table>"
                    << L"<tr><th>Program Name</th><th>Size</th><th>Installed On</th><th>Publisher</th><th>Location</th></tr>";
                for (const auto& a : g_allApps) {
                    out << L"<tr><td>" << a.displayName << L"</td><td>" << a.sizeFormatted << L"</td><td>"
                        << a.installDate << L"</td><td>" << a.publisher << L"</td><td>" << a.installLocation << L"</td></tr>";
                }
                out << L"</table></body></html>";
                MessageBoxW(g_hWndMain, L"HTML report successfully exported!", L"Export", MB_OK | MB_ICONINFORMATION);
            }
        }
    }

    void ExportToCSV() {
        wchar_t szFile[MAX_PATH] = L"Installed_Programs.csv";
        OPENFILENAMEW ofn = { sizeof(ofn) };
        ofn.hwndOwner = g_hWndMain;
        ofn.lpstrFilter = L"CSV Files (*.csv)\0*.csv\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        ofn.lpstrDefExt = L"csv";

        if (GetSaveFileNameW(&ofn)) {
            std::wofstream out(szFile);
            if (out.is_open()) {
                out << L"Program Name,Size,Installed On,Publisher,Location\n";
                for (const auto& a : g_allApps) {
                    out << L"\"" << a.displayName << L"\",\"" << a.sizeFormatted << L"\",\""
                        << a.installDate << L"\",\"" << a.publisher << L"\",\"" << a.installLocation << L"\"\n";
                }
                MessageBoxW(g_hWndMain, L"CSV report successfully exported!", L"Export", MB_OK | MB_ICONINFORMATION);
            }
        }
    }

    void ShowContextMenu(HWND hWnd, int x, int y) {
        if (g_currentViewMode == AppViewMode::Processes) {
            const ProcessItem* pProc = GetSelectedProcess();
            if (!pProc) return;

            HMENU hMenu = CreatePopupMenu();
            InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, IDM_PROCESS_END_TASK, L"&End Task\tDel");
            InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
            UINT pathFlag = pProc->fullPath.empty() ? (MF_BYPOSITION | MF_STRING | MF_GRAYED) : (MF_BYPOSITION | MF_STRING);
            InsertMenuW(hMenu, 2, pathFlag, IDM_PROCESS_OPEN_FOLDER, L"&Open File Location");
            InsertMenuW(hMenu, 3, pathFlag, IDM_PROCESS_PROPERTIES, L"&Properties");
            InsertMenuW(hMenu, 4, MF_BYPOSITION | MF_STRING, IDM_PROCESS_SEARCH_ONLINE, L"&Search Online");
            InsertMenuW(hMenu, 5, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
            InsertMenuW(hMenu, 6, MF_BYPOSITION | MF_STRING, IDM_VIEW_REFRESH, L"&Refresh\tCtrl+R");

            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_TOPALIGN, x, y, 0, hWnd, nullptr);
            DestroyMenu(hMenu);
            return;
        }

        const InstalledApp* pApp = GetSelectedApp();
        if (!pApp) return;

        HMENU hMenu = CreatePopupMenu();
        InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, IDM_ACTION_UNINSTALL, L"&Uninstall...\tEnter");
        InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_STRING, IDM_ACTION_FORCE_REMOVAL, L"&Force Removal...\tDel");
        InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_STRING, IDM_ACTION_REMOVE_ENTRY, L"&Remove Entry");
        InsertMenuW(hMenu, 3, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
        InsertMenuW(hMenu, 4, MF_BYPOSITION | MF_STRING, IDM_ACTION_OPEN_REGISTRY, L"&Registry Entry");
        
        UINT folderFlag = MF_BYPOSITION | MF_STRING;
        if (pApp->installLocation.empty()) folderFlag |= MF_GRAYED;
        InsertMenuW(hMenu, 5, folderFlag, IDM_ACTION_OPEN_FOLDER, L"&Installation Folder");
        InsertMenuW(hMenu, 6, MF_BYPOSITION | MF_STRING, IDM_ACTION_SEARCH_GOOGLE, L"&Google for...");

        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_TOPALIGN, x, y, 0, hWnd, nullptr);
        DestroyMenu(hMenu);
    }

    RECT GetTopMenuItemRect(HWND hWnd, int index) {
        RECT rcItem = { 0, 0, 0, 0 };
        if (index < 0 || index >= TOP_MENU_COUNT) return rcItem;

        UINT dpi = GetWindowDpi(g_hWndMain ? g_hWndMain : hWnd);
        HDC hdc = GetDC(hWnd);
        HFONT hOldFont = (HFONT)SelectObject(hdc, g_hFontStatus ? g_hFontStatus : (HFONT)GetStockObject(DEFAULT_GUI_FONT));

        int curX = ScaleDpi(6, dpi);
        int padX = ScaleDpi(12, dpi);
        int barHeight = ScaleDpi(30, dpi);

        RECT rcClient;
        GetClientRect(hWnd, &rcClient);
        if (rcClient.bottom > 0) barHeight = rcClient.bottom;

        for (int i = 0; i <= index; ++i) {
            SIZE sz = { 0 };
            GetTextExtentPoint32W(hdc, g_topMenuNames[i], static_cast<int>(wcslen(g_topMenuNames[i])), &sz);
            int itemW = sz.cx + padX * 2;
            if (i == index) {
                rcItem.left = curX;
                rcItem.right = curX + itemW;
                rcItem.top = ScaleDpi(2, dpi);
                rcItem.bottom = barHeight - ScaleDpi(2, dpi);
                break;
            }
            curX += itemW + ScaleDpi(2, dpi);
        }

        SelectObject(hdc, hOldFont);
        ReleaseDC(hWnd, hdc);
        return rcItem;
    }

    int HitTestTopMenu(HWND hWnd, int x, int y) {
        for (int i = 0; i < TOP_MENU_COUNT; ++i) {
            RECT rc = GetTopMenuItemRect(hWnd, i);
            POINT pt = { x, y };
            if (PtInRect(&rc, pt)) {
                return i;
            }
        }
        return -1;
    }

    HMENU CreateSubMenuForIndex(int index) {
        switch (index) {
        case 0: { // File
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, IDM_FILE_EXPORT_HTML, L"Export to &HTML...\tCtrl+S");
            AppendMenuW(hMenu, MF_STRING, IDM_FILE_EXPORT_CSV, L"Export to &CSV...");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, IDM_FILE_EXIT, L"E&xit\tAlt+F4");
            return hMenu;
        }
        case 1: { // Action
            HMENU hMenu = CreatePopupMenu();
            if (g_currentViewMode == AppViewMode::Processes) {
                const ProcessItem* pProc = GetSelectedProcess();
                UINT itemFlag = pProc ? MF_STRING : (MF_STRING | MF_GRAYED);
                UINT pathFlag = (pProc && !pProc->fullPath.empty()) ? MF_STRING : (MF_STRING | MF_GRAYED);

                AppendMenuW(hMenu, itemFlag, IDM_PROCESS_END_TASK, L"&End Task\tDel");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, pathFlag, IDM_PROCESS_OPEN_FOLDER, L"&Open File Location");
                AppendMenuW(hMenu, pathFlag, IDM_PROCESS_PROPERTIES, L"&Properties");
                AppendMenuW(hMenu, itemFlag, IDM_PROCESS_SEARCH_ONLINE, L"&Search Online");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, IDM_VIEW_REFRESH, L"&Refresh\tCtrl+R");
            } else {
                const InstalledApp* pApp = GetSelectedApp();
                UINT itemFlag = pApp ? MF_STRING : (MF_STRING | MF_GRAYED);
                UINT folderFlag = (pApp && !pApp->installLocation.empty()) ? MF_STRING : (MF_STRING | MF_GRAYED);

                AppendMenuW(hMenu, itemFlag, IDM_ACTION_UNINSTALL, L"&Uninstall...\tEnter");
                AppendMenuW(hMenu, itemFlag, IDM_ACTION_FORCE_REMOVAL, L"&Force Removal...\tDel");
                AppendMenuW(hMenu, itemFlag, IDM_ACTION_REMOVE_ENTRY, L"&Remove Entry");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, itemFlag, IDM_ACTION_OPEN_REGISTRY, L"&Registry Entry");
                AppendMenuW(hMenu, folderFlag, IDM_ACTION_OPEN_FOLDER, L"&Installation Folder");
                AppendMenuW(hMenu, itemFlag, IDM_ACTION_SEARCH_GOOGLE, L"&Google for...");
            }
            return hMenu;
        }
        case 2: { // Processes
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING | (g_currentViewMode == AppViewMode::Processes ? MF_CHECKED : MF_UNCHECKED),
                        IDM_VIEW_PROCESSES, L"&Running Processes\tCtrl+3");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            const ProcessItem* pProc = GetSelectedProcess();
            UINT itemFlag = pProc ? MF_STRING : (MF_STRING | MF_GRAYED);
            UINT pathFlag = (pProc && !pProc->fullPath.empty()) ? MF_STRING : (MF_STRING | MF_GRAYED);
            AppendMenuW(hMenu, itemFlag, IDM_PROCESS_END_TASK, L"&End Task\tDel");
            AppendMenuW(hMenu, pathFlag, IDM_PROCESS_OPEN_FOLDER, L"&Open File Location");
            AppendMenuW(hMenu, pathFlag, IDM_PROCESS_PROPERTIES, L"&Properties");
            AppendMenuW(hMenu, itemFlag, IDM_PROCESS_SEARCH_ONLINE, L"&Search Online");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, IDM_VIEW_REFRESH, L"&Refresh\tCtrl+R");
            return hMenu;
        }
        case 3: { // View
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING | (g_currentViewMode == AppViewMode::Desktop ? MF_CHECKED : MF_UNCHECKED),
                        IDM_VIEW_DESKTOP_APPS, L"&Desktop Apps\tCtrl+1");
            AppendMenuW(hMenu, MF_STRING | (g_currentViewMode == AppViewMode::WindowsStore ? MF_CHECKED : MF_UNCHECKED),
                        IDM_VIEW_STORE_APPS, L"Windows &Store Apps\tCtrl+2");
            AppendMenuW(hMenu, MF_STRING | (g_currentViewMode == AppViewMode::Processes ? MF_CHECKED : MF_UNCHECKED),
                        IDM_VIEW_PROCESSES, L"&Processes\tCtrl+3");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, IDM_VIEW_REFRESH, L"&Refresh\tCtrl+R");
            return hMenu;
        }
        case 4: { // Help
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, IDM_HELP_ABOUT, L"&About Zero...");
            return hMenu;
        }
        }
        return nullptr;
    }

    void OpenTopMenu(int index) {
        if (index < 0 || index >= TOP_MENU_COUNT || !g_hTopMenuBar) return;

        if (index == 2 && g_currentViewMode != AppViewMode::Processes) {
            SwitchToViewMode(AppViewMode::Processes);
        }

        RECT rcItem = GetTopMenuItemRect(g_hTopMenuBar, index);
        POINT pt = { rcItem.left, rcItem.bottom };
        ClientToScreen(g_hTopMenuBar, &pt);

        RECT rcItemScreen = rcItem;
        MapWindowPoints(g_hTopMenuBar, nullptr, (LPPOINT)&rcItemScreen, 2);

        TPMPARAMS tpm = { sizeof(tpm) };
        tpm.rcExclude = rcItemScreen;

        HMENU hPopup = CreateSubMenuForIndex(index);
        if (!hPopup) return;

        g_topMenuOpenIndex = index;
        InvalidateRect(g_hTopMenuBar, nullptr, FALSE);
        UpdateWindow(g_hTopMenuBar);

        TrackPopupMenuEx(hPopup, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_VERTICAL, pt.x, pt.y, g_hWndMain, &tpm);

        DestroyMenu(hPopup);
        g_topMenuOpenIndex = -1;
        InvalidateRect(g_hTopMenuBar, nullptr, FALSE);
    }

    LRESULT CALLBACK TopMenuBarWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBM = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
            HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM);

            // 1. Fill background with exact CLR_BOTTOM_BG (#1A1A1A)
            FillRect(memDC, &rcClient, g_hBrushBottom);

            // 2. Draw 1px bottom border with CLR_BOTTOM_BORDER (#2D2D2D)
            HPEN hPen = CreatePen(PS_SOLID, 1, CLR_BOTTOM_BORDER);
            HPEN oldPen = (HPEN)SelectObject(memDC, hPen);
            MoveToEx(memDC, rcClient.left, rcClient.bottom - 1, nullptr);
            LineTo(memDC, rcClient.right, rcClient.bottom - 1);
            SelectObject(memDC, oldPen);
            DeleteObject(hPen);

            // 3. Draw items
            HFONT oldFont = (HFONT)SelectObject(memDC, g_hFontStatus);
            SetBkMode(memDC, TRANSPARENT);

            for (int i = 0; i < TOP_MENU_COUNT; ++i) {
                RECT rcItem = GetTopMenuItemRect(hWnd, i);
                bool isActive = (i == g_topMenuOpenIndex);
                bool isHover = (i == g_topMenuHoverIndex);
                bool isCurrentMode = (i == 2 && g_currentViewMode == AppViewMode::Processes);

                if (isActive || isHover || isCurrentMode) {
                    COLORREF bg = isActive ? RGB(50, 50, 50) : (isCurrentMode ? RGB(40, 40, 40) : RGB(35, 35, 35));
                    HBRUSH hBrHover = CreateSolidBrush(bg);
                    FillRect(memDC, &rcItem, hBrHover);
                    DeleteObject(hBrHover);

                    COLORREF frameCol = isActive ? RGB(75, 75, 75) : (isCurrentMode ? RGB(224, 86, 46) : RGB(55, 55, 55));
                    HPEN hPenFrame = CreatePen(PS_SOLID, 1, frameCol);
                    HPEN oldFramePen = (HPEN)SelectObject(memDC, hPenFrame);
                    HBRUSH oldNullBr = (HBRUSH)SelectObject(memDC, GetStockObject(NULL_BRUSH));
                    Rectangle(memDC, rcItem.left, rcItem.top, rcItem.right, rcItem.bottom);
                    SelectObject(memDC, oldNullBr);
                    SelectObject(memDC, oldFramePen);
                    DeleteObject(hPenFrame);
                }

                SetTextColor(memDC, (isActive || isHover || isCurrentMode) ? CLR_TEXT_SELECTED : CLR_TEXT_NORMAL);
                DrawTextW(memDC, g_topMenuNames[i], -1, &rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            SelectObject(memDC, oldFont);

            BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBM);
            DeleteObject(memBM);
            DeleteDC(memDC);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            int hit = HitTestTopMenu(hWnd, x, y);
            if (hit != g_topMenuHoverIndex) {
                g_topMenuHoverIndex = hit;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            if (!g_topMenuTrackingMouse) {
                TRACKMOUSEEVENT tme = { sizeof(tme) };
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hWnd;
                TrackMouseEvent(&tme);
                g_topMenuTrackingMouse = true;
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            g_topMenuTrackingMouse = false;
            if (g_topMenuHoverIndex != -1) {
                g_topMenuHoverIndex = -1;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            int hit = HitTestTopMenu(hWnd, x, y);
            if (hit != -1) {
                OpenTopMenu(hit);
            }
            return 0;
        }
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    LRESULT CALLBACK HeaderSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
        switch (uMsg) {
        case HDM_LAYOUT: {
            LRESULT lr = DefSubclassProc(hWnd, uMsg, wParam, lParam);
            HDLAYOUT* pHL = reinterpret_cast<HDLAYOUT*>(lParam);
            if (pHL && pHL->pwpos && pHL->prc) {
                UINT dpi = GetWindowDpi(g_hWndMain);
                int targetHeight = (g_currentViewMode == AppViewMode::Processes) ? ScaleDpi(42, dpi) : ScaleDpi(36, dpi);
                pHL->pwpos->cy = targetHeight;
                pHL->prc->top = targetHeight;
            }
            return lr;
        }

        case WM_WINDOWPOSCHANGING: {
            WINDOWPOS* pwp = reinterpret_cast<WINDOWPOS*>(lParam);
            if (pwp && !(pwp->flags & SWP_NOSIZE)) {
                UINT dpi = GetWindowDpi(g_hWndMain);
                pwp->cy = (g_currentViewMode == AppViewMode::Processes) ? ScaleDpi(42, dpi) : ScaleDpi(36, dpi);
            }
            break;
        }

        case WM_LBUTTONUP: {
            HDHITTESTINFO hti = { 0 };
            hti.pt.x = GET_X_LPARAM(lParam);
            hti.pt.y = GET_Y_LPARAM(lParam);
            SendMessage(hWnd, HDM_HITTEST, 0, (LPARAM)&hti);
            if ((hti.flags & HHT_ONHEADER) && hti.iItem >= 0 && hti.iItem <= 4) {
                if (g_currentViewMode == AppViewMode::Processes) {
                    int col = hti.iItem;
                    if (g_processSortColumn == col) {
                        g_processSortAscending = !g_processSortAscending;
                    } else {
                        g_processSortColumn = col;
                        g_processSortAscending = (col == 0);
                    }
                    SortProcesses();
                    PopulateMainListView();
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBM = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
            HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM);

            // 1. Fill entire header background with g_hBrushBottom (CLR_BOTTOM_BG = #1A1A1A)
            FillRect(memDC, &rcClient, g_hBrushBottom);

            // 2. Full bottom border line with CLR_BOTTOM_BORDER (#2D2D2D)
            HPEN hPen = CreatePen(PS_SOLID, 1, CLR_BOTTOM_BORDER);
            HPEN oldPen = (HPEN)SelectObject(memDC, hPen);
            MoveToEx(memDC, rcClient.left, rcClient.bottom - 1, nullptr);
            LineTo(memDC, rcClient.right, rcClient.bottom - 1);

            UINT dpi = GetWindowDpi(g_hWndMain);
            int count = Header_GetItemCount(hWnd);

            if (g_currentViewMode == AppViewMode::Processes) {
                SetBkMode(memDC, TRANSPARENT);

                for (int i = 0; i < count; ++i) {
                    RECT rcItem;
                    Header_GetItemRect(hWnd, i, &rcItem);

                    bool isSorted = (i == g_processSortColumn);
                    if (isSorted) {
                        // Subtle highlight for sorted header column
                        HBRUSH hBrSortHdr = CreateSolidBrush(RGB(32, 36, 42));
                        FillRect(memDC, &rcItem, hBrSortHdr);
                        DeleteObject(hBrSortHdr);

                        // 2px accent underline
                        RECT rcUnderline = { rcItem.left, rcItem.bottom - ScaleDpi(2, dpi), rcItem.right, rcItem.bottom };
                        HBRUSH hBrAccent = CreateSolidBrush(CLR_BG_SELECTED);
                        FillRect(memDC, &rcUnderline, hBrAccent);
                        DeleteObject(hBrAccent);
                    }

                    // Vertical separator
                    MoveToEx(memDC, rcItem.right - 1, rcItem.top + ScaleDpi(6, dpi), nullptr);
                    LineTo(memDC, rcItem.right - 1, rcItem.bottom - ScaleDpi(6, dpi));

                    if (i == 0) {
                        // Col 0: Name (Left aligned, vertically centered)
                        HFONT oldFont = (HFONT)SelectObject(memDC, g_hFontHeader);
                        SetTextColor(memDC, isSorted ? CLR_TEXT_SELECTED : CLR_HEADER_TEXT);

                        std::wstring label = L"Name";
                        if (isSorted) {
                            label += g_processSortAscending ? L"  \x25B4" : L"  \x25BE";
                        }
                        RECT rcText = rcItem;
                        rcText.left += ScaleDpi(12, dpi);
                        rcText.right -= ScaleDpi(12, dpi);
                        DrawTextW(memDC, label.c_str(), -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                        SelectObject(memDC, oldFont);
                    } else {
                        // Cols 1..4: Metric on top, Label + Sort on bottom
                        std::wstring metricStr;
                        std::wstring labelStr;
                        if (i == 1) {
                            metricStr = ProcessManager::FormatPercent(g_sysStats.totalCpuPercent);
                            labelStr = L"CPU";
                        } else if (i == 2) {
                            metricStr = ProcessManager::FormatPercent(g_sysStats.totalMemoryPercent);
                            labelStr = L"Memory";
                        } else if (i == 3) {
                            metricStr = ProcessManager::FormatPercent(g_sysStats.totalDiskPercent);
                            labelStr = L"Disk";
                        } else if (i == 4) {
                            metricStr = ProcessManager::FormatPercent(g_sysStats.totalNetPercent);
                            labelStr = L"Network";
                        }

                        if (isSorted) {
                            labelStr += g_processSortAscending ? L" \x25B4" : L" \x25BE";
                        }

                        int halfH = (rcItem.bottom - rcItem.top) / 2;

                        // Top: Metric
                        RECT rcTop = rcItem;
                        rcTop.bottom = rcItem.top + halfH;
                        rcTop.right -= ScaleDpi(16, dpi);
                        rcTop.top += ScaleDpi(4, dpi);

                        HFONT oldFont = (HFONT)SelectObject(memDC, g_hFontStatus);
                        SetTextColor(memDC, isSorted ? CLR_TEXT_SELECTED : CLR_TEXT_NORMAL);
                        DrawTextW(memDC, metricStr.c_str(), -1, &rcTop, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

                        // Bottom: Column Label
                        RECT rcBottom = rcItem;
                        rcBottom.top = rcTop.bottom - ScaleDpi(1, dpi);
                        rcBottom.right -= ScaleDpi(16, dpi);
                        rcBottom.bottom -= ScaleDpi(3, dpi);

                        SelectObject(memDC, g_hFontHeader);
                        SetTextColor(memDC, isSorted ? CLR_TEXT_SELECTED : CLR_HEADER_TEXT);
                        DrawTextW(memDC, labelStr.c_str(), -1, &rcBottom, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

                        SelectObject(memDC, oldFont);
                    }
                }
            } else {
                // Apps Mode (Desktop / Store)
                HFONT oldFont = (HFONT)SelectObject(memDC, g_hFontHeader);
                SetBkMode(memDC, TRANSPARENT);
                SetTextColor(memDC, CLR_HEADER_TEXT);

                for (int i = 0; i < count; ++i) {
                    RECT rcItem;
                    Header_GetItemRect(hWnd, i, &rcItem);

                    // Vertical separator between columns
                    MoveToEx(memDC, rcItem.right - 1, rcItem.top + ScaleDpi(7, dpi), nullptr);
                    LineTo(memDC, rcItem.right - 1, rcItem.bottom - ScaleDpi(7, dpi));

                    wchar_t text[128] = { 0 };
                    HDITEMW hdi = { 0 };
                    hdi.mask = HDI_TEXT | HDI_FORMAT;
                    hdi.pszText = text;
                    hdi.cchTextMax = 128;
                    Header_GetItem(hWnd, i, &hdi);

                    RECT rcText = rcItem;
                    rcText.left += ScaleDpi(12, dpi);
                    rcText.right -= (i == 0) ? ScaleDpi(12, dpi) : ScaleDpi(20, dpi);

                    UINT align = (i == 0) ? DT_LEFT : DT_RIGHT;
                    DrawTextW(memDC, text, -1, &rcText, align | DT_VCENTER | DT_SINGLELINE);
                }
                SelectObject(memDC, oldFont);
            }

            SelectObject(memDC, oldPen);
            DeleteObject(hPen);

            BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBM);
            DeleteObject(memBM);
            DeleteDC(memDC);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_NCDESTROY:
            RemoveWindowSubclass(hWnd, HeaderSubclassProc, uIdSubclass);
            break;
        }
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }

    void EnableDarkThemeUx() {
        HMODULE hUxTheme = GetModuleHandleW(L"uxtheme.dll");
        if (!hUxTheme) hUxTheme = LoadLibraryW(L"uxtheme.dll");
        if (hUxTheme) {
            typedef int (WINAPI *fnSetPreferredAppMode)(int mode);
            auto pSetPreferredAppMode = (fnSetPreferredAppMode)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135));
            if (pSetPreferredAppMode) {
                pSetPreferredAppMode(2); // 2 = ForceDark
            }
        }
    }

    LRESULT CALLBACK SearchEditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
        switch (uMsg) {
        case WM_SETFOCUS:
            g_searchHasFocus = true;
            if (g_hBottomBar) InvalidateRect(g_hBottomBar, &g_rcCapsule, FALSE);
            break;
        case WM_KILLFOCUS:
            g_searchHasFocus = false;
            if (g_hBottomBar) InvalidateRect(g_hBottomBar, &g_rcCapsule, FALSE);
            break;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                SetWindowTextW(hWnd, L"");
                if (g_currentViewMode == AppViewMode::Processes) {
                    FilterProcesses();
                } else {
                    FilterApplications();
                }
                if (g_hBottomBar) InvalidateRect(g_hBottomBar, &g_rcCapsule, FALSE);
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && (wParam == 'R' || wParam == 'r')) {
                if (g_currentViewMode == AppViewMode::Processes) {
                    ReloadProcesses();
                } else {
                    ReloadApplications();
                }
                return 0;
            }
            if (wParam == VK_F5) {
                if (g_currentViewMode == AppViewMode::Processes) {
                    ReloadProcesses();
                } else {
                    ReloadApplications();
                }
                return 0;
            }
            break;
        case WM_NCDESTROY:
            RemoveWindowSubclass(hWnd, SearchEditSubclassProc, uIdSubclass);
            break;
        }
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }

    LRESULT CALLBACK BottomBarWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_ERASEBKGND:
            return 1;

        case WM_SIZE:
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);

            // Double buffer to avoid any flicker
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

            // Fill background
            FillRect(memDC, &rc, g_hBrushBottom);

            // Draw top 1px border line
            HPEN hPen = CreatePen(PS_SOLID, 1, CLR_BOTTOM_BORDER);
            HPEN hOldPen = (HPEN)SelectObject(memDC, hPen);
            MoveToEx(memDC, 0, 0, nullptr);
            LineTo(memDC, rc.right, 0);
            SelectObject(memDC, hOldPen);
            DeleteObject(hPen);

            UINT dpi = GetWindowDpi(hWnd);

            // Draw Capsule Search Bar on the far right
            if (g_rcCapsule.right > g_rcCapsule.left && g_rcCapsule.bottom > g_rcCapsule.top) {
                Gdiplus::Graphics graphics(memDC);
                graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

                float cx = (float)g_rcCapsule.left + 0.5f;
                float cy = (float)g_rcCapsule.top + 0.5f;
                float cw = (float)(g_rcCapsule.right - g_rcCapsule.left) - 1.0f;
                float ch = (float)(g_rcCapsule.bottom - g_rcCapsule.top) - 1.0f;
                float d = ch;
                float r = ch / 2.0f;

                Gdiplus::GraphicsPath path;
                path.AddArc(cx, cy, d, d, 90.0f, 180.0f);
                path.AddArc(cx + cw - d, cy, d, d, 270.0f, 180.0f);
                path.CloseFigure();

                // Fill capsule background
                Gdiplus::SolidBrush fillBrush(Gdiplus::Color(255, 38, 38, 38)); // Match CLR_EDIT_BG (#262626)
                graphics.FillPath(&fillBrush, &path);

                // Border: Coral/Orange if focused, lighter gray if hovered, dark subtle gray default
                Gdiplus::Color borderColor;
                if (g_searchHasFocus) {
                    borderColor = Gdiplus::Color(255, 224, 86, 46); // Accent #E0562E
                } else if (g_searchIsHovered) {
                    borderColor = Gdiplus::Color(255, 80, 80, 80);
                } else {
                    borderColor = Gdiplus::Color(255, 52, 52, 52);
                }
                Gdiplus::Pen borderPen(borderColor, 1.0f);
                graphics.DrawPath(&borderPen, &path);

                // Draw Search Magnifying Glass Icon
                float iconDiam = (float)ScaleDpi(10, dpi);
                float iconX = cx + r - (float)ScaleDpi(2, dpi);
                float iconY = cy + (ch - iconDiam) / 2.0f - (float)ScaleDpi(0.5f, dpi);

                Gdiplus::Color iconColor = g_searchHasFocus ? Gdiplus::Color(255, 224, 86, 46) : Gdiplus::Color(255, 140, 140, 140);
                Gdiplus::Pen iconPen(iconColor, (float)ScaleDpi(1.4f, dpi));
                graphics.DrawEllipse(&iconPen, iconX, iconY, iconDiam, iconDiam);

                float handleStartX = iconX + iconDiam * 0.78f;
                float handleStartY = iconY + iconDiam * 0.78f;
                float handleEndX = handleStartX + (float)ScaleDpi(3.5f, dpi);
                float handleEndY = handleStartY + (float)ScaleDpi(3.5f, dpi);
                graphics.DrawLine(&iconPen, handleStartX, handleStartY, handleEndX, handleEndY);

                // Draw Clear 'X' button if text is non-empty
                int textLen = GetWindowTextLengthW(g_hEditSearch);
                if (textLen > 0) {
                    float closeX = cx + cw - r + (float)ScaleDpi(1, dpi);
                    float closeY = cy + ch / 2.0f;
                    float arm = (float)ScaleDpi(3.5f, dpi);
                    Gdiplus::Color closeColor = g_searchClearHovered ? Gdiplus::Color(255, 230, 230, 230) : Gdiplus::Color(255, 140, 140, 140);
                    Gdiplus::Pen closePen(closeColor, (float)ScaleDpi(1.3f, dpi));
                    graphics.DrawLine(&closePen, closeX - arm, closeY - arm, closeX + arm, closeY + arm);
                    graphics.DrawLine(&closePen, closeX - arm, closeY + arm, closeX + arm, closeY - arm);
                }
            }

            // Draw Left Status: "240 programs..." or "124 processes..."
            std::wstring statusStr;
            if (g_currentViewMode == AppViewMode::Desktop) {
                statusStr = std::to_wstring(g_displayedApps.size()) + L" programs";
                if (g_totalSizeBytes > 0) {
                    statusStr += L" of " + FormatTotalSize(g_totalSizeBytes) + L" size in total";
                }
                if (g_displayedApps.size() != g_allApps.size()) {
                    statusStr += L" (Filtered from " + std::to_wstring(g_allApps.size()) + L")";
                }
            } else if (g_currentViewMode == AppViewMode::WindowsStore) {
                statusStr = std::to_wstring(g_displayedApps.size()) + L" Windows Store apps";
                if (g_totalSizeBytes > 0) {
                    statusStr += L" of " + FormatTotalSize(g_totalSizeBytes) + L" size in total";
                }
                if (g_displayedApps.size() != g_allApps.size()) {
                    statusStr += L" (Filtered from " + std::to_wstring(g_allApps.size()) + L")";
                }
            } else {
                statusStr = std::to_wstring(g_displayedProcesses.size()) + L" processes";
                statusStr += L" (CPU: " + ProcessManager::FormatPercent(g_sysStats.totalCpuPercent) +
                             L", Memory: " + ProcessManager::FormatMemory(g_sysStats.usedPhysMemBytes) +
                             L" / " + ProcessManager::FormatMemory(g_sysStats.totalPhysMemBytes) + L")";
                if (g_displayedProcesses.size() != g_allProcesses.size()) {
                    statusStr += L" (Filtered from " + std::to_wstring(g_allProcesses.size()) + L")";
                }
            }

            HFONT hOldFont = (HFONT)SelectObject(memDC, g_hFontStatus);
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, CLR_TEXT_MUTED);

            int textLeft = ScaleDpi(14, dpi);
            int textRight = (g_rcCapsule.left > textLeft) ? (g_rcCapsule.left - ScaleDpi(14, dpi)) : textLeft;
            RECT rcText = { textLeft, 0, textRight, rc.bottom };
            DrawTextW(memDC, statusStr.c_str(), -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            SelectObject(memDC, hOldFont);

            BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            bool inCapsule = PtInRect(&g_rcCapsule, pt) != FALSE;

            bool inClearBtn = false;
            if (GetWindowTextLengthW(g_hEditSearch) > 0) {
                UINT dpi = GetWindowDpi(hWnd);
                int r = (g_rcCapsule.bottom - g_rcCapsule.top) / 2;
                RECT rcClear = {
                    g_rcCapsule.right - r * 2,
                    g_rcCapsule.top,
                    g_rcCapsule.right,
                    g_rcCapsule.bottom
                };
                inClearBtn = PtInRect(&rcClear, pt) != FALSE;
            }

            if (inCapsule != g_searchIsHovered || inClearBtn != g_searchClearHovered) {
                g_searchIsHovered = inCapsule;
                g_searchClearHovered = inClearBtn;
                InvalidateRect(hWnd, &g_rcCapsule, FALSE);
            }

            if (!g_bottomBarTrackingMouse) {
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hWnd, 0 };
                TrackMouseEvent(&tme);
                g_bottomBarTrackingMouse = true;
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            g_bottomBarTrackingMouse = false;
            if (g_searchIsHovered || g_searchClearHovered) {
                g_searchIsHovered = false;
                g_searchClearHovered = false;
                InvalidateRect(hWnd, &g_rcCapsule, FALSE);
            }
            return 0;
        }

        case WM_SETCURSOR: {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hWnd, &pt);
            if (PtInRect(&g_rcCapsule, pt)) {
                if (g_searchClearHovered) {
                    SetCursor(LoadCursor(nullptr, IDC_HAND));
                } else {
                    SetCursor(LoadCursor(nullptr, IDC_IBEAM));
                }
                return TRUE;
            }
            break;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (PtInRect(&g_rcCapsule, pt)) {
                if (GetWindowTextLengthW(g_hEditSearch) > 0) {
                    UINT dpi = GetWindowDpi(hWnd);
                    int r = (g_rcCapsule.bottom - g_rcCapsule.top) / 2;
                    RECT rcClear = {
                        g_rcCapsule.right - r * 2,
                        g_rcCapsule.top,
                        g_rcCapsule.right,
                        g_rcCapsule.bottom
                    };
                    if (PtInRect(&rcClear, pt)) {
                        SetWindowTextW(g_hEditSearch, L"");
                        if (g_currentViewMode == AppViewMode::Processes) {
                            FilterProcesses();
                        } else {
                            FilterApplications();
                        }
                        SetFocus(g_hEditSearch);
                        InvalidateRect(hWnd, &g_rcCapsule, FALSE);
                        return 0;
                    }
                }
                SetFocus(g_hEditSearch);
                return 0;
            }
            break;
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            if ((HWND)lParam == g_hEditSearch) {
                HDC hdcEdit = (HDC)wParam;
                SetTextColor(hdcEdit, CLR_TEXT_NORMAL);
                SetBkColor(hdcEdit, CLR_EDIT_BG);
                return (LRESULT)g_hBrushEdit;
            }
            break;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == IDC_SEARCH_EDIT && HIWORD(wParam) == EN_CHANGE) {
                if (g_currentViewMode == AppViewMode::Processes) {
                    FilterProcesses();
                } else {
                    FilterApplications();
                }
                InvalidateRect(hWnd, &g_rcCapsule, FALSE);
                return 0;
            }
            break;
        }
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    LRESULT HandleListViewCustomDraw(LPARAM lParam) {
        NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(lParam);

        if (pLVCD->nmcd.dwDrawStage == CDDS_PREPAINT) {
            return CDRF_NOTIFYITEMDRAW;
        }

        if (pLVCD->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
            int iItem = static_cast<int>(pLVCD->nmcd.dwItemSpec);

            if (g_currentViewMode == AppViewMode::Processes) {
                if (iItem < 0 || iItem >= static_cast<int>(g_displayedProcesses.size())) {
                    return CDRF_DODEFAULT;
                }

                const auto& proc = g_displayedProcesses[iItem];
                HDC hdc = pLVCD->nmcd.hdc;
                SetStretchBltMode(hdc, HALFTONE);
                SetBrushOrgEx(hdc, 0, 0, nullptr);

                bool isSelected = (ListView_GetItemState(g_hListView, iItem, LVIS_SELECTED) & LVIS_SELECTED) != 0;

                RECT rcItem;
                ListView_GetItemRect(g_hListView, iItem, &rcItem, LVIR_BOUNDS);

                // 1. Draw Background: Selected = Orange, Unselected = Alternating Dark Gray
                COLORREF bgCol = isSelected ? CLR_BG_SELECTED : ((iItem % 2 == 0) ? CLR_BG_EVEN : CLR_BG_ODD);
                HBRUSH hBr = CreateSolidBrush(bgCol);
                FillRect(hdc, &rcItem, hBr);
                DeleteObject(hBr);

                SetBkMode(hdc, TRANSPARENT);

                UINT dpi = GetWindowDpi(g_hWndMain);
                int iconSize = ScaleDpi(16, dpi);

                // Column 0: Name (+ chevron if grouped + icon)
                int col0Width = ListView_GetColumnWidth(g_hListView, 0);
                RECT rcCol0 = rcItem;
                rcCol0.right = rcCol0.left + col0Width;

                // Subtle tint for sorted column across rows when not selected
                auto DrawSortedColumnTint = [&](const RECT& rcCol, int colIndex) {
                    if (!isSelected && g_processSortColumn == colIndex) {
                        COLORREF tintCol = (iItem % 2 == 0) ? RGB(30, 32, 38) : RGB(36, 38, 44);
                        HBRUSH hBrTint = CreateSolidBrush(tintCol);
                        FillRect(hdc, &rcCol, hBrTint);
                        DeleteObject(hBrTint);
                    }
                };

                DrawSortedColumnTint(rcCol0, 0);

                int curX = rcCol0.left + ScaleDpi(8, dpi);

                // If instanceCount > 1, draw expandable chevron indicator '>'
                if (proc.instanceCount > 1) {
                    RECT rcChevron = { curX, rcCol0.top, curX + ScaleDpi(10, dpi), rcCol0.bottom };
                    HFONT hOldF = (HFONT)SelectObject(hdc, g_hFontStatus);
                    SetTextColor(hdc, isSelected ? CLR_TEXT_SELECTED : CLR_TEXT_MUTED);
                    DrawTextW(hdc, L"\x25B8", -1, &rcChevron, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    SelectObject(hdc, hOldF);
                    curX += ScaleDpi(12, dpi);
                } else {
                    curX += ScaleDpi(4, dpi);
                }

                // Draw Icon
                int iconY = rcCol0.top + (rcCol0.bottom - rcCol0.top - iconSize) / 2;
                if (proc.hIcon) {
                    DrawIconEx(hdc, curX, iconY, proc.hIcon, iconSize, iconSize, 0, nullptr, DI_NORMAL);
                }
                curX += iconSize + ScaleDpi(8, dpi);

                // Draw Friendly Process Name + (count)
                std::wstring nameDisplay = proc.name;
                if (proc.instanceCount > 1) {
                    nameDisplay += L" (" + std::to_wstring(proc.instanceCount) + L")";
                }

                RECT rcName = rcCol0;
                rcName.left = curX;
                rcName.right -= ScaleDpi(8, dpi);

                HFONT hOldFont = (HFONT)SelectObject(hdc, g_hFont);
                SetTextColor(hdc, isSelected ? CLR_TEXT_SELECTED : CLR_TEXT_NORMAL);
                DrawTextW(hdc, nameDisplay.c_str(), -1, &rcName, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                // Column 1: CPU
                int col1Width = ListView_GetColumnWidth(g_hListView, 1);
                RECT rcCol1 = rcItem;
                rcCol1.left = rcCol0.right;
                rcCol1.right = rcCol1.left + col1Width;
                DrawSortedColumnTint(rcCol1, 1);

                RECT rcText1 = rcCol1;
                rcText1.right -= ScaleDpi(16, dpi);
                if (proc.cpuPercent < 0.05) {
                    SetTextColor(hdc, isSelected ? RGB(240, 200, 190) : CLR_TEXT_MUTED);
                } else {
                    SetTextColor(hdc, isSelected ? CLR_TEXT_SELECTED : CLR_TEXT_NORMAL);
                }
                DrawTextW(hdc, proc.cpuText.c_str(), -1, &rcText1, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

                // Column 2: Memory
                int col2Width = ListView_GetColumnWidth(g_hListView, 2);
                RECT rcCol2 = rcItem;
                rcCol2.left = rcCol1.right;
                rcCol2.right = rcCol2.left + col2Width;
                DrawSortedColumnTint(rcCol2, 2);

                RECT rcText2 = rcCol2;
                rcText2.right -= ScaleDpi(16, dpi);
                SetTextColor(hdc, isSelected ? CLR_TEXT_SELECTED : CLR_TEXT_NORMAL);
                DrawTextW(hdc, proc.memoryText.c_str(), -1, &rcText2, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

                // Column 3: Disk
                int col3Width = ListView_GetColumnWidth(g_hListView, 3);
                RECT rcCol3 = rcItem;
                rcCol3.left = rcCol2.right;
                rcCol3.right = rcCol3.left + col3Width;
                DrawSortedColumnTint(rcCol3, 3);

                RECT rcText3 = rcCol3;
                rcText3.right -= ScaleDpi(16, dpi);
                if (proc.diskBytesPerSec < 1024.0) {
                    SetTextColor(hdc, isSelected ? RGB(240, 200, 190) : CLR_TEXT_MUTED);
                } else {
                    SetTextColor(hdc, isSelected ? CLR_TEXT_SELECTED : CLR_TEXT_NORMAL);
                }
                DrawTextW(hdc, proc.diskText.c_str(), -1, &rcText3, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

                // Column 4: Network
                int col4Width = ListView_GetColumnWidth(g_hListView, 4);
                RECT rcCol4 = rcItem;
                rcCol4.left = rcCol3.right;
                rcCol4.right = rcCol4.left + col4Width;
                DrawSortedColumnTint(rcCol4, 4);

                RECT rcText4 = rcCol4;
                rcText4.right -= ScaleDpi(16, dpi);
                if (proc.netBytesPerSec < 100.0) {
                    SetTextColor(hdc, isSelected ? RGB(240, 200, 190) : CLR_TEXT_MUTED);
                } else {
                    SetTextColor(hdc, isSelected ? CLR_TEXT_SELECTED : CLR_TEXT_NORMAL);
                }
                DrawTextW(hdc, proc.networkText.c_str(), -1, &rcText4, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

                SelectObject(hdc, hOldFont);
                return CDRF_SKIPDEFAULT;
            }

            // Desktop / Store Apps Mode
            if (iItem < 0 || iItem >= static_cast<int>(g_displayedApps.size())) {
                return CDRF_DODEFAULT;
            }

            const auto& app = g_displayedApps[iItem];
            HDC hdc = pLVCD->nmcd.hdc;
            SetStretchBltMode(hdc, HALFTONE);
            SetBrushOrgEx(hdc, 0, 0, nullptr);

            bool isSelected = (ListView_GetItemState(g_hListView, iItem, LVIS_SELECTED) & LVIS_SELECTED) != 0;

            RECT rcItem;
            ListView_GetItemRect(g_hListView, iItem, &rcItem, LVIR_BOUNDS);

            // 1. Draw Background: Selected = Orange, Unselected = Alternating Dark Gray
            COLORREF bgCol = isSelected ? CLR_BG_SELECTED : ((iItem % 2 == 0) ? CLR_BG_EVEN : CLR_BG_ODD);
            HBRUSH hBr = CreateSolidBrush(bgCol);
            FillRect(hdc, &rcItem, hBr);
            DeleteObject(hBr);

            SetBkMode(hdc, TRANSPARENT);

            UINT dpi = GetWindowDpi(g_hWndMain);
            int iconSize = ScaleDpi(18, dpi);

            // 2. Column 0: Program Name + Icon + (32-bit tag)
            int col0Width = ListView_GetColumnWidth(g_hListView, 0);
            RECT rcCol0 = rcItem;
            rcCol0.right = rcCol0.left + col0Width;

            // Draw Icon
            int iconX = rcCol0.left + ScaleDpi(8, dpi);
            int iconY = rcCol0.top + (rcCol0.bottom - rcCol0.top - iconSize) / 2;
            if (app.hIcon) {
                DrawIconEx(hdc, iconX, iconY, app.hIcon, iconSize, iconSize, 0, nullptr, DI_NORMAL);
            }

            // Draw Program Name
            RECT rcName = rcCol0;
            rcName.left = iconX + iconSize + ScaleDpi(12, dpi);
            rcName.right -= ScaleDpi(8, dpi);

            HFONT hOldFont = (HFONT)SelectObject(hdc, g_hFont);
            SetTextColor(hdc, isSelected ? CLR_TEXT_SELECTED : CLR_TEXT_NORMAL);

            // Only show (32-bit) tag if app specifically came from 32-bit WOW64 registry node
            if (app.show32BitTag) {
                std::wstring nameOnly = app.displayName;
                SIZE sz;
                GetTextExtentPoint32W(hdc, nameOnly.c_str(), static_cast<int>(nameOnly.length()), &sz);

                DrawTextW(hdc, nameOnly.c_str(), -1, &rcName, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                if (rcName.left + sz.cx + ScaleDpi(6, dpi) < rcName.right) {
                    RECT rcTag = rcName;
                    rcTag.left += sz.cx + ScaleDpi(6, dpi);
                    SetTextColor(hdc, isSelected ? RGB(255, 230, 200) : CLR_TEXT_MUTED);
                    DrawTextW(hdc, L"(32-bit)", -1, &rcTag, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                }
            } else {
                DrawTextW(hdc, app.displayName.c_str(), -1, &rcName, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }

            // 3. Column 1: Size (Right Aligned)
            int col1Width = ListView_GetColumnWidth(g_hListView, 1);
            RECT rcCol1 = rcItem;
            rcCol1.left = rcCol0.right;
            rcCol1.right = rcCol1.left + col1Width - ScaleDpi(16, dpi);

            SetTextColor(hdc, isSelected ? CLR_TEXT_SELECTED : CLR_TEXT_NORMAL);
            DrawTextW(hdc, app.sizeFormatted.c_str(), -1, &rcCol1, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

            // 4. Column 2: Installed On (Right Aligned)
            int col2Width = ListView_GetColumnWidth(g_hListView, 2);
            RECT rcCol2 = rcItem;
            rcCol2.left = rcCol0.right + col1Width;
            rcCol2.right = rcCol2.left + col2Width - ScaleDpi(20, dpi);

            DrawTextW(hdc, app.installDate.c_str(), -1, &rcCol2, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, hOldFont);
            return CDRF_SKIPDEFAULT;
        }

        return CDRF_DODEFAULT;
    }

    LRESULT HandleHeaderCustomDraw(LPARAM lParam) {
        return CDRF_DODEFAULT;
    }

    void LayoutWindow(int width, int height) {
        UINT dpi = GetWindowDpi(g_hWndMain);
        const int topBarHeight = ScaleDpi(30, dpi);
        const int bottomBarHeight = ScaleDpi(36, dpi);
        int listHeight = height - topBarHeight - bottomBarHeight;
        if (listHeight < 50) listHeight = 50;

        // Position Top Menu Bar
        if (g_hTopMenuBar) {
            SetWindowPos(g_hTopMenuBar, nullptr, 0, 0, width, topBarHeight, SWP_NOZORDER);
        }

        // Resize ListView (starts immediately below top menu bar)
        if (g_hListView) {
            SetWindowPos(g_hListView, nullptr, 0, topBarHeight, width, listHeight, SWP_NOZORDER);
        }

        // Resize Bottom Bar
        if (g_hBottomBar) {
            SetWindowPos(g_hBottomBar, nullptr, 0, height - bottomBarHeight, width, bottomBarHeight, SWP_NOZORDER);
        }

        // Capsule Search box on the far right of bottom bar
        const int capsuleWidth = ScaleDpi(280, dpi);
        const int capsuleHeight = ScaleDpi(26, dpi);
        const int rightMargin = ScaleDpi(14, dpi);
        int capsuleX = width - capsuleWidth - rightMargin;
        if (capsuleX < ScaleDpi(10, dpi)) capsuleX = ScaleDpi(10, dpi);
        int capsuleY = (bottomBarHeight - capsuleHeight) / 2;

        g_rcCapsule.left = capsuleX;
        g_rcCapsule.top = capsuleY;
        g_rcCapsule.right = capsuleX + capsuleWidth;
        g_rcCapsule.bottom = capsuleY + capsuleHeight;

        // Position inner borderless EDIT control neatly within the capsule
        const int radius = capsuleHeight / 2;
        const int iconDiam = ScaleDpi(10, dpi);
        const int iconX = capsuleX + radius - ScaleDpi(2, dpi);
        const int editLeft = iconX + iconDiam + ScaleDpi(6, dpi);
        const int editRight = capsuleX + capsuleWidth - radius - ScaleDpi(14, dpi);
        const int editWidth = (editRight > editLeft) ? (editRight - editLeft) : 50;
        const int editHeight = ScaleDpi(18, dpi);
        const int editTop = capsuleY + (capsuleHeight - editHeight) / 2;

        if (g_hEditSearch) {
            SetWindowPos(g_hEditSearch, nullptr, editLeft, editTop, editWidth, editHeight, SWP_NOZORDER);
        }

        if (g_currentViewMode == AppViewMode::Processes) {
            int col1Width = ScaleDpi(90, dpi);  // CPU
            int col2Width = ScaleDpi(130, dpi); // Memory
            int col3Width = ScaleDpi(110, dpi); // Disk
            int col4Width = ScaleDpi(110, dpi); // Network
            int col0Width = width - col1Width - col2Width - col3Width - col4Width - GetSystemMetrics(SM_CXVSCROLL) - 2;
            if (col0Width < 220) col0Width = 220;
            if (g_hListView) {
                ListView_SetColumnWidth(g_hListView, 0, col0Width);
                ListView_SetColumnWidth(g_hListView, 1, col1Width);
                ListView_SetColumnWidth(g_hListView, 2, col2Width);
                ListView_SetColumnWidth(g_hListView, 3, col3Width);
                ListView_SetColumnWidth(g_hListView, 4, col4Width);
            }
        } else {
            // Dynamically adjust Column 0 width so Columns 1 and 2 stay fixed at right
            int col1Width = ScaleDpi(130, dpi); // Size
            int col2Width = ScaleDpi(160, dpi); // Installed On
            int col0Width = width - col1Width - col2Width - GetSystemMetrics(SM_CXVSCROLL) - 2;
            if (col0Width < 250) col0Width = 250;
            if (g_hListView) {
                ListView_SetColumnWidth(g_hListView, 0, col0Width);
                ListView_SetColumnWidth(g_hListView, 1, col1Width);
                ListView_SetColumnWidth(g_hListView, 2, col2Width);
            }
        }
    }

    LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_CREATE: {
            g_hWndMain = hWnd;
            RecreateFonts(hWnd);

            g_hBrushBottom = CreateSolidBrush(CLR_BOTTOM_BG);
            g_hBrushEdit = CreateSolidBrush(CLR_EDIT_BG);

            // Enable Windows 10/11 Immersive Dark Title Bar and Popup Theme
            BOOL darkMode = TRUE;
            DwmSetWindowAttribute(hWnd, 20, &darkMode, sizeof(darkMode)); // DWMWA_USE_IMMERSIVE_DARK_MODE (Win10 20H1+ & Win11)
            DwmSetWindowAttribute(hWnd, 19, &darkMode, sizeof(darkMode)); // Older Win10 builds
            EnableDarkThemeUx();

            // Register and Create Custom Top Menu Bar (Matches Bottom Bar Color #1A1A1A)
            const wchar_t* TOP_MENU_CLASS = L"ZeroTopMenuBarClass";
            WNDCLASSEXW mwc = { sizeof(mwc) };
            mwc.lpfnWndProc = TopMenuBarWndProc;
            mwc.hInstance = g_hInstance;
            mwc.lpszClassName = TOP_MENU_CLASS;
            mwc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            RegisterClassExW(&mwc);

            g_hTopMenuBar = CreateWindowExW(0, TOP_MENU_CLASS, L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                0, 0, 0, 0, hWnd, (HMENU)IDC_TOP_MENUBAR, g_hInstance, nullptr);

            // Create SysListView32 with no border, full width
            g_hListView = CreateWindowExW(0, WC_LISTVIEWW, L"",
                WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_VSCROLL | WS_CLIPSIBLINGS,
                0, 0, 0, 0, hWnd, (HMENU)IDC_APP_LISTVIEW, g_hInstance, nullptr);

            ListView_SetExtendedListViewStyle(g_hListView, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
            SendMessage(g_hListView, WM_SETFONT, (WPARAM)g_hFont, TRUE);

            g_hHeader = ListView_GetHeader(g_hListView);
            if (g_hHeader) {
                SetWindowTheme(g_hHeader, L"", L"");
                SendMessage(g_hHeader, WM_SETFONT, (WPARAM)g_hFontHeader, TRUE);
                SetWindowSubclass(g_hHeader, HeaderSubclassProc, 1, 0);
            }

            UINT dpi = GetWindowDpi(hWnd);

            // Add Columns
            LVCOLUMNW col = { 0 };
            col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT | LVCF_SUBITEM;

            col.fmt = LVCFMT_LEFT;
            col.cx = ScaleDpi(600, dpi);
            col.pszText = const_cast<LPWSTR>(L"Program Name");
            ListView_InsertColumn(g_hListView, 0, &col);

            col.fmt = LVCFMT_RIGHT;
            col.cx = ScaleDpi(130, dpi);
            col.pszText = const_cast<LPWSTR>(L"Size");
            ListView_InsertColumn(g_hListView, 1, &col);

            col.fmt = LVCFMT_RIGHT;
            col.cx = ScaleDpi(160, dpi);
            col.pszText = const_cast<LPWSTR>(L"Installed On");
            ListView_InsertColumn(g_hListView, 2, &col);

            // Create Custom Bottom Bar Container
            const wchar_t* BOTTOM_CLASS = L"ZeroBottomBarClass";
            WNDCLASSEXW bwc = { sizeof(bwc) };
            bwc.lpfnWndProc = BottomBarWndProc;
            bwc.hInstance = g_hInstance;
            bwc.lpszClassName = BOTTOM_CLASS;
            bwc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            RegisterClassExW(&bwc);

            g_hBottomBar = CreateWindowExW(0, BOTTOM_CLASS, L"",
                WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)IDC_STATUS_BAR, g_hInstance, nullptr);

            // Create Search Edit in Bottom Bar (borderless, embedded in capsule)
            g_hEditSearch = CreateWindowExW(0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                0, 0, 0, 0, g_hBottomBar, (HMENU)IDC_SEARCH_EDIT, g_hInstance, nullptr);

            SendMessage(g_hEditSearch, WM_SETFONT, (WPARAM)g_hFontStatus, TRUE);
            SendMessage(g_hEditSearch, EM_SETCUEBANNER, TRUE, (LPARAM)L"Type to find a program");
            SetWindowSubclass(g_hEditSearch, SearchEditSubclassProc, 2, 0);

            // Load initial list
            ReloadApplications();
            return 0;
        }

        case WM_DPICHANGED: {
            RECT* prc = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(hWnd, nullptr, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, SWP_NOZORDER | SWP_NOACTIVATE);
            RecreateFonts(hWnd);
            ReloadApplications();
            return 0;
        }

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            LayoutWindow(width, height);
            return 0;
        }

        case WM_NOTIFY: {
            LPNMHDR pnm = reinterpret_cast<LPNMHDR>(lParam);

            // Custom draw for ListView
            if (pnm->idFrom == IDC_APP_LISTVIEW && pnm->code == NM_CUSTOMDRAW) {
                return HandleListViewCustomDraw(lParam);
            }

            // Custom draw for Header
            if (pnm->hwndFrom == g_hHeader && pnm->code == NM_CUSTOMDRAW) {
                return HandleHeaderCustomDraw(lParam);
            }

            // Column Header Click (for sorting)
            if (pnm->idFrom == IDC_APP_LISTVIEW && pnm->code == LVN_COLUMNCLICK) {
                NMLISTVIEW* pnmlv = reinterpret_cast<NMLISTVIEW*>(lParam);
                int col = pnmlv->iSubItem;
                if (g_currentViewMode == AppViewMode::Processes) {
                    if (col >= 0 && col <= 4) {
                        if (g_processSortColumn == col) {
                            g_processSortAscending = !g_processSortAscending;
                        } else {
                            g_processSortColumn = col;
                            g_processSortAscending = (col == 0);
                        }
                        SortProcesses();
                        PopulateMainListView();
                        if (g_hHeader) InvalidateRect(g_hHeader, nullptr, FALSE);
                    }
                }
                return 0;
            }

            if (pnm->hwndFrom == g_hHeader && (pnm->code == HDN_ITEMCLICKW || pnm->code == HDN_ITEMCLICKA)) {
                NMHEADERW* pnmh = reinterpret_cast<NMHEADERW*>(lParam);
                int col = pnmh->iItem;
                if (g_currentViewMode == AppViewMode::Processes) {
                    if (col >= 0 && col <= 4) {
                        if (g_processSortColumn == col) {
                            g_processSortAscending = !g_processSortAscending;
                        } else {
                            g_processSortColumn = col;
                            g_processSortAscending = (col == 0);
                        }
                        SortProcesses();
                        PopulateMainListView();
                        if (g_hHeader) InvalidateRect(g_hHeader, nullptr, FALSE);
                    }
                }
                return 0;
            }

            // Double Click on Item
            if (pnm->idFrom == IDC_APP_LISTVIEW && pnm->code == NM_DBLCLK) {
                if (g_currentViewMode == AppViewMode::Processes) {
                    OpenProcessFileLocation();
                } else {
                    PerformStandardUninstall();
                }
                return 0;
            }

            // Right Click Context Menu
            if (pnm->idFrom == IDC_APP_LISTVIEW && pnm->code == NM_RCLICK) {
                POINT pt;
                GetCursorPos(&pt);
                ShowContextMenu(hWnd, pt.x, pt.y);
                return 0;
            }
            break;
        }

        case WM_TIMER: {
            if (wParam == IDT_PROCESS_REFRESH_TIMER) {
                if (g_currentViewMode == AppViewMode::Processes) {
                    UpdateProcessesInPlace();
                }
                return 0;
            }
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
            case IDM_FILE_EXIT:
                DestroyWindow(hWnd);
                return 0;
            case IDM_FILE_EXPORT_HTML:
                ExportToHTML();
                return 0;
            case IDM_FILE_EXPORT_CSV:
                ExportToCSV();
                return 0;
            case IDM_ACTION_UNINSTALL:
                PerformStandardUninstall();
                return 0;
            case IDM_ACTION_FORCE_REMOVAL:
                PerformForceRemoval();
                return 0;
            case IDM_ACTION_REMOVE_ENTRY:
                PerformRemoveEntry();
                return 0;
            case IDM_ACTION_OPEN_FOLDER:
                OpenInstallLocation();
                return 0;
            case IDM_ACTION_OPEN_REGISTRY:
                OpenRegistryKey();
                return 0;
            case IDM_ACTION_SEARCH_GOOGLE:
                SearchGoogle();
                return 0;
            case IDM_VIEW_DESKTOP_APPS:
                SwitchToViewMode(AppViewMode::Desktop);
                return 0;
            case IDM_VIEW_STORE_APPS:
                SwitchToViewMode(AppViewMode::WindowsStore);
                return 0;
            case IDM_VIEW_PROCESSES:
                SwitchToViewMode(AppViewMode::Processes);
                return 0;
            case IDM_PROCESS_END_TASK:
                PerformEndTask();
                return 0;
            case IDM_PROCESS_OPEN_FOLDER:
                OpenProcessFileLocation();
                return 0;
            case IDM_PROCESS_PROPERTIES:
                ShowProcessProperties();
                return 0;
            case IDM_PROCESS_SEARCH_ONLINE:
                SearchProcessOnline();
                return 0;
            case IDM_VIEW_REFRESH:
                if (g_currentViewMode == AppViewMode::Processes) {
                    ReloadProcesses();
                } else {
                    ReloadApplications();
                }
                return 0;
            case IDM_HELP_ABOUT:
                MessageBoxW(hWnd, L"Zero 0.0.1-beta (Portable)\n\nFast, lightweight standalone uninstaller and leftover cleaner.\nNative C++ Win32.",
                            L"About Zero", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            break;
        }

        case WM_SYSKEYDOWN: {
            if (wParam == 'F' || wParam == 'f') {
                OpenTopMenu(0);
                return 0;
            }
            if (wParam == 'A' || wParam == 'a') {
                OpenTopMenu(1);
                return 0;
            }
            if (wParam == 'P' || wParam == 'p') {
                OpenTopMenu(2);
                return 0;
            }
            if (wParam == 'V' || wParam == 'v') {
                OpenTopMenu(3);
                return 0;
            }
            if (wParam == 'H' || wParam == 'h') {
                OpenTopMenu(4);
                return 0;
            }
            if (wParam == VK_F10) {
                OpenTopMenu(0);
                return 0;
            }
            break;
        }

        case WM_KEYDOWN: {
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                if (wParam == 'R' || wParam == 'r') {
                    if (g_currentViewMode == AppViewMode::Processes) {
                        ReloadProcesses();
                    } else {
                        ReloadApplications();
                    }
                    return 0;
                }
                if (wParam == 'S' || wParam == 's') {
                    ExportToHTML();
                    return 0;
                }
                if (wParam == '1') {
                    SwitchToViewMode(AppViewMode::Desktop);
                    return 0;
                }
                if (wParam == '2') {
                    SwitchToViewMode(AppViewMode::WindowsStore);
                    return 0;
                }
                if (wParam == '3') {
                    SwitchToViewMode(AppViewMode::Processes);
                    return 0;
                }
            }
            if (wParam == VK_F5) {
                if (g_currentViewMode == AppViewMode::Processes) {
                    ReloadProcesses();
                } else {
                    ReloadApplications();
                }
                return 0;
            }
            if (wParam == VK_RETURN) {
                if (g_currentViewMode == AppViewMode::Processes) {
                    OpenProcessFileLocation();
                } else {
                    PerformStandardUninstall();
                }
                return 0;
            }
            if (wParam == VK_DELETE) {
                if (g_currentViewMode == AppViewMode::Processes) {
                    PerformEndTask();
                } else {
                    PerformForceRemoval();
                }
                return 0;
            }
            break;
        }

        case WM_DESTROY:
            KillTimer(hWnd, IDT_PROCESS_REFRESH_TIMER);
            if (g_hFont) DeleteObject(g_hFont);
            if (g_hFontBold) DeleteObject(g_hFontBold);
            if (g_hFontHeader) DeleteObject(g_hFontHeader);
            if (g_hFontStatus) DeleteObject(g_hFontStatus);
            if (g_hBrushBottom) DeleteObject(g_hBrushBottom);
            if (g_hBrushEdit) DeleteObject(g_hBrushEdit);
            if (g_hImageList) ImageList_Destroy(g_hImageList);
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    g_hInstance = hInstance;

    // Initialize COM
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Initialize GDI+ for crisp PNG and high-DPI icon handling
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    // Initialize Common Controls
    INITCOMMONCONTROLSEX icex = { sizeof(icex) };
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    const wchar_t* MAIN_CLASS = L"ZeroGeekMainClass";

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = MAIN_CLASS;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(CLR_BG_EVEN);

    if (!RegisterClassExW(&wc)) {
        return 1;
    }

    // Query screen DPI to launch with generous initial window dimensions
    HDC hdcScreen = GetDC(nullptr);
    UINT screenDpi = GetDeviceCaps(hdcScreen, LOGPIXELSY);
    ReleaseDC(nullptr, hdcScreen);
    if (screenDpi == 0) screenDpi = 96;

    int initWidth = MulDiv(1120, screenDpi, 96);
    int initHeight = MulDiv(700, screenDpi, 96);
    int posX = (GetSystemMetrics(SM_CXSCREEN) - initWidth) / 2;
    int posY = (GetSystemMetrics(SM_CYSCREEN) - initHeight) / 2;

    g_hWndMain = CreateWindowExW(
        0,
        MAIN_CLASS,
        L"Zero 0.0.1-beta",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        posX, posY, initWidth, initHeight,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!g_hWndMain) {
        return 1;
    }

    ShowWindow(g_hWndMain, nCmdShow);
    UpdateWindow(g_hWndMain);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN) {
            bool ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (ctrlPressed && (msg.wParam == 'R' || msg.wParam == 'r')) {
                if (g_currentViewMode == AppViewMode::Processes) {
                    ReloadProcesses();
                } else {
                    ReloadApplications();
                }
                continue;
            }
            if (ctrlPressed && msg.wParam == '1') {
                SwitchToViewMode(AppViewMode::Desktop);
                continue;
            }
            if (ctrlPressed && msg.wParam == '2') {
                SwitchToViewMode(AppViewMode::WindowsStore);
                continue;
            }
            if (ctrlPressed && msg.wParam == '3') {
                SwitchToViewMode(AppViewMode::Processes);
                continue;
            }
            if (msg.wParam == VK_F5) {
                if (g_currentViewMode == AppViewMode::Processes) {
                    ReloadProcesses();
                } else {
                    ReloadApplications();
                }
                continue;
            }
            // Handle ESC in search box to clear search
            if (msg.wParam == VK_ESCAPE && msg.hwnd == g_hEditSearch) {
                SetWindowTextW(g_hEditSearch, L"");
                if (g_currentViewMode == AppViewMode::Processes) {
                    FilterProcesses();
                } else {
                    FilterApplications();
                }
                if (g_hBottomBar) InvalidateRect(g_hBottomBar, &g_rcCapsule, FALSE);
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
