#include "updater.h"
#include "resource.h"
#include <shellapi.h>
#include <wininet.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <thread>
#include <atomic>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cwctype>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shlwapi.lib")

namespace {
    const wchar_t* CURRENT_VERSION_STR = L"0.0.2-beta";
    const wchar_t* GITHUB_REPO_URL      = L"https://github.com/hamzabellouch/zero";
    const wchar_t* GITHUB_RELEASES_URL  = L"https://github.com/hamzabellouch/zero/releases";
    const wchar_t* GITHUB_API_URL       = L"https://api.github.com/repos/hamzabellouch/zero/releases/latest";

    // Midnight Navy Theme Palette
    const COLORREF CLR_BG           = RGB(20, 26, 36);     // #141A24 Deep Midnight Navy
    const COLORREF CLR_CARD_BG      = RGB(26, 33, 45);     // #1A212D Exact Title Bar Match
    const COLORREF CLR_BORDER       = RGB(40, 53, 72);     // #283548 Soft Midnight Border
    const COLORREF CLR_ACCENT       = RGB(0, 120, 215);    // #0078D7 Windows Accent Blue
    const COLORREF CLR_ACCENT_HOVER = RGB(24, 140, 235);
    const COLORREF CLR_ACCENT_PRESS = RGB(0, 95, 175);
    const COLORREF CLR_TEXT_NORMAL  = RGB(226, 232, 240);  // #E2E8F0 Clean Silver White
    const COLORREF CLR_TEXT_MUTED   = RGB(141, 155, 176);  // #8D9BB0 Slate
    const COLORREF CLR_BTN_BG       = RGB(34, 44, 61);     // #222C3D
    const COLORREF CLR_BTN_HOVER    = RGB(44, 58, 80);
    const COLORREF CLR_BTN_BORDER   = RGB(55, 70, 95);     // #37465F
    const COLORREF CLR_BTN_PRESSED  = RGB(20, 26, 36);     // #141A24
    const COLORREF CLR_BAR_BG       = RGB(20, 26, 36);

    const UINT WM_APP_CHECK_FINISHED    = WM_APP + 201;
    const UINT WM_APP_DOWNLOAD_PROGRESS = WM_APP + 202;
    const UINT WM_APP_DOWNLOAD_FINISHED = WM_APP + 203;

    const int IDC_BTN_ACTION_PRIMARY    = 6001;
    const int IDC_BTN_ACTION_SECONDARY  = 6002;
    const int IDC_BTN_ACTION_CANCEL     = 6003;

    enum class UpdateState {
        Checking,
        UpToDate,
        UpdateAvailable,
        Downloading,
        Restarting,
        Error
    };

    struct AssetInfo {
        std::wstring name;
        std::wstring downloadUrl;
    };

    struct SemVer {
        int major = 0;
        int minor = 0;
        int patch = 0;
        int build = 0;
        bool isPrerelease = false;
        int prereleaseNum = 0;
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
        return dpi ? dpi : 96;
    }

    int ScaleDpi(int value, UINT dpi) {
        return MulDiv(value, dpi, 96);
    }

    bool EndsWithIgnoreCase(const std::wstring& str, const std::wstring& suffix) {
        if (str.length() < suffix.length()) return false;
        std::wstring s1 = str.substr(str.length() - suffix.length());
        return _wcsicmp(s1.c_str(), suffix.c_str()) == 0;
    }

    bool ContainsIgnoreCase(const std::wstring& str, const std::wstring& sub) {
        auto it = std::search(
            str.begin(), str.end(),
            sub.begin(), sub.end(),
            [](wchar_t ch1, wchar_t ch2) { return towlower(ch1) == towlower(ch2); }
        );
        return (it != str.end());
    }

    SemVer ParseVersion(const std::wstring& str) {
        SemVer v;
        size_t i = 0;
        while (i < str.length() && (str[i] == L' ' || str[i] == L'v' || str[i] == L'V')) i++;

        while (i < str.length() && iswdigit(str[i])) {
            v.major = v.major * 10 + (str[i] - L'0');
            i++;
        }
        if (i < str.length() && str[i] == L'.') i++;

        while (i < str.length() && iswdigit(str[i])) {
            v.minor = v.minor * 10 + (str[i] - L'0');
            i++;
        }
        if (i < str.length() && str[i] == L'.') i++;

        while (i < str.length() && iswdigit(str[i])) {
            v.patch = v.patch * 10 + (str[i] - L'0');
            i++;
        }
        if (i < str.length() && str[i] == L'.') i++;

        while (i < str.length() && iswdigit(str[i])) {
            v.build = v.build * 10 + (str[i] - L'0');
            i++;
        }

        if (i < str.length() && (str[i] == L'-' || str[i] == L'.')) {
            v.isPrerelease = true;
            while (i < str.length()) {
                if (iswdigit(str[i])) {
                    v.prereleaseNum = v.prereleaseNum * 10 + (str[i] - L'0');
                }
                i++;
            }
        }
        return v;
    }

    bool IsRemoteNewer(const SemVer& remote, const SemVer& local) {
        if (remote.major != local.major) return remote.major > local.major;
        if (remote.minor != local.minor) return remote.minor > local.minor;
        if (remote.patch != local.patch) return remote.patch > local.patch;
        if (remote.build != local.build) return remote.build > local.build;

        // If versions are identical numerically:
        // Local is prerelease (0.0.2-beta) and remote is stable (0.0.2) -> remote is newer!
        if (local.isPrerelease && !remote.isPrerelease) return true;
        if (!local.isPrerelease && remote.isPrerelease) return false;

        // Both are prereleases: compare prerelease number
        if (local.isPrerelease && remote.isPrerelease) {
            return remote.prereleaseNum > local.prereleaseNum;
        }

        return false;
    }

    std::wstring UnescapeJsonString(const std::wstring& in) {
        std::wstring out;
        out.reserve(in.length());
        for (size_t i = 0; i < in.length(); ++i) {
            if (in[i] == L'\\' && i + 1 < in.length()) {
                wchar_t c = in[++i];
                if (c == L'"') out.push_back(L'"');
                else if (c == L'\\') out.push_back(L'\\');
                else if (c == L'/') out.push_back(L'/');
                else if (c == L'b') out.push_back(L'\b');
                else if (c == L'f') out.push_back(L'\f');
                else if (c == L'n') out.push_back(L'\n');
                else if (c == L'r') out.push_back(L'\r');
                else if (c == L't') out.push_back(L'\t');
                else if (c == L'u' && i + 4 < in.length()) {
                    wchar_t hex[5] = { in[i+1], in[i+2], in[i+3], in[i+4], 0 };
                    wchar_t* end = nullptr;
                    long val = wcstol(hex, &end, 16);
                    if (end != hex) {
                        out.push_back((wchar_t)val);
                        i += 4;
                    } else {
                        out.push_back(c);
                    }
                } else {
                    out.push_back(c);
                }
            } else {
                out.push_back(in[i]);
            }
        }
        return out;
    }

    std::wstring ExtractJsonField(const std::wstring& json, const std::wstring& key) {
        std::wstring searchKey = L"\"" + key + L"\"";
        size_t pos = json.find(searchKey);
        if (pos == std::wstring::npos) return L"";
        pos += searchKey.length();
        while (pos < json.length() && (json[pos] == L' ' || json[pos] == L':' || json[pos] == L'\t' || json[pos] == L'\r' || json[pos] == L'\n')) {
            pos++;
        }
        if (pos >= json.length() || json[pos] != L'"') return L"";
        pos++; // skip opening quote
        size_t start = pos;
        bool inEscape = false;
        while (pos < json.length()) {
            if (inEscape) {
                inEscape = false;
            } else if (json[pos] == L'\\') {
                inEscape = true;
            } else if (json[pos] == L'"') {
                break;
            }
            pos++;
        }
        std::wstring raw = json.substr(start, pos - start);
        return UnescapeJsonString(raw);
    }

    std::vector<AssetInfo> ExtractAssets(const std::wstring& json) {
        std::vector<AssetInfo> result;
        size_t assetsPos = json.find(L"\"assets\"");
        if (assetsPos == std::wstring::npos) return result;
        size_t openBracket = json.find(L'[', assetsPos);
        if (openBracket == std::wstring::npos) return result;

        size_t closeBracket = std::wstring::npos;
        int depth = 0;
        for (size_t i = openBracket; i < json.length(); ++i) {
            if (json[i] == L'[') depth++;
            else if (json[i] == L']') {
                depth--;
                if (depth == 0) {
                    closeBracket = i;
                    break;
                }
            }
        }
        if (closeBracket == std::wstring::npos) return result;

        std::wstring assetsJson = json.substr(openBracket, closeBracket - openBracket + 1);

        size_t pos = 0;
        while ((pos = assetsJson.find(L'{', pos)) != std::wstring::npos) {
            size_t objEnd = std::wstring::npos;
            int objDepth = 0;
            for (size_t i = pos; i < assetsJson.length(); ++i) {
                if (assetsJson[i] == L'{') objDepth++;
                else if (assetsJson[i] == L'}') {
                    objDepth--;
                    if (objDepth == 0) {
                        objEnd = i;
                        break;
                    }
                }
            }
            if (objEnd == std::wstring::npos) break;

            std::wstring objJson = assetsJson.substr(pos, objEnd - pos + 1);
            std::wstring name = ExtractJsonField(objJson, L"name");
            std::wstring downloadUrl = ExtractJsonField(objJson, L"browser_download_url");

            if (!name.empty() && !downloadUrl.empty()) {
                AssetInfo info;
                info.name = name;
                info.downloadUrl = downloadUrl;
                result.push_back(info);
            }

            pos = objEnd + 1;
        }

        return result;
    }

    bool RunHiddenProcess(const std::wstring& cmd) {
        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = { 0 };
        std::vector<wchar_t> buf(cmd.begin(), cmd.end());
        buf.push_back(L'\0');

        if (CreateProcessW(NULL, buf.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 30000);
            DWORD exitCode = 1;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return (exitCode == 0);
        }
        return false;
    }

    struct UpdateContext {
        HWND hWnd = nullptr;
        HWND hBtnPrimary = nullptr;
        HWND hBtnSecondary = nullptr;
        HWND hBtnCancel = nullptr;

        HFONT hFontTitle = nullptr;
        HFONT hFontSubtitle = nullptr;
        HFONT hFontBody = nullptr;
        HFONT hFontBold = nullptr;
        HFONT hFontSmall = nullptr;
        HBRUSH hBrushBg = nullptr;

        UpdateState state = UpdateState::Checking;
        ReleaseInfo releaseInfo;

        std::atomic<bool> cancelToken{false};
        std::atomic<bool> isDialogActive{true};

        int downloadPercent = 0;
        size_t downloadedBytes = 0;
        size_t totalBytes = 0;
        std::wstring downloadStatus;

        std::thread checkThread;
        std::thread downloadThread;
    };

    void DrawDarkCapsuleButton(HDC hdc, RECT rc, const wchar_t* text, bool isHovered, bool isPressed, bool isAccent, HFONT hFont) {
        HBRUSH hBrParent = CreateSolidBrush(CLR_BG);
        FillRect(hdc, &rc, hBrParent);
        DeleteObject(hBrParent);

        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        float x = (float)rc.left + 1.0f;
        float y = (float)rc.top + 1.0f;
        float w = (float)(rc.right - rc.left) - 2.0f;
        float h = (float)(rc.bottom - rc.top) - 2.0f;
        float r = h;

        Gdiplus::GraphicsPath path;
        path.AddArc(x, y, r, r, 90, 180);
        path.AddArc(x + w - r, y, r, r, 270, 180);
        path.CloseFigure();

        Gdiplus::Color fillCol;
        Gdiplus::Color borderCol;

        if (isAccent) {
            if (isPressed) {
                fillCol = Gdiplus::Color(255, GetRValue(CLR_ACCENT_PRESS), GetGValue(CLR_ACCENT_PRESS), GetBValue(CLR_ACCENT_PRESS));
                borderCol = Gdiplus::Color(255, 0, 120, 215);
            } else if (isHovered) {
                fillCol = Gdiplus::Color(255, GetRValue(CLR_ACCENT_HOVER), GetGValue(CLR_ACCENT_HOVER), GetBValue(CLR_ACCENT_HOVER));
                borderCol = Gdiplus::Color(255, 70, 175, 255);
            } else {
                fillCol = Gdiplus::Color(255, GetRValue(CLR_ACCENT), GetGValue(CLR_ACCENT), GetBValue(CLR_ACCENT));
                borderCol = Gdiplus::Color(255, 40, 150, 255);
            }
        } else {
            if (isPressed) {
                fillCol = Gdiplus::Color(255, GetRValue(CLR_BTN_PRESSED), GetGValue(CLR_BTN_PRESSED), GetBValue(CLR_BTN_PRESSED));
                borderCol = Gdiplus::Color(255, 40, 53, 72);
            } else if (isHovered) {
                fillCol = Gdiplus::Color(255, GetRValue(CLR_BTN_HOVER), GetGValue(CLR_BTN_HOVER), GetBValue(CLR_BTN_HOVER));
                borderCol = Gdiplus::Color(255, 70, 90, 120);
            } else {
                fillCol = Gdiplus::Color(255, GetRValue(CLR_BTN_BG), GetGValue(CLR_BTN_BG), GetBValue(CLR_BTN_BG));
                borderCol = Gdiplus::Color(255, GetRValue(CLR_BTN_BORDER), GetGValue(CLR_BTN_BORDER), GetBValue(CLR_BTN_BORDER));
            }
        }

        Gdiplus::SolidBrush br(fillCol);
        g.FillPath(&br, &path);

        Gdiplus::Pen pen(borderCol, 1.2f);
        g.DrawPath(&pen, &path);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        HFONT hOld = (HFONT)SelectObject(hdc, hFont);
        DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, hOld);
    }

    void LayoutControls(UpdateContext* ctx) {
        if (!ctx || !ctx->hWnd) return;

        RECT rcClient;
        GetClientRect(ctx->hWnd, &rcClient);
        int width = rcClient.right;
        int height = rcClient.bottom;

        UINT dpi = GetDlgDpi(ctx->hWnd);
        int margin = ScaleDpi(20, dpi);
        int btnHeight = ScaleDpi(32, dpi);
        int btnY = height - margin - btnHeight;

        switch (ctx->state) {
        case UpdateState::Checking: {
            int btnW = ScaleDpi(110, dpi);
            ShowWindow(ctx->hBtnPrimary, SW_HIDE);
            ShowWindow(ctx->hBtnSecondary, SW_HIDE);
            SetWindowPos(ctx->hBtnCancel, nullptr, width - margin - btnW, btnY, btnW, btnHeight, SWP_NOZORDER | SWP_SHOWWINDOW);
            SetWindowTextW(ctx->hBtnCancel, L"Cancel");
            break;
        }
        case UpdateState::UpToDate: {
            int btnW1 = ScaleDpi(130, dpi);
            int btnW2 = ScaleDpi(90, dpi);
            ShowWindow(ctx->hBtnCancel, SW_HIDE);

            SetWindowPos(ctx->hBtnPrimary, nullptr, width - margin - btnW2, btnY, btnW2, btnHeight, SWP_NOZORDER | SWP_SHOWWINDOW);
            SetWindowTextW(ctx->hBtnPrimary, L"OK");

            SetWindowPos(ctx->hBtnSecondary, nullptr, width - margin - btnW2 - ScaleDpi(10, dpi) - btnW1, btnY, btnW1, btnHeight, SWP_NOZORDER | SWP_SHOWWINDOW);
            SetWindowTextW(ctx->hBtnSecondary, L"Go to GitHub");
            break;
        }
        case UpdateState::UpdateAvailable: {
            if (!ctx->releaseInfo.downloadUrl.empty()) {
                int btnW1 = ScaleDpi(145, dpi); // Update In-Place
                int btnW2 = ScaleDpi(120, dpi); // Go to GitHub
                int btnW3 = ScaleDpi(90, dpi);  // Later

                int x = width - margin;

                x -= btnW3;
                SetWindowPos(ctx->hBtnCancel, nullptr, x, btnY, btnW3, btnHeight, SWP_NOZORDER | SWP_SHOWWINDOW);
                SetWindowTextW(ctx->hBtnCancel, L"Later");

                x -= (btnW2 + ScaleDpi(10, dpi));
                SetWindowPos(ctx->hBtnSecondary, nullptr, x, btnY, btnW2, btnHeight, SWP_NOZORDER | SWP_SHOWWINDOW);
                SetWindowTextW(ctx->hBtnSecondary, L"Go to GitHub");

                x -= (btnW1 + ScaleDpi(10, dpi));
                SetWindowPos(ctx->hBtnPrimary, nullptr, x, btnY, btnW1, btnHeight, SWP_NOZORDER | SWP_SHOWWINDOW);
                SetWindowTextW(ctx->hBtnPrimary, L"Update In-Place");
            } else {
                int btnW1 = ScaleDpi(130, dpi); // Go to GitHub
                int btnW2 = ScaleDpi(90, dpi);  // Close

                ShowWindow(ctx->hBtnCancel, SW_HIDE);

                SetWindowPos(ctx->hBtnSecondary, nullptr, width - margin - btnW2 - ScaleDpi(10, dpi) - btnW1, btnY, btnW1, btnHeight, SWP_NOZORDER | SWP_SHOWWINDOW);
                SetWindowTextW(ctx->hBtnSecondary, L"Go to GitHub");

                SetWindowPos(ctx->hBtnPrimary, nullptr, width - margin - btnW2, btnY, btnW2, btnHeight, SWP_NOZORDER | SWP_SHOWWINDOW);
                SetWindowTextW(ctx->hBtnPrimary, L"Close");
            }
            break;
        }
        case UpdateState::Downloading: {
            int btnW = ScaleDpi(110, dpi);
            ShowWindow(ctx->hBtnPrimary, SW_HIDE);
            ShowWindow(ctx->hBtnSecondary, SW_HIDE);
            SetWindowPos(ctx->hBtnCancel, nullptr, width - margin - btnW, btnY, btnW, btnHeight, SWP_NOZORDER | SWP_SHOWWINDOW);
            SetWindowTextW(ctx->hBtnCancel, L"Cancel");
            break;
        }
        case UpdateState::Restarting: {
            ShowWindow(ctx->hBtnPrimary, SW_HIDE);
            ShowWindow(ctx->hBtnSecondary, SW_HIDE);
            ShowWindow(ctx->hBtnCancel, SW_HIDE);
            break;
        }
        case UpdateState::Error: {
            int btnW1 = ScaleDpi(130, dpi);
            int btnW2 = ScaleDpi(90, dpi);
            ShowWindow(ctx->hBtnCancel, SW_HIDE);

            SetWindowPos(ctx->hBtnSecondary, nullptr, width - margin - btnW2 - ScaleDpi(10, dpi) - btnW1, btnY, btnW1, btnHeight, SWP_NOZORDER | SWP_SHOWWINDOW);
            SetWindowTextW(ctx->hBtnSecondary, L"Go to GitHub");

            SetWindowPos(ctx->hBtnPrimary, nullptr, width - margin - btnW2, btnY, btnW2, btnHeight, SWP_NOZORDER | SWP_SHOWWINDOW);
            SetWindowTextW(ctx->hBtnPrimary, L"Close");
            break;
        }
        }
    }

    void CheckThreadProc(UpdateContext* ctx) {
        HINTERNET hInternet = InternetOpenW(L"Zero-Updater/0.0.2", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (!hInternet) {
            ctx->releaseInfo.checkSucceeded = false;
            ctx->releaseInfo.errorMessage = L"Failed to initialize network interface.";
            PostMessageW(ctx->hWnd, WM_APP_CHECK_FINISHED, 0, 0);
            return;
        }

        DWORD timeoutMs = 8000;
        InternetSetOptionW(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
        InternetSetOptionW(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE;
        HINTERNET hUrl = InternetOpenUrlW(
            hInternet,
            GITHUB_API_URL,
            L"User-Agent: Zero-Updater/0.0.2\r\nAccept: application/vnd.github.v3+json\r\n",
            -1,
            flags,
            0
        );

        if (!hUrl) {
            InternetCloseHandle(hInternet);
            ctx->releaseInfo.checkSucceeded = false;
            ctx->releaseInfo.errorMessage = L"Unable to connect to GitHub. Please check your internet connection.";
            PostMessageW(ctx->hWnd, WM_APP_CHECK_FINISHED, 0, 0);
            return;
        }

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        HttpQueryInfoW(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusSize, NULL);

        if (statusCode == 404) {
            // No releases published yet
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            ctx->releaseInfo.checkSucceeded = true;
            ctx->releaseInfo.isUpdateAvailable = false;
            PostMessageW(ctx->hWnd, WM_APP_CHECK_FINISHED, 0, 0);
            return;
        }

        if (statusCode != 200) {
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            ctx->releaseInfo.checkSucceeded = false;
            ctx->releaseInfo.errorMessage = L"GitHub API responded with code: " + std::to_wstring(statusCode);
            PostMessageW(ctx->hWnd, WM_APP_CHECK_FINISHED, 0, 0);
            return;
        }

        std::string rawJson;
        char buffer[4096];
        DWORD bytesRead = 0;
        while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
            rawJson.append(buffer, bytesRead);
        }

        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);

        if (rawJson.empty()) {
            ctx->releaseInfo.checkSucceeded = false;
            ctx->releaseInfo.errorMessage = L"Empty response received from GitHub.";
            PostMessageW(ctx->hWnd, WM_APP_CHECK_FINISHED, 0, 0);
            return;
        }

        int wlen = MultiByteToWideChar(CP_UTF8, 0, rawJson.c_str(), -1, nullptr, 0);
        std::wstring json(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, rawJson.c_str(), -1, &json[0], wlen);

        std::wstring tagName = ExtractJsonField(json, L"tag_name");
        std::wstring releaseTitle = ExtractJsonField(json, L"name");
        std::wstring htmlUrl = ExtractJsonField(json, L"html_url");
        std::wstring body = ExtractJsonField(json, L"body");

        ctx->releaseInfo.tagName = tagName;
        ctx->releaseInfo.releaseTitle = releaseTitle;
        ctx->releaseInfo.htmlUrl = htmlUrl.empty() ? GITHUB_RELEASES_URL : htmlUrl;
        ctx->releaseInfo.releaseNotes = body;
        ctx->releaseInfo.checkSucceeded = true;

        std::vector<AssetInfo> assets = ExtractAssets(json);

        // Find best asset for Windows
        // 1. Direct .exe
        for (const auto& a : assets) {
            if (EndsWithIgnoreCase(a.name, L".exe")) {
                ctx->releaseInfo.downloadUrl = a.downloadUrl;
                ctx->releaseInfo.assetName = a.name;
                break;
            }
        }
        // 2. Windows .zip
        if (ctx->releaseInfo.downloadUrl.empty()) {
            for (const auto& a : assets) {
                if (ContainsIgnoreCase(a.name, L"windows") && EndsWithIgnoreCase(a.name, L".zip")) {
                    ctx->releaseInfo.downloadUrl = a.downloadUrl;
                    ctx->releaseInfo.assetName = a.name;
                    break;
                }
            }
        }
        // 3. Fallback: Any .zip
        if (ctx->releaseInfo.downloadUrl.empty()) {
            for (const auto& a : assets) {
                if (EndsWithIgnoreCase(a.name, L".zip")) {
                    ctx->releaseInfo.downloadUrl = a.downloadUrl;
                    ctx->releaseInfo.assetName = a.name;
                    break;
                }
            }
        }

        SemVer remoteVer = ParseVersion(tagName);
        SemVer localVer = ParseVersion(CURRENT_VERSION_STR);

        ctx->releaseInfo.isUpdateAvailable = IsRemoteNewer(remoteVer, localVer);

        PostMessageW(ctx->hWnd, WM_APP_CHECK_FINISHED, 0, 0);
    }

    void DownloadThreadProc(UpdateContext* ctx) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);

        wchar_t dirPath[MAX_PATH];
        wcscpy_s(dirPath, exePath);
        PathRemoveFileSpecW(dirPath);

        std::wstring tmpFile = std::wstring(dirPath) + L"\\Zero.download.tmp";
        std::wstring newExePath = std::wstring(dirPath) + L"\\Zero.new.exe";

        DeleteFileW(tmpFile.c_str());
        DeleteFileW(newExePath.c_str());

        HINTERNET hInternet = InternetOpenW(L"Zero-Updater/0.0.2", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (!hInternet) {
            ctx->state = UpdateState::Error;
            ctx->releaseInfo.errorMessage = L"Failed to initiate download session.";
            PostMessageW(ctx->hWnd, WM_APP_DOWNLOAD_FINISHED, FALSE, 0);
            return;
        }

        std::wstring currentUrl = ctx->releaseInfo.downloadUrl;
        HINTERNET hUrl = NULL;

        for (int redirectCount = 0; redirectCount < 5; ++redirectCount) {
            hUrl = InternetOpenUrlW(hInternet, currentUrl.c_str(),
                                    L"User-Agent: Zero-Updater/0.0.2\r\n", -1,
                                    INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE, 0);
            if (!hUrl) break;

            DWORD statusCode = 0;
            DWORD statusSize = sizeof(statusCode);
            if (HttpQueryInfoW(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusSize, NULL)) {
                if (statusCode == 301 || statusCode == 302 || statusCode == 307 || statusCode == 308) {
                    wchar_t redirectLocation[2048] = { 0 };
                    DWORD locSize = sizeof(redirectLocation);
                    if (HttpQueryInfoW(hUrl, HTTP_QUERY_LOCATION, redirectLocation, &locSize, NULL)) {
                        InternetCloseHandle(hUrl);
                        hUrl = NULL;
                        currentUrl = redirectLocation;
                        continue;
                    }
                }
            }
            break;
        }

        if (!hUrl) {
            InternetCloseHandle(hInternet);
            ctx->state = UpdateState::Error;
            ctx->releaseInfo.errorMessage = L"Failed to open download stream from GitHub.";
            PostMessageW(ctx->hWnd, WM_APP_DOWNLOAD_FINISHED, FALSE, 0);
            return;
        }

        DWORD totalSize = 0;
        DWORD querySize = sizeof(totalSize);
        HttpQueryInfoW(hUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &totalSize, &querySize, NULL);
        ctx->totalBytes = totalSize;

        HANDLE hFile = CreateFileW(tmpFile.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            ctx->state = UpdateState::Error;
            ctx->releaseInfo.errorMessage = L"Failed to create temporary file for update.";
            PostMessageW(ctx->hWnd, WM_APP_DOWNLOAD_FINISHED, FALSE, 0);
            return;
        }

        char buffer[32768];
        DWORD bytesRead = 0;
        DWORD totalDownloaded = 0;

        while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
            if (ctx->cancelToken.load()) {
                CloseHandle(hFile);
                DeleteFileW(tmpFile.c_str());
                InternetCloseHandle(hUrl);
                InternetCloseHandle(hInternet);
                return;
            }

            DWORD written = 0;
            WriteFile(hFile, buffer, bytesRead, &written, NULL);
            totalDownloaded += bytesRead;
            ctx->downloadedBytes = totalDownloaded;

            int pct = (totalSize > 0) ? (int)((totalDownloaded * 100ULL) / totalSize) : 0;
            ctx->downloadPercent = pct;

            PostMessageW(ctx->hWnd, WM_APP_DOWNLOAD_PROGRESS, (WPARAM)pct, 0);
        }

        CloseHandle(hFile);
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);

        if (ctx->cancelToken.load()) {
            DeleteFileW(tmpFile.c_str());
            return;
        }

        // Handle Zip vs Direct Exe
        bool isZip = EndsWithIgnoreCase(ctx->releaseInfo.assetName, L".zip");
        if (isZip) {
            std::wstring stagingDir = std::wstring(dirPath) + L"\\Zero_staging";
            CreateDirectoryW(stagingDir.c_str(), NULL);

            std::wstring tarCmd = L"tar.exe -xf \"" + tmpFile + L"\" -C \"" + stagingDir + L"\"";
            bool ok = RunHiddenProcess(tarCmd);

            if (!ok) {
                std::wstring psCmd = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"Expand-Archive -LiteralPath '" +
                                     tmpFile + L"' -DestinationPath '" + stagingDir + L"' -Force\"";
                RunHiddenProcess(psCmd);
            }

            std::wstring extractedExe = stagingDir + L"\\Zero.exe";
            if (GetFileAttributesW(extractedExe.c_str()) != INVALID_FILE_ATTRIBUTES) {
                MoveFileExW(extractedExe.c_str(), newExePath.c_str(), MOVEFILE_REPLACE_EXISTING);
            } else {
                WIN32_FIND_DATAW fd;
                HANDLE hFind = FindFirstFileW((stagingDir + L"\\*.exe").c_str(), &fd);
                if (hFind != INVALID_HANDLE_VALUE) {
                    MoveFileExW((stagingDir + L"\\" + fd.cFileName).c_str(), newExePath.c_str(), MOVEFILE_REPLACE_EXISTING);
                    FindClose(hFind);
                }
            }

            DeleteFileW(tmpFile.c_str());
            RemoveDirectoryW(stagingDir.c_str());
        } else {
            MoveFileExW(tmpFile.c_str(), newExePath.c_str(), MOVEFILE_REPLACE_EXISTING);
        }

        if (GetFileAttributesW(newExePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            ctx->state = UpdateState::Error;
            ctx->releaseInfo.errorMessage = L"Failed to extract new Zero executable.";
            PostMessageW(ctx->hWnd, WM_APP_DOWNLOAD_FINISHED, FALSE, 0);
            return;
        }

        // Success! Ready to swap
        PostMessageW(ctx->hWnd, WM_APP_DOWNLOAD_FINISHED, TRUE, 0);
    }

    LRESULT CALLBACK UpdateWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        UpdateContext* ctx = reinterpret_cast<UpdateContext*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

        switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* pcs = reinterpret_cast<CREATESTRUCT*>(lParam);
            ctx = reinterpret_cast<UpdateContext*>(pcs->lpCreateParams);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx));
            ctx->hWnd = hWnd;

            // Dark Title Bar
            BOOL darkMode = TRUE;
            DwmSetWindowAttribute(hWnd, 20, &darkMode, sizeof(darkMode));
            DwmSetWindowAttribute(hWnd, 19, &darkMode, sizeof(darkMode));

            // Icons
            HICON hBigIcon = LoadIconW(GetModuleHandle(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON));
            HICON hSmallIcon = (HICON)LoadImageW(GetModuleHandle(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
                                                 GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
            if (hBigIcon) SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hBigIcon);
            if (hSmallIcon) SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hSmallIcon);

            ctx->hBrushBg = CreateSolidBrush(CLR_BG);
            UINT dpi = GetDlgDpi(hWnd);

            ctx->hFontTitle = CreateFontW(ScaleDpi(-18, dpi), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            ctx->hFontSubtitle = CreateFontW(ScaleDpi(-13, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            ctx->hFontBody = CreateFontW(ScaleDpi(-14, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            ctx->hFontBold = CreateFontW(ScaleDpi(-14, dpi), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            ctx->hFontSmall = CreateFontW(ScaleDpi(-12, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            // Buttons
            ctx->hBtnPrimary = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                               0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_ACTION_PRIMARY, GetModuleHandle(nullptr), nullptr);

            ctx->hBtnSecondary = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                                 0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_ACTION_SECONDARY, GetModuleHandle(nullptr), nullptr);

            ctx->hBtnCancel = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                              0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_ACTION_CANCEL, GetModuleHandle(nullptr), nullptr);

            LayoutControls(ctx);

            // Start check in background
            ctx->checkThread = std::thread(CheckThreadProc, ctx);
            return 0;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* pDIS = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            bool isPressed = (pDIS->itemState & ODS_SELECTED) != 0;
            bool isAccent = false;

            if (pDIS->CtlID == IDC_BTN_ACTION_PRIMARY) {
                if (ctx->state == UpdateState::UpdateAvailable && !ctx->releaseInfo.downloadUrl.empty()) {
                    isAccent = true; // Update In-Place is Accent
                } else if (ctx->state == UpdateState::UpToDate) {
                    isAccent = true; // OK is Accent
                }
            } else if (pDIS->CtlID == IDC_BTN_ACTION_SECONDARY) {
                if (ctx->state == UpdateState::UpdateAvailable && ctx->releaseInfo.downloadUrl.empty()) {
                    isAccent = true; // Go to GitHub is Accent when no direct binary
                }
            }

            wchar_t text[128] = { 0 };
            GetWindowTextW(pDIS->hwndItem, text, 128);
            DrawDarkCapsuleButton(pDIS->hDC, pDIS->rcItem, text, false, isPressed, isAccent, isAccent ? ctx->hFontBold : ctx->hFontBody);
            return TRUE;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
            case IDC_BTN_ACTION_PRIMARY: {
                if (ctx->state == UpdateState::UpdateAvailable && !ctx->releaseInfo.downloadUrl.empty()) {
                    // Start download
                    ctx->state = UpdateState::Downloading;
                    ctx->downloadPercent = 0;
                    ctx->downloadStatus = L"Connecting to download server...";
                    LayoutControls(ctx);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    ctx->downloadThread = std::thread(DownloadThreadProc, ctx);
                } else {
                    // Close dialog (e.g. OK on up-to-date, or Close on error)
                    ctx->isDialogActive = false;
                    DestroyWindow(hWnd);
                }
                return 0;
            }

            case IDC_BTN_ACTION_SECONDARY: {
                // Go to GitHub
                std::wstring targetUrl = ctx->releaseInfo.htmlUrl.empty() ? GITHUB_RELEASES_URL : ctx->releaseInfo.htmlUrl;
                ShellExecuteW(nullptr, L"open", targetUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                return 0;
            }

            case IDC_BTN_ACTION_CANCEL:
            case IDCANCEL: {
                if (ctx->state == UpdateState::Downloading || ctx->state == UpdateState::Checking) {
                    ctx->cancelToken.store(true);
                    if (ctx->state == UpdateState::Downloading) {
                        ctx->state = UpdateState::UpdateAvailable;
                        LayoutControls(ctx);
                        InvalidateRect(hWnd, nullptr, FALSE);
                        return 0;
                    }
                }
                ctx->isDialogActive = false;
                DestroyWindow(hWnd);
                return 0;
            }
            }
            break;
        }

        case WM_APP_CHECK_FINISHED: {
            if (!ctx->releaseInfo.checkSucceeded) {
                ctx->state = UpdateState::Error;
            } else if (ctx->releaseInfo.isUpdateAvailable) {
                ctx->state = UpdateState::UpdateAvailable;
            } else {
                ctx->state = UpdateState::UpToDate;
            }
            LayoutControls(ctx);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_APP_DOWNLOAD_PROGRESS: {
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_APP_DOWNLOAD_FINISHED: {
            BOOL ok = (BOOL)wParam;
            if (ok) {
                ctx->state = UpdateState::Restarting;
                LayoutControls(ctx);
                InvalidateRect(hWnd, nullptr, FALSE);
                UpdateWindow(hWnd);

                // Launch self-replacer process and exit
                wchar_t szExePath[MAX_PATH];
                GetModuleFileNameW(NULL, szExePath, MAX_PATH);

                wchar_t szNewExePath[MAX_PATH];
                wcscpy_s(szNewExePath, szExePath);
                PathRemoveFileSpecW(szNewExePath);
                PathAppendW(szNewExePath, L"Zero.new.exe");

                std::wstring cmd = L"cmd.exe /c timeout /t 1 /nobreak >nul & move /y \"" +
                                   std::wstring(szNewExePath) + L"\" \"" + std::wstring(szExePath) +
                                   L"\" & start \"\" \"" + std::wstring(szExePath) + L"\"";

                STARTUPINFOW si = { sizeof(si) };
                si.dwFlags = STARTF_USESHOWWINDOW;
                si.wShowWindow = SW_HIDE;
                PROCESS_INFORMATION pi = { 0 };

                std::vector<wchar_t> cmdBuffer(cmd.begin(), cmd.end());
                cmdBuffer.push_back(L'\0');

                if (CreateProcessW(NULL, cmdBuffer.data(), NULL, NULL, FALSE,
                                   CREATE_NO_WINDOW | DETACHED_PROCESS, NULL, NULL, &si, &pi)) {
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    ExitProcess(0);
                }
            } else {
                LayoutControls(ctx);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
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

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

            FillRect(memDC, &rcClient, ctx->hBrushBg);

            UINT dpi = GetDlgDpi(hWnd);
            int margin = ScaleDpi(20, dpi);
            int iconSize = ScaleDpi(36, dpi);

            // 1. Draw Application Icon
            HICON hIcon = (HICON)LoadImageW(GetModuleHandle(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
                                            iconSize, iconSize, LR_DEFAULTCOLOR);
            if (hIcon) {
                DrawIconEx(memDC, margin, margin, hIcon, iconSize, iconSize, 0, nullptr, DI_NORMAL);
                DestroyIcon(hIcon);
            }

            SetBkMode(memDC, TRANSPARENT);

            // 2. Header Title
            std::wstring titleText;
            switch (ctx->state) {
            case UpdateState::Checking:        titleText = L"Checking for Updates..."; break;
            case UpdateState::UpToDate:        titleText = L"You're Up to Date!"; break;
            case UpdateState::UpdateAvailable: titleText = L"New Version Available!"; break;
            case UpdateState::Downloading:     titleText = L"Downloading Update..."; break;
            case UpdateState::Restarting:      titleText = L"Restarting Zero..."; break;
            case UpdateState::Error:           titleText = L"Update Check Failed"; break;
            }

            RECT rcTitle = { margin + iconSize + ScaleDpi(14, dpi), margin, width - margin, margin + ScaleDpi(24, dpi) };
            HFONT hOldFont = (HFONT)SelectObject(memDC, ctx->hFontTitle);
            SetTextColor(memDC, RGB(255, 255, 255));
            DrawTextW(memDC, titleText.c_str(), -1, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Subtitle
            RECT rcSub = { rcTitle.left, rcTitle.bottom + ScaleDpi(2, dpi), width - margin, rcTitle.bottom + ScaleDpi(20, dpi) };
            SelectObject(memDC, ctx->hFontSmall);
            SetTextColor(memDC, CLR_TEXT_MUTED);
            const wchar_t* subText = L"hamzabellouch/zero on GitHub";
            DrawTextW(memDC, subText, -1, &rcSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // 3. Central Content Card
            int cardTop = margin + iconSize + ScaleDpi(16, dpi);
            int cardBottom = height - margin - ScaleDpi(48, dpi);
            RECT rcCard = { margin, cardTop, width - margin, cardBottom };

            // Card background & rounded border
            Gdiplus::Graphics g(memDC);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

            float cx = (float)rcCard.left;
            float cy = (float)rcCard.top;
            float cw = (float)(rcCard.right - rcCard.left);
            float ch = (float)(rcCard.bottom - rcCard.top);
            float cr = (float)ScaleDpi(8, dpi);

            Gdiplus::GraphicsPath cardPath;
            cardPath.AddArc(cx, cy, cr, cr, 180, 90);
            cardPath.AddArc(cx + cw - cr, cy, cr, cr, 270, 90);
            cardPath.AddArc(cx + cw - cr, cy + ch - cr, cr, cr, 0, 90);
            cardPath.AddArc(cx, cy + ch - cr, cr, cr, 90, 90);
            cardPath.CloseFigure();

            Gdiplus::SolidBrush cardBgBr(Gdiplus::Color(255, GetRValue(CLR_CARD_BG), GetGValue(CLR_CARD_BG), GetBValue(CLR_CARD_BG)));
            g.FillPath(&cardBgBr, &cardPath);

            Gdiplus::Pen cardPen(Gdiplus::Color(255, GetRValue(CLR_BORDER), GetGValue(CLR_BORDER), GetBValue(CLR_BORDER)), 1.0f);
            g.DrawPath(&cardPen, &cardPath);

            // Card Content Rendering
            int padX = ScaleDpi(16, dpi);
            int padY = ScaleDpi(16, dpi);
            RECT rcCardText = { rcCard.left + padX, rcCard.top + padY, rcCard.right - padX, rcCard.bottom - padY };

            switch (ctx->state) {
            case UpdateState::Checking: {
                SelectObject(memDC, ctx->hFontBody);
                SetTextColor(memDC, CLR_TEXT_NORMAL);
                const wchar_t* msg = L"Connecting to GitHub Releases API to verify latest version...\nPlease wait a moment.";
                DrawTextW(memDC, msg, -1, &rcCardText, DT_LEFT | DT_TOP | DT_WORDBREAK);
                break;
            }
            case UpdateState::UpToDate: {
                std::wstring msg = L"Zero " + std::wstring(CURRENT_VERSION_STR) + L" is currently the latest version.\n\n" +
                                   L"No new updates were found on GitHub. Your standalone uninstaller and task manager is up to date!";
                SelectObject(memDC, ctx->hFontBody);
                SetTextColor(memDC, CLR_TEXT_NORMAL);
                DrawTextW(memDC, msg.c_str(), -1, &rcCardText, DT_LEFT | DT_TOP | DT_WORDBREAK);
                break;
            }
            case UpdateState::UpdateAvailable: {
                std::wstring line1 = L"A new version (" + ctx->releaseInfo.tagName + L") is available!";
                RECT rcLine1 = { rcCardText.left, rcCardText.top, rcCardText.right, rcCardText.top + ScaleDpi(22, dpi) };
                SelectObject(memDC, ctx->hFontBold);
                SetTextColor(memDC, CLR_ACCENT);
                DrawTextW(memDC, line1.c_str(), -1, &rcLine1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                std::wstring line2 = L"Installed Version: " + std::wstring(CURRENT_VERSION_STR) +
                                     L"\nLatest Version: " + ctx->releaseInfo.tagName;
                if (!ctx->releaseInfo.assetName.empty()) {
                    line2 += L"\nPortable Package: " + ctx->releaseInfo.assetName;
                }
                RECT rcLine2 = { rcCardText.left, rcLine1.bottom + ScaleDpi(6, dpi), rcCardText.right, rcCardText.bottom };
                SelectObject(memDC, ctx->hFontBody);
                SetTextColor(memDC, CLR_TEXT_NORMAL);
                DrawTextW(memDC, line2.c_str(), -1, &rcLine2, DT_LEFT | DT_TOP | DT_WORDBREAK);
                break;
            }
            case UpdateState::Downloading: {
                std::wstring statusLine = L"Downloading update package from GitHub...";
                RECT rcStatus = { rcCardText.left, rcCardText.top, rcCardText.right, rcCardText.top + ScaleDpi(20, dpi) };
                SelectObject(memDC, ctx->hFontBody);
                SetTextColor(memDC, CLR_TEXT_NORMAL);
                DrawTextW(memDC, statusLine.c_str(), -1, &rcStatus, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                // Progress Bar
                int barY = rcStatus.bottom + ScaleDpi(12, dpi);
                int barH = ScaleDpi(14, dpi);
                RECT rcBar = { rcCardText.left, barY, rcCardText.right, barY + barH };

                // Bar Background
                Gdiplus::GraphicsPath barBgPath;
                float bx = (float)rcBar.left;
                float by = (float)rcBar.top;
                float bw = (float)(rcBar.right - rcBar.left);
                float bh = (float)(rcBar.bottom - rcBar.top);
                float brad = bh;
                barBgPath.AddArc(bx, by, brad, brad, 90, 180);
                barBgPath.AddArc(bx + bw - brad, by, brad, brad, 270, 180);
                barBgPath.CloseFigure();

                Gdiplus::SolidBrush barBgBr(Gdiplus::Color(255, GetRValue(CLR_BAR_BG), GetGValue(CLR_BAR_BG), GetBValue(CLR_BAR_BG)));
                g.FillPath(&barBgBr, &barBgPath);

                Gdiplus::Pen barPen(Gdiplus::Color(255, GetRValue(CLR_BORDER), GetGValue(CLR_BORDER), GetBValue(CLR_BORDER)), 1.0f);
                g.DrawPath(&barPen, &barBgPath);

                // Bar Fill
                int pct = ctx->downloadPercent;
                if (pct > 0) {
                    float fillW = (bw * (float)pct) / 100.0f;
                    if (fillW < brad) fillW = brad;
                    if (fillW > bw) fillW = bw;

                    Gdiplus::GraphicsPath fillPath;
                    fillPath.AddArc(bx, by, brad, brad, 90, 180);
                    fillPath.AddArc(bx + fillW - brad, by, brad, brad, 270, 180);
                    fillPath.CloseFigure();

                    Gdiplus::SolidBrush fillBr(Gdiplus::Color(255, GetRValue(CLR_ACCENT), GetGValue(CLR_ACCENT), GetBValue(CLR_ACCENT)));
                    g.FillPath(&fillBr, &fillPath);
                }

                // Progress Label
                std::wstringstream ss;
                if (ctx->totalBytes > 0) {
                    double dlMB = (double)ctx->downloadedBytes / (1024.0 * 1024.0);
                    double totMB = (double)ctx->totalBytes / (1024.0 * 1024.0);
                    ss.precision(1);
                    ss << std::fixed << dlMB << L" MB / " << totMB << L" MB (" << pct << L"%)";
                } else {
                    ss << pct << L"%";
                }
                std::wstring pctStr = ss.str();
                RECT rcPct = { rcCardText.left, barY + barH + ScaleDpi(10, dpi), rcCardText.right, barY + barH + ScaleDpi(28, dpi) };
                SelectObject(memDC, ctx->hFontSmall);
                SetTextColor(memDC, CLR_TEXT_MUTED);
                DrawTextW(memDC, pctStr.c_str(), -1, &rcPct, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                break;
            }
            case UpdateState::Restarting: {
                std::wstring msg = L"Update downloaded successfully!\n\nZero is now performing the in-place executable swap and will restart automatically in 1 second...";
                SelectObject(memDC, ctx->hFontBody);
                SetTextColor(memDC, CLR_TEXT_NORMAL);
                DrawTextW(memDC, msg.c_str(), -1, &rcCardText, DT_LEFT | DT_TOP | DT_WORDBREAK);
                break;
            }
            case UpdateState::Error: {
                std::wstring msg = L"Could not complete update check:\n\n" + ctx->releaseInfo.errorMessage +
                                   L"\n\nPlease visit the GitHub releases page directly to download the latest version.";
                SelectObject(memDC, ctx->hFontBody);
                SetTextColor(memDC, RGB(255, 120, 120));
                DrawTextW(memDC, msg.c_str(), -1, &rcCardText, DT_LEFT | DT_TOP | DT_WORDBREAK);
                break;
            }
            }

            SelectObject(memDC, hOldFont);

            BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_CLOSE: {
            if (ctx) {
                ctx->cancelToken.store(true);
                ctx->isDialogActive = false;
            }
            DestroyWindow(hWnd);
            return 0;
        }

        case WM_DESTROY: {
            if (ctx) {
                if (ctx->hBrushBg) DeleteObject(ctx->hBrushBg);
                if (ctx->hFontTitle) DeleteObject(ctx->hFontTitle);
                if (ctx->hFontSubtitle) DeleteObject(ctx->hFontSubtitle);
                if (ctx->hFontBody) DeleteObject(ctx->hFontBody);
                if (ctx->hFontBold) DeleteObject(ctx->hFontBold);
                if (ctx->hFontSmall) DeleteObject(ctx->hFontSmall);
            }
            return 0;
        }
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

const wchar_t* Updater::GetCurrentVersion() {
    return CURRENT_VERSION_STR;
}

const wchar_t* Updater::GetRepoUrl() {
    return GITHUB_REPO_URL;
}

const wchar_t* Updater::GetReleasesUrl() {
    return GITHUB_RELEASES_URL;
}

void Updater::CheckForUpdates(HWND hWndParent, bool silentIfLatest) {
    const wchar_t* CLASS_NAME = L"ZeroUpdateDlgClass";

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = UpdateWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(CLR_BG);

    RegisterClassExW(&wc);

    UpdateContext ctx;
    ctx.isDialogActive = true;

    UINT screenDpi = GetDlgDpi(hWndParent);
    int dlgWidth = ScaleDpi(520, screenDpi);
    int dlgHeight = ScaleDpi(290, screenDpi);

    RECT rcParent;
    GetWindowRect(hWndParent, &rcParent);
    int posX = rcParent.left + (rcParent.right - rcParent.left - dlgWidth) / 2;
    int posY = rcParent.top + (rcParent.bottom - rcParent.top - dlgHeight) / 2;
    if (posX < 0) posX = 100;
    if (posY < 0) posY = 100;

    EnableWindow(hWndParent, FALSE);

    HWND hWndDlg = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Zero - Check for Updates",
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

    if (ctx.checkThread.joinable()) {
        ctx.checkThread.join();
    }
    if (ctx.downloadThread.joinable()) {
        ctx.downloadThread.join();
    }

    EnableWindow(hWndParent, TRUE);
    SetActiveWindow(hWndParent);
    UnregisterClassW(CLASS_NAME, GetModuleHandle(nullptr));
}
