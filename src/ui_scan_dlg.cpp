#include "ui_scan_dlg.h"
#include <commctrl.h>
#include <dwmapi.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")

namespace {
    // Geek Uninstaller Dark Palette
    const COLORREF CLR_BG          = RGB(25, 25, 25);     // #191919
    const COLORREF CLR_TEXT_NORMAL = RGB(224, 224, 224);  // #E0E0E0
    const COLORREF CLR_TEXT_MUTED  = RGB(140, 140, 140);  // #8C8C8C
    const COLORREF CLR_ACCENT      = RGB(224, 86, 46);    // #E0562E Vivid Coral/Orange
    const COLORREF CLR_BAR_BG      = RGB(38, 38, 38);     // #262626
    const COLORREF CLR_BAR_BORDER  = RGB(60, 60, 60);     // #3C3C3C
    const COLORREF CLR_BTN_BG      = RGB(38, 38, 38);     // #262626
    const COLORREF CLR_BTN_BORDER  = RGB(60, 60, 60);     // #3C3C3C
    const COLORREF CLR_BTN_PRESSED = RGB(26, 26, 26);     // #1A1A1A

    const UINT WM_APP_PROGRESS_TICK  = WM_APP + 101;
    const UINT WM_APP_SCAN_COMPLETE  = WM_APP + 102;
    const UINT_PTR TIMER_ANIM_ID     = 1001;
    const int IDC_BTN_CANCEL         = 2001;

    struct ScanContext {
        HWND hWnd = nullptr;
        HWND hBtnCancel = nullptr;

        HFONT hFontTitle = nullptr;
        HFONT hFontStage = nullptr;
        HFONT hFontNormal = nullptr;
        HFONT hFontSmall = nullptr;
        HBRUSH hBrushBg = nullptr;

        const InstalledApp* pApp = nullptr;
        const std::vector<InstalledApp>* pAllApps = nullptr;
        std::vector<LeftoverItem>* pLeftovers = nullptr;

        std::atomic<bool> cancelToken{false};
        std::atomic<bool> isCompleted{false};
        std::atomic<bool> isDialogActive{true};

        std::mutex mtx;
        std::wstring stage = L"Initializing Deep Scan...";
        std::wstring currentPath = L"";
        int percent = 0;
        int displayPercent = 0; // For smooth animated progression
        std::vector<LeftoverItem> foundResults;

        std::thread workerThread;
    };

    UINT GetDlgDpi(HWND hWnd) {
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

    void CreateScanFonts(ScanContext* ctx, UINT dpi) {
        if (ctx->hFontTitle) DeleteObject(ctx->hFontTitle);
        if (ctx->hFontStage) DeleteObject(ctx->hFontStage);
        if (ctx->hFontNormal) DeleteObject(ctx->hFontNormal);
        if (ctx->hFontSmall) DeleteObject(ctx->hFontSmall);

        ctx->hFontTitle = CreateFontW(-ScaleDpi(15, dpi), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        ctx->hFontStage = CreateFontW(-ScaleDpi(13, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        ctx->hFontNormal = CreateFontW(-ScaleDpi(12, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        ctx->hFontSmall = CreateFontW(-ScaleDpi(11, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }

    LRESULT CALLBACK ScanWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        ScanContext* ctx = reinterpret_cast<ScanContext*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

        switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* pcs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            ctx = reinterpret_cast<ScanContext*>(pcs->lpCreateParams);
            ctx->hWnd = hWnd;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx));

            // Enable Windows 10/11 Immersive Dark Titlebar
            BOOL darkMode = TRUE;
            DwmSetWindowAttribute(hWnd, 20, &darkMode, sizeof(darkMode));
            DwmSetWindowAttribute(hWnd, 19, &darkMode, sizeof(darkMode));

            UINT dpi = GetDlgDpi(hWnd);
            CreateScanFonts(ctx, dpi);
            ctx->hBrushBg = CreateSolidBrush(CLR_BG);

            // Create Owner-drawn Cancel Button
            ctx->hBtnCancel = CreateWindowExW(
                0, L"BUTTON", L"Cancel",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                0, 0, 0, 0, hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BTN_CANCEL)), GetModuleHandle(nullptr), nullptr
            );

            // Start 30ms timer for smooth progress animation
            SetTimer(hWnd, TIMER_ANIM_ID, 30, nullptr);

            // Launch background scanning thread
            ctx->workerThread = std::thread([ctx]() {
                auto onProgress = [ctx](const std::wstring& stage, const std::wstring& currentPath, int pct) {
                    {
                        std::lock_guard<std::mutex> lock(ctx->mtx);
                        ctx->stage = stage;
                        ctx->currentPath = currentPath;
                        ctx->percent = pct;
                    }
                    PostMessageW(ctx->hWnd, WM_APP_PROGRESS_TICK, 0, 0);
                };

                std::vector<LeftoverItem> items = Cleaner::ScanLeftovers(
                    *ctx->pApp,
                    *ctx->pAllApps,
                    onProgress,
                    &ctx->cancelToken
                );

                {
                    std::lock_guard<std::mutex> lock(ctx->mtx);
                    ctx->foundResults = std::move(items);
                }

                PostMessageW(ctx->hWnd, WM_APP_SCAN_COMPLETE, 0, 0);
            });

            return 0;
        }

        case WM_DPICHANGED: {
            RECT* prc = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(hWnd, nullptr, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, SWP_NOZORDER | SWP_NOACTIVATE);
            UINT dpi = GetDlgDpi(hWnd);
            CreateScanFonts(ctx, dpi);
            InvalidateRect(hWnd, nullptr, TRUE);
            return 0;
        }

        case WM_SIZE: {
            if (!ctx) break;
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            UINT dpi = GetDlgDpi(hWnd);

            int btnWidth = ScaleDpi(110, dpi);
            int btnHeight = ScaleDpi(32, dpi);
            int margin = ScaleDpi(24, dpi);

            if (ctx->hBtnCancel) {
                SetWindowPos(ctx->hBtnCancel, nullptr,
                    width - margin - btnWidth,
                    height - margin - btnHeight,
                    btnWidth, btnHeight,
                    SWP_NOZORDER);
            }
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_TIMER: {
            if (wParam == TIMER_ANIM_ID && ctx) {
                // Smoothly interpolate displayPercent towards actual percent
                int target = 0;
                {
                    std::lock_guard<std::mutex> lock(ctx->mtx);
                    target = ctx->percent;
                }
                if (ctx->displayPercent < target) {
                    ctx->displayPercent += (target - ctx->displayPercent > 10) ? 3 : 1;
                    if (ctx->displayPercent > target) ctx->displayPercent = target;
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
            }
            return 0;
        }

        case WM_APP_PROGRESS_TICK: {
            if (ctx) {
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_APP_SCAN_COMPLETE: {
            if (ctx) {
                KillTimer(hWnd, TIMER_ANIM_ID);
                if (ctx->workerThread.joinable()) {
                    ctx->workerThread.join();
                }

                if (!ctx->cancelToken.load()) {
                    ctx->isCompleted = true;
                    if (ctx->pLeftovers) {
                        *ctx->pLeftovers = std::move(ctx->foundResults);
                    }
                }

                ctx->isDialogActive = false;
                DestroyWindow(hWnd);
            }
            return 0;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* pDIS = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (pDIS->CtlType == ODT_BUTTON && pDIS->CtlID == IDC_BTN_CANCEL) {
                HDC hdc = pDIS->hDC;
                RECT rc = pDIS->rcItem;
                bool isPressed = (pDIS->itemState & ODS_SELECTED) != 0;
                bool isFocus = (pDIS->itemState & ODS_FOCUS) != 0;

                COLORREF bgCol = isPressed ? CLR_BTN_PRESSED : CLR_BTN_BG;
                COLORREF borderCol = isFocus ? CLR_ACCENT : CLR_BTN_BORDER;

                HBRUSH hBr = CreateSolidBrush(bgCol);
                HPEN hPen = CreatePen(PS_SOLID, 1, borderCol);
                HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hBr);
                HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

                RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);

                SelectObject(hdc, hOldBr);
                SelectObject(hdc, hOldPen);
                DeleteObject(hBr);
                DeleteObject(hPen);

                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, CLR_TEXT_NORMAL);
                HFONT hOldFont = (HFONT)SelectObject(hdc, ctx ? ctx->hFontNormal : GetStockObject(DEFAULT_GUI_FONT));

                wchar_t text[64] = { 0 };
                GetWindowTextW(pDIS->hwndItem, text, 64);
                DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                SelectObject(hdc, hOldFont);
                return TRUE;
            }
            break;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == IDC_BTN_CANCEL || LOWORD(wParam) == IDCANCEL) {
                if (ctx && !ctx->cancelToken.load()) {
                    ctx->cancelToken.store(true);
                    {
                        std::lock_guard<std::mutex> lock(ctx->mtx);
                        ctx->stage = L"Cancelling scan...";
                    }
                    if (ctx->hBtnCancel) {
                        EnableWindow(ctx->hBtnCancel, FALSE);
                    }
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            if (!ctx) {
                EndPaint(hWnd, &ps);
                return 0;
            }

            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            int width = rcClient.right;
            int height = rcClient.bottom;

            // Double Buffering
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

            // Fill Background
            FillRect(memDC, &rcClient, ctx->hBrushBg);

            UINT dpi = GetDlgDpi(hWnd);
            int margin = ScaleDpi(24, dpi);
            int iconSize = ScaleDpi(36, dpi);

            // 1. Draw Application Icon
            int iconX = margin;
            int iconY = margin;
            if (ctx->pApp && ctx->pApp->hIcon) {
                DrawIconEx(memDC, iconX, iconY, ctx->pApp->hIcon, iconSize, iconSize, 0, nullptr, DI_NORMAL);
            } else {
                HICON hDef = LoadIcon(nullptr, IDI_APPLICATION);
                if (hDef) {
                    DrawIconEx(memDC, iconX, iconY, hDef, iconSize, iconSize, 0, nullptr, DI_NORMAL);
                }
            }

            SetBkMode(memDC, TRANSPARENT);

            // 2. Draw Title Header: "Deep Scanning - [App Name]"
            std::wstring titleText = L"Deep Scanning - " + (ctx->pApp ? ctx->pApp->displayName : L"Application");
            RECT rcTitle = { iconX + iconSize + ScaleDpi(16, dpi), iconY, width - margin, iconY + ScaleDpi(22, dpi) };
            HFONT hOldFont = (HFONT)SelectObject(memDC, ctx->hFontTitle);
            SetTextColor(memDC, CLR_TEXT_NORMAL);
            DrawTextW(memDC, titleText.c_str(), -1, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            // 3. Draw Subtitle: "Searching file system and registry for leftover traces..."
            RECT rcSub = { rcTitle.left, rcTitle.bottom + ScaleDpi(2, dpi), width - margin, rcTitle.bottom + ScaleDpi(20, dpi) };
            SelectObject(memDC, ctx->hFontSmall);
            SetTextColor(memDC, CLR_TEXT_MUTED);
            const wchar_t* subText = L"Searching file system and registry for leftover traces...";
            DrawTextW(memDC, subText, -1, &rcSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Read safe snapshot of stage, path, percent
            std::wstring curStage;
            std::wstring curPath;
            int pct = ctx->displayPercent;
            {
                std::lock_guard<std::mutex> lock(ctx->mtx);
                curStage = ctx->stage;
                curPath = ctx->currentPath;
                if (ctx->displayPercent == 0 && ctx->percent > 0) {
                    pct = ctx->percent;
                }
            }

            // 4. Draw Stage & Percentage
            int stageY = iconY + iconSize + ScaleDpi(26, dpi);
            std::wstring stageDisplay = curStage + L" (" + std::to_wstring(pct) + L"%)";
            RECT rcStage = { margin, stageY, width - margin, stageY + ScaleDpi(22, dpi) };
            SelectObject(memDC, ctx->hFontStage);
            SetTextColor(memDC, CLR_TEXT_NORMAL);
            DrawTextW(memDC, stageDisplay.c_str(), -1, &rcStage, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // 5. Draw Custom Progress Bar
            int barY = rcStage.bottom + ScaleDpi(8, dpi);
            int barHeight = ScaleDpi(16, dpi);
            RECT rcBar = { margin, barY, width - margin, barY + barHeight };

            // Bar background & border
            HBRUSH hBrBarBg = CreateSolidBrush(CLR_BAR_BG);
            HPEN hPenBar = CreatePen(PS_SOLID, 1, CLR_BAR_BORDER);
            HBRUSH hOldBarBr = (HBRUSH)SelectObject(memDC, hBrBarBg);
            HPEN hOldBarPen = (HPEN)SelectObject(memDC, hPenBar);

            RoundRect(memDC, rcBar.left, rcBar.top, rcBar.right, rcBar.bottom, 4, 4);

            SelectObject(memDC, hOldBarBr);
            SelectObject(memDC, hOldBarPen);
            DeleteObject(hBrBarBg);
            DeleteObject(hPenBar);

            // Bar fill (Vivid Orange)
            if (pct > 0) {
                int totalBarWidth = rcBar.right - rcBar.left - 2;
                int fillWidth = (totalBarWidth * pct) / 100;
                if (fillWidth > totalBarWidth) fillWidth = totalBarWidth;
                if (fillWidth < 4) fillWidth = 4;

                RECT rcFill = { rcBar.left + 1, rcBar.top + 1, rcBar.left + 1 + fillWidth, rcBar.bottom - 1 };
                HBRUSH hBrFill = CreateSolidBrush(CLR_ACCENT);
                HPEN hPenFill = CreatePen(PS_SOLID, 1, CLR_ACCENT);
                hOldBarBr = (HBRUSH)SelectObject(memDC, hBrFill);
                hOldBarPen = (HPEN)SelectObject(memDC, hPenFill);

                RoundRect(memDC, rcFill.left, rcFill.top, rcFill.right, rcFill.bottom, 3, 3);

                SelectObject(memDC, hOldBarBr);
                SelectObject(memDC, hOldBarPen);
                DeleteObject(hBrFill);
                DeleteObject(hPenFill);
            }

            // 6. Draw Live Scanning Path
            int pathY = rcBar.bottom + ScaleDpi(10, dpi);
            int btnHeight = ScaleDpi(32, dpi);
            int pathWidth = width - (margin * 2) - ScaleDpi(120, dpi); // Give space before cancel button
            RECT rcPath = { margin, pathY, margin + pathWidth, pathY + ScaleDpi(20, dpi) };
            SelectObject(memDC, ctx->hFontSmall);
            SetTextColor(memDC, CLR_TEXT_MUTED);
            DrawTextW(memDC, curPath.c_str(), -1, &rcPath, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS);

            SelectObject(memDC, hOldFont);

            // Copy to screen
            BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

            // Clean up double buffer
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_CLOSE: {
            if (ctx && !ctx->cancelToken.load()) {
                ctx->cancelToken.store(true);
                {
                    std::lock_guard<std::mutex> lock(ctx->mtx);
                    ctx->stage = L"Cancelling scan...";
                }
                if (ctx->hBtnCancel) {
                    EnableWindow(ctx->hBtnCancel, FALSE);
                }
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_DESTROY: {
            if (ctx) {
                KillTimer(hWnd, TIMER_ANIM_ID);
                ctx->cancelToken.store(true);
                if (ctx->workerThread.joinable()) {
                    ctx->workerThread.join();
                }
                if (ctx->hFontTitle) DeleteObject(ctx->hFontTitle);
                if (ctx->hFontStage) DeleteObject(ctx->hFontStage);
                if (ctx->hFontNormal) DeleteObject(ctx->hFontNormal);
                if (ctx->hFontSmall) DeleteObject(ctx->hFontSmall);
                if (ctx->hBrushBg) DeleteObject(ctx->hBrushBg);
                ctx->isDialogActive = false;
            }
            return 0;
        }
        }

        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

bool ScanProgressDialog::Show(HWND hWndParent, 
                             const InstalledApp& app, 
                             const std::vector<InstalledApp>& allApps, 
                             std::vector<LeftoverItem>& leftovers) {
    const wchar_t* CLASS_NAME = L"ZeroScanDlgClass";

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = ScanWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(CLR_BG);

    RegisterClassExW(&wc);

    ScanContext ctx;
    ctx.pApp = &app;
    ctx.pAllApps = &allApps;
    ctx.pLeftovers = &leftovers;
    ctx.isCompleted = false;
    ctx.isDialogActive = true;

    UINT screenDpi = GetDlgDpi(hWndParent);
    int dlgWidth = ScaleDpi(620, screenDpi);
    int dlgHeight = ScaleDpi(250, screenDpi);

    RECT rcParent;
    GetWindowRect(hWndParent, &rcParent);
    int posX = rcParent.left + (rcParent.right - rcParent.left - dlgWidth) / 2;
    int posY = rcParent.top + (rcParent.bottom - rcParent.top - dlgHeight) / 2;
    if (posX < 0) posX = 100;
    if (posY < 0) posY = 100;

    std::wstring winTitle = L"Deep Scanning - " + app.displayName;

    EnableWindow(hWndParent, FALSE);

    HWND hWndDlg = CreateWindowExW(
        0,
        CLASS_NAME,
        winTitle.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        posX, posY, dlgWidth, dlgHeight,
        hWndParent, nullptr, GetModuleHandle(nullptr), &ctx
    );

    MSG msg;
    while (ctx.isDialogActive && GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessage(hWndDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!IsWindow(hWndDlg)) {
            break;
        }
    }

    EnableWindow(hWndParent, TRUE);
    SetActiveWindow(hWndParent);
    UnregisterClassW(CLASS_NAME, GetModuleHandle(nullptr));

    return ctx.isCompleted.load();
}
