#include "ui_leftovers_dlg.h"
#include "resource.h"
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

namespace {
    // Geek Uninstaller Dark Palette (Matching main.cpp exactly)
    const COLORREF CLR_BG_EVEN        = RGB(25, 25, 25);     // #191919
    const COLORREF CLR_BG_ODD         = RGB(33, 33, 33);     // #212121
    const COLORREF CLR_BG_SELECTED    = RGB(224, 86, 46);    // #E0562E Vivid Coral/Orange
    const COLORREF CLR_TEXT_NORMAL    = RGB(224, 224, 224);  // Light Gray
    const COLORREF CLR_TEXT_SELECTED  = RGB(255, 255, 255);  // Pure White
    const COLORREF CLR_TEXT_MUTED     = RGB(140, 140, 140);  // Dimmed Gray
    const COLORREF CLR_HEADER_BG      = RGB(22, 22, 22);     // #161616
    const COLORREF CLR_HEADER_TEXT    = RGB(200, 200, 200);  // #C8C8C8
    const COLORREF CLR_BORDER         = RGB(45, 45, 45);     // #2D2D2D
    const COLORREF CLR_BTN_BG         = RGB(38, 38, 38);     // #262626
    const COLORREF CLR_BTN_BORDER     = RGB(60, 60, 60);     // #3C3C3C
    const COLORREF CLR_BTN_PRESSED    = RGB(26, 26, 26);     // #1A1A1A
    const COLORREF CLR_BTN_DEL_BG     = RGB(200, 50, 30);    // Crimson Coral for Delete
    const COLORREF CLR_BTN_DEL_PRESS  = RGB(160, 35, 20);

    struct DialogContext {
        HWND hWnd = nullptr;
        HWND hListView = nullptr;
        HWND hHeader = nullptr;
        HWND hBtnSelectAll = nullptr;
        HWND hBtnDeselectAll = nullptr;
        HWND hBtnDelete = nullptr;
        HWND hBtnClose = nullptr;
        HWND hStaticInfo = nullptr;

        HFONT hFont = nullptr;
        HFONT hFontBold = nullptr;
        HFONT hFontHeader = nullptr;
        HBRUSH hBrushBg = nullptr;

        const InstalledApp* pApp = nullptr;
        std::vector<LeftoverItem>* pItems = nullptr;
        bool deletedAny = false;
        bool isDialogActive = true;
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

    std::wstring FormatBytes(uint64_t bytes) {
        if (bytes == 0) return L"-";
        if (bytes < 1024) return std::to_wstring(bytes) + L" B";
        if (bytes < 1024 * 1024) {
            std::wstringstream ss;
            ss << std::fixed << std::setprecision(1) << (bytes / 1024.0) << L" KB";
            return ss.str();
        }
        std::wstringstream ss;
        ss << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024.0)) << L" MB";
        return ss.str();
    }

    void LayoutDialogControls(DialogContext* ctx, int width, int height) {
        if (!ctx || !ctx->hListView) return;

        UINT dpi = GetDlgDpi(ctx->hWnd);
        const int margin = ScaleDpi(16, dpi);
        const int btnHeight = ScaleDpi(32, dpi);
        const int headerHeight = ScaleDpi(48, dpi);

        // Header Label
        SetWindowPos(ctx->hStaticInfo, nullptr, margin, margin, width - (margin * 2), headerHeight, SWP_NOZORDER);

        // ListView
        int listTop = margin + headerHeight + ScaleDpi(8, dpi);
        int listHeight = height - listTop - btnHeight - (margin * 2);
        if (listHeight < ScaleDpi(100, dpi)) listHeight = ScaleDpi(100, dpi);

        SetWindowPos(ctx->hListView, nullptr, margin, listTop, width - (margin * 2), listHeight, SWP_NOZORDER);

        // Column widths
        int col1Width = ScaleDpi(90, dpi);
        int col0Width = ScaleDpi(160, dpi);
        int col2Width = width - (margin * 2) - col0Width - col1Width - GetSystemMetrics(SM_CXVSCROLL) - 4;
        if (col2Width < ScaleDpi(200, dpi)) col2Width = ScaleDpi(200, dpi);

        ListView_SetColumnWidth(ctx->hListView, 0, col0Width);
        ListView_SetColumnWidth(ctx->hListView, 1, col1Width);
        ListView_SetColumnWidth(ctx->hListView, 2, col2Width);

        // Bottom buttons
        int btnTop = listTop + listHeight + margin;
        int btnWidth = ScaleDpi(110, dpi);

        SetWindowPos(ctx->hBtnSelectAll, nullptr, margin, btnTop, btnWidth, btnHeight, SWP_NOZORDER);
        SetWindowPos(ctx->hBtnDeselectAll, nullptr, margin + btnWidth + ScaleDpi(8, dpi), btnTop, btnWidth, btnHeight, SWP_NOZORDER);

        int delBtnWidth = ScaleDpi(140, dpi);
        int closeBtnWidth = ScaleDpi(85, dpi);
        int closeLeft = width - margin - closeBtnWidth;
        int delLeft = closeLeft - ScaleDpi(10, dpi) - delBtnWidth;

        SetWindowPos(ctx->hBtnDelete, nullptr, delLeft, btnTop, delBtnWidth, btnHeight, SWP_NOZORDER);
        SetWindowPos(ctx->hBtnClose, nullptr, closeLeft, btnTop, closeBtnWidth, btnHeight, SWP_NOZORDER);
    }

    void PopulateListView(DialogContext* ctx) {
        SendMessage(ctx->hListView, WM_SETREDRAW, FALSE, 0);
        ListView_DeleteAllItems(ctx->hListView);

        LVITEMW lvi = { 0 };
        lvi.mask = LVIF_TEXT;

        for (size_t i = 0; i < ctx->pItems->size(); ++i) {
            const auto& item = (*ctx->pItems)[i];

            lvi.iItem = static_cast<int>(i);
            lvi.iSubItem = 0;
            lvi.pszText = const_cast<LPWSTR>(item.displayType.c_str());
            ListView_InsertItem(ctx->hListView, &lvi);

            std::wstring sizeStr = FormatBytes(item.sizeBytes);
            ListView_SetItemText(ctx->hListView, static_cast<int>(i), 1, const_cast<LPWSTR>(sizeStr.c_str()));
            ListView_SetItemText(ctx->hListView, static_cast<int>(i), 2, const_cast<LPWSTR>(item.fullPath.c_str()));

            ListView_SetCheckState(ctx->hListView, static_cast<int>(i), item.selected ? TRUE : FALSE);
        }

        SendMessage(ctx->hListView, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(ctx->hListView, nullptr, TRUE);
    }

    LRESULT HandleLeftoversCustomDraw(DialogContext* ctx, LPARAM lParam) {
        NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(lParam);

        if (pLVCD->nmcd.dwDrawStage == CDDS_PREPAINT) {
            return CDRF_NOTIFYITEMDRAW;
        }

        if (pLVCD->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
            int iItem = static_cast<int>(pLVCD->nmcd.dwItemSpec);
            if (iItem < 0 || iItem >= static_cast<int>(ctx->pItems->size())) {
                return CDRF_DODEFAULT;
            }

            const auto& item = (*ctx->pItems)[iItem];
            HDC hdc = pLVCD->nmcd.hdc;
            bool isSelected = (ListView_GetItemState(ctx->hListView, iItem, LVIS_SELECTED) & LVIS_SELECTED) != 0;
            bool isChecked = (ListView_GetCheckState(ctx->hListView, iItem) != 0);

            RECT rcItem;
            ListView_GetItemRect(ctx->hListView, iItem, &rcItem, LVIR_BOUNDS);

            // 1. Draw Alternating / Selected Background
            COLORREF bgCol = isSelected ? CLR_BG_SELECTED : ((iItem % 2 == 0) ? CLR_BG_EVEN : CLR_BG_ODD);
            HBRUSH hBr = CreateSolidBrush(bgCol);
            FillRect(hdc, &rcItem, hBr);
            DeleteObject(hBr);

            SetBkMode(hdc, TRANSPARENT);
            UINT dpi = GetDlgDpi(ctx->hWnd);

            // 2. Column 0: Checkbox + Display Type
            int col0Width = ListView_GetColumnWidth(ctx->hListView, 0);
            RECT rcCol0 = rcItem;
            rcCol0.right = rcCol0.left + col0Width;

            // Draw Custom Dark Checkbox
            int boxSize = ScaleDpi(15, dpi);
            int boxX = rcCol0.left + ScaleDpi(8, dpi);
            int boxY = rcCol0.top + (rcCol0.bottom - rcCol0.top - boxSize) / 2;
            RECT rcBox = { boxX, boxY, boxX + boxSize, boxY + boxSize };

            COLORREF boxFill = isChecked ? (isSelected ? RGB(255, 255, 255) : CLR_BG_SELECTED) : RGB(35, 35, 35);
            COLORREF boxPenCol = isChecked ? (isSelected ? RGB(255, 255, 255) : CLR_BG_SELECTED) : RGB(85, 85, 85);

            HBRUSH hBoxBr = CreateSolidBrush(boxFill);
            HPEN hBoxPen = CreatePen(PS_SOLID, 1, boxPenCol);
            HGDIOBJ hOldBr = SelectObject(hdc, hBoxBr);
            HGDIOBJ hOldPen = SelectObject(hdc, hBoxPen);
            Rectangle(hdc, rcBox.left, rcBox.top, rcBox.right, rcBox.bottom);

            if (isChecked) {
                // Draw checkmark tick
                COLORREF tickCol = isSelected ? CLR_BG_SELECTED : RGB(255, 255, 255);
                HPEN hTickPen = CreatePen(PS_SOLID, ScaleDpi(2, dpi), tickCol);
                SelectObject(hdc, hTickPen);
                MoveToEx(hdc, boxX + ScaleDpi(3, dpi), boxY + ScaleDpi(7, dpi), nullptr);
                LineTo(hdc, boxX + ScaleDpi(6, dpi), boxY + ScaleDpi(11, dpi));
                LineTo(hdc, boxX + ScaleDpi(12, dpi), boxY + ScaleDpi(4, dpi));
                DeleteObject(hTickPen);
            }
            SelectObject(hdc, hOldBr);
            SelectObject(hdc, hOldPen);
            DeleteObject(hBoxBr);
            DeleteObject(hBoxPen);

            // Draw Type Text
            RECT rcType = rcCol0;
            rcType.left = boxX + boxSize + ScaleDpi(10, dpi);
            rcType.right -= ScaleDpi(6, dpi);

            HFONT hOldFont = (HFONT)SelectObject(hdc, ctx->hFont);
            SetTextColor(hdc, isSelected ? CLR_TEXT_SELECTED : CLR_TEXT_NORMAL);
            DrawTextW(hdc, item.displayType.c_str(), -1, &rcType, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            // 3. Column 1: Size (Right Aligned)
            int col1Width = ListView_GetColumnWidth(ctx->hListView, 1);
            RECT rcCol1 = rcItem;
            rcCol1.left = rcCol0.right;
            rcCol1.right = rcCol1.left + col1Width - ScaleDpi(12, dpi);

            std::wstring sizeStr = FormatBytes(item.sizeBytes);
            DrawTextW(hdc, sizeStr.c_str(), -1, &rcCol1, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

            // 4. Column 2: Full Path
            int col2Width = ListView_GetColumnWidth(ctx->hListView, 2);
            RECT rcCol2 = rcItem;
            rcCol2.left = rcCol0.right + col1Width + ScaleDpi(8, dpi);
            rcCol2.right = rcCol2.left + col2Width - ScaleDpi(12, dpi);

            DrawTextW(hdc, item.fullPath.c_str(), -1, &rcCol2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            SelectObject(hdc, hOldFont);
            return CDRF_SKIPDEFAULT;
        }
        return CDRF_DODEFAULT;
    }

    LRESULT HandleHeaderCustomDraw(DialogContext* ctx, LPARAM lParam) {
        NMCUSTOMDRAW* pNMCD = reinterpret_cast<NMCUSTOMDRAW*>(lParam);

        if (pNMCD->dwDrawStage == CDDS_PREPAINT) {
            return CDRF_NOTIFYITEMDRAW;
        }

        if (pNMCD->dwDrawStage == CDDS_ITEMPREPAINT) {
            HDC hdc = pNMCD->hdc;
            RECT rc = pNMCD->rc;

            HBRUSH hBr = CreateSolidBrush(CLR_HEADER_BG);
            FillRect(hdc, &rc, hBr);
            DeleteObject(hBr);

            HPEN hPen = CreatePen(PS_SOLID, 1, CLR_BORDER);
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            MoveToEx(hdc, rc.right - 1, rc.top, nullptr);
            LineTo(hdc, rc.right - 1, rc.bottom);
            MoveToEx(hdc, rc.left, rc.bottom - 1, nullptr);
            LineTo(hdc, rc.right, rc.bottom - 1);
            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);

            wchar_t textBuf[64] = { 0 };
            HDITEMW hdi = { 0 };
            hdi.mask = HDI_TEXT;
            hdi.pszText = textBuf;
            hdi.cchTextMax = 64;
            Header_GetItem(pNMCD->hdr.hwndFrom, pNMCD->dwItemSpec, &hdi);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, CLR_HEADER_TEXT);
            HFONT hOldFont = (HFONT)SelectObject(hdc, ctx->hFontHeader);

            UINT dpi = GetDlgDpi(ctx->hWnd);
            RECT rcText = rc;
            rcText.left += ScaleDpi(10, dpi);
            rcText.right -= ScaleDpi(10, dpi);

            UINT align = (pNMCD->dwItemSpec == 1) ? DT_RIGHT : DT_LEFT;
            DrawTextW(hdc, textBuf, -1, &rcText, align | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, hOldFont);
            return CDRF_SKIPDEFAULT;
        }
        return CDRF_DODEFAULT;
    }

    void DrawDarkButton(DialogContext* ctx, DRAWITEMSTRUCT* pDIS) {
        HDC hdc = pDIS->hDC;
        RECT rc = pDIS->rcItem;
        bool isPressed = (pDIS->itemState & ODS_SELECTED) != 0;
        bool isDeleteBtn = (pDIS->CtlID == IDC_BTN_DELETE_SELECTED);

        COLORREF bgCol;
        if (isDeleteBtn) {
            bgCol = isPressed ? CLR_BTN_DEL_PRESS : CLR_BTN_DEL_BG;
        } else {
            bgCol = isPressed ? CLR_BTN_PRESSED : CLR_BTN_BG;
        }

        HBRUSH hBr = CreateSolidBrush(bgCol);
        FillRect(hdc, &rc, hBr);
        DeleteObject(hBr);

        HPEN hPen = CreatePen(PS_SOLID, 1, isDeleteBtn ? CLR_BTN_DEL_BG : CLR_BTN_BORDER);
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        HGDIOBJ hOldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, hOldPen);
        SelectObject(hdc, hOldBrush);
        DeleteObject(hPen);

        wchar_t btnText[64] = { 0 };
        GetWindowTextW(pDIS->hwndItem, btnText, 64);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, isDeleteBtn ? RGB(255, 255, 255) : CLR_TEXT_NORMAL);
        HFONT hOldFont = (HFONT)SelectObject(hdc, isDeleteBtn ? ctx->hFontBold : ctx->hFont);

        DrawTextW(hdc, btnText, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, hOldFont);
    }

    LRESULT CALLBACK LeftoversWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        DialogContext* ctx = reinterpret_cast<DialogContext*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

        switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* pcs = reinterpret_cast<CREATESTRUCT*>(lParam);
            ctx = reinterpret_cast<DialogContext*>(pcs->lpCreateParams);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx));
            ctx->hWnd = hWnd;

            // Enable Immersive Dark Mode Titlebar
            BOOL darkMode = TRUE;
            DwmSetWindowAttribute(hWnd, 20, &darkMode, sizeof(darkMode));
            DwmSetWindowAttribute(hWnd, 19, &darkMode, sizeof(darkMode));

            ctx->hBrushBg = CreateSolidBrush(CLR_BG_EVEN);
            UINT dpi = GetDlgDpi(hWnd);

            int fontSize = ScaleDpi(-14, dpi);
            int headerSize = ScaleDpi(-13, dpi);

            ctx->hFont = CreateFontW(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            ctx->hFontBold = CreateFontW(fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            ctx->hFontHeader = CreateFontW(headerSize, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            // Clean English Header
            std::wstring headerText = L"Leftover traces found for: " + ctx->pApp->displayName +
                L"\nReview and select items to remove permanently from registry and filesystem:";

            ctx->hStaticInfo = CreateWindowExW(0, L"STATIC", headerText.c_str(),
                WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)IDC_STATIC_INFO, GetModuleHandle(nullptr), nullptr);

            // ListView
            ctx->hListView = CreateWindowExW(0, WC_LISTVIEWW, L"",
                WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_VSCROLL,
                0, 0, 0, 0, hWnd, (HMENU)IDC_LEFTOVERS_LIST, GetModuleHandle(nullptr), nullptr);

            ListView_SetExtendedListViewStyle(ctx->hListView, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
            SetWindowTheme(ctx->hListView, L"DarkMode_Explorer", nullptr);
            ctx->hHeader = ListView_GetHeader(ctx->hListView);

            // Columns
            LVCOLUMNW col = { 0 };
            col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

            col.cx = ScaleDpi(160, dpi);
            col.pszText = const_cast<LPWSTR>(L"Type");
            ListView_InsertColumn(ctx->hListView, 0, &col);

            col.cx = ScaleDpi(90, dpi);
            col.pszText = const_cast<LPWSTR>(L"Size");
            ListView_InsertColumn(ctx->hListView, 1, &col);

            col.cx = ScaleDpi(560, dpi);
            col.pszText = const_cast<LPWSTR>(L"Item / Registry Path");
            ListView_InsertColumn(ctx->hListView, 2, &col);

            // Owner-drawn Dark Buttons
            ctx->hBtnSelectAll = CreateWindowExW(0, L"BUTTON", L"Select All",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_SELECT_ALL, GetModuleHandle(nullptr), nullptr);

            ctx->hBtnDeselectAll = CreateWindowExW(0, L"BUTTON", L"Deselect All",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_DESELECT_ALL, GetModuleHandle(nullptr), nullptr);

            ctx->hBtnDelete = CreateWindowExW(0, L"BUTTON", L"Delete Selected",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_DELETE_SELECTED, GetModuleHandle(nullptr), nullptr);

            ctx->hBtnClose = CreateWindowExW(0, L"BUTTON", L"Close",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_CLOSE_DLG, GetModuleHandle(nullptr), nullptr);

            SendMessage(ctx->hStaticInfo, WM_SETFONT, (WPARAM)ctx->hFont, TRUE);
            SendMessage(ctx->hListView, WM_SETFONT, (WPARAM)ctx->hFont, TRUE);
            if (ctx->hHeader) SendMessage(ctx->hHeader, WM_SETFONT, (WPARAM)ctx->hFontHeader, TRUE);

            PopulateListView(ctx);
            return 0;
        }

        case WM_CTLCOLORDLG:
            return (LRESULT)ctx->hBrushBg;

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, CLR_TEXT_NORMAL);
            return (LRESULT)ctx->hBrushBg;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* pDIS = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            DrawDarkButton(ctx, pDIS);
            return TRUE;
        }

        case WM_NOTIFY: {
            LPNMHDR pnm = reinterpret_cast<LPNMHDR>(lParam);

            if (pnm->idFrom == IDC_LEFTOVERS_LIST && pnm->code == NM_CUSTOMDRAW) {
                return HandleLeftoversCustomDraw(ctx, lParam);
            }

            if (pnm->hwndFrom == ctx->hHeader && pnm->code == NM_CUSTOMDRAW) {
                return HandleHeaderCustomDraw(ctx, lParam);
            }
            break;
        }

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            LayoutDialogControls(ctx, width, height);
            return 0;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
            case IDC_BTN_SELECT_ALL: {
                int count = ListView_GetItemCount(ctx->hListView);
                for (int i = 0; i < count; ++i) {
                    ListView_SetCheckState(ctx->hListView, i, TRUE);
                }
                InvalidateRect(ctx->hListView, nullptr, FALSE);
                return 0;
            }
            case IDC_BTN_DESELECT_ALL: {
                int count = ListView_GetItemCount(ctx->hListView);
                for (int i = 0; i < count; ++i) {
                    ListView_SetCheckState(ctx->hListView, i, FALSE);
                }
                InvalidateRect(ctx->hListView, nullptr, FALSE);
                return 0;
            }
            case IDC_BTN_DELETE_SELECTED: {
                int count = ListView_GetItemCount(ctx->hListView);
                int selectedCount = 0;
                for (int i = 0; i < count; ++i) {
                    bool checked = (ListView_GetCheckState(ctx->hListView, i) != 0);
                    (*ctx->pItems)[i].selected = checked;
                    if (checked) selectedCount++;
                }

                if (selectedCount == 0) {
                    MessageBoxW(hWnd, L"Please select at least one leftover item to delete.", L"Notice", MB_OK | MB_ICONWARNING);
                    return 0;
                }

                std::wstring confirmMsg = L"Are you sure you want to delete the (" + std::to_wstring(selectedCount) +
                    L") selected item(s)?\n\n(Files will be sent to the Recycle Bin for safe recovery).";

                if (MessageBoxW(hWnd, confirmMsg.c_str(), L"Confirm Removal", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) {
                    return 0;
                }

                std::vector<std::wstring> errors;
                Cleaner::DeleteSelectedItems(*ctx->pItems, errors);
                ctx->deletedAny = true;

                if (!errors.empty()) {
                    std::wstring errText = L"Deletion finished with some errors:\n";
                    for (const auto& err : errors) {
                        errText += L"• " + err + L"\n";
                    }
                    MessageBoxW(hWnd, errText.c_str(), L"Removal Report", MB_OK | MB_ICONWARNING);
                } else {
                    MessageBoxW(hWnd, L"All selected traces have been successfully removed!", L"Success", MB_OK | MB_ICONINFORMATION);
                }

                std::vector<LeftoverItem> remaining;
                for (const auto& item : *ctx->pItems) {
                    if (!item.selected) {
                        remaining.push_back(item);
                    }
                }
                *ctx->pItems = remaining;
                PopulateListView(ctx);

                if (ctx->pItems->empty()) {
                    ctx->isDialogActive = false;
                    DestroyWindow(hWnd);
                }
                return 0;
            }
            case IDC_BTN_CLOSE_DLG:
            case IDCANCEL:
                ctx->isDialogActive = false;
                DestroyWindow(hWnd);
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            ctx->isDialogActive = false;
            DestroyWindow(hWnd);
            return 0;

        case WM_DESTROY:
            if (ctx) {
                if (ctx->hFont) DeleteObject(ctx->hFont);
                if (ctx->hFontBold) DeleteObject(ctx->hFontBold);
                if (ctx->hFontHeader) DeleteObject(ctx->hFontHeader);
                if (ctx->hBrushBg) DeleteObject(ctx->hBrushBg);
                ctx->isDialogActive = false;
            }
            return 0;
        }

        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

bool LeftoversDialog::Show(HWND hWndParent, const InstalledApp& app, std::vector<LeftoverItem>& leftovers) {
    if (leftovers.empty()) {
        MessageBoxW(hWndParent, L"No leftover files or registry keys were found for this program.",
                    L"Scan Complete", MB_OK | MB_ICONINFORMATION);
        return false;
    }

    const wchar_t* CLASS_NAME = L"ZeroLeftoversWndClass";

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = LeftoversWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(CLR_BG_EVEN);

    RegisterClassExW(&wc);

    DialogContext ctx;
    ctx.pApp = &app;
    ctx.pItems = &leftovers;
    ctx.isDialogActive = true;

    UINT screenDpi = GetDlgDpi(hWndParent);
    int dlgWidth = ScaleDpi(860, screenDpi);
    int dlgHeight = ScaleDpi(540, screenDpi);

    RECT rcParent;
    GetWindowRect(hWndParent, &rcParent);
    int posX = rcParent.left + (rcParent.right - rcParent.left - dlgWidth) / 2;
    int posY = rcParent.top + (rcParent.bottom - rcParent.top - dlgHeight) / 2;
    if (posX < 0) posX = 100;
    if (posY < 0) posY = 100;

    std::wstring winTitle = L"Leftover Traces: " + app.displayName;

    EnableWindow(hWndParent, FALSE);

    HWND hWndDlg = CreateWindowExW(
        0,
        CLASS_NAME,
        winTitle.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VISIBLE,
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

    return ctx.deletedAny;
}
