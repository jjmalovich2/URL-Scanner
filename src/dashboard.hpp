#pragma once
#ifndef DASHBOARD_HPP
#define DASHBOARD_HPP

#include "gui_common.hpp"
#include "stats.hpp"
#include "config.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <psapi.h>

#define ID_DASH_CLOSE     2001
#define ID_DASH_MINIMIZE  2002
#define ID_TAB_DASHBOARD  3001
#define ID_TAB_SETTINGS   3002
#define ID_TAB_LISTS      3003
#define ID_TAB_PERFORMANCE 3004
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_SHOW 7001
#define ID_TRAY_EXIT 7002
#define ID_EDIT_SUSP_SCORE    4001
#define ID_EDIT_SUSP_ENTROPY  4002
#define ID_EDIT_ACT_SCORE     4003
#define ID_EDIT_ACT_ENTROPY   4004
#define ID_EDIT_KEYWORDS      4005
#define ID_EDIT_WHITELIST     4006
#define ID_EDIT_BLACKLIST     4007
#define ID_BTN_SAVE       5001

struct DashboardState {
    HWND hwnd = nullptr;
    HWND btnClose = nullptr;
    HWND btnMinimize = nullptr;
    HWND btnTabDashboard = nullptr;
    HWND btnTabSettings = nullptr;
    HWND btnTabLists = nullptr;
    HWND btnTabPerformance = nullptr;
    HWND editSuspScore = nullptr;
    HWND editSuspEntropy = nullptr;
    HWND editActionScore = nullptr;
    HWND editActionEntropy = nullptr;
    HWND editKeywords = nullptr;
    HWND editWhitelist = nullptr;
    HWND editBlacklist = nullptr;
    HWND btnSave = nullptr;
    HFONT fontTitle = nullptr;
    HFONT fontValue = nullptr;
    HFONT fontLabel = nullptr;
    HFONT fontSmall = nullptr;
    HFONT fontClose = nullptr;
    HBRUSH brushBg = nullptr;
    HBRUSH brushEditBg = nullptr;
    ScanStats* stats = nullptr;
    int activeTab = 0;
    std::chrono::steady_clock::time_point startTime;
};

inline ScanStats g_stats;

namespace {
    void draw_modern_card(HDC hdc, int x, int y, int w, int h, const wchar_t* label, int value,
                          COLORREF accent, HFONT fontVal, HFONT fontLbl) {
        RECT rc = {x, y, x + w, y + h};
        urlscan::draw_rounded_rect(hdc, rc, 12, RGB(38, 38, 52));

        // Top accent strip
        RECT rcAccent = {x + 16, y, x + w - 16, y + 4};
        HBRUSH barBrush = CreateSolidBrush(accent);
        FillRect(hdc, &rcAccent, barBrush);
        DeleteObject(barBrush);

        // Label
        SelectObject(hdc, fontLbl);
        SetTextColor(hdc, RGB(160, 160, 180));
        SetBkMode(hdc, TRANSPARENT);
        RECT rcLabel = {x + 24, y + 20, x + w - 24, y + 46};
        DrawTextW(hdc, label, -1, &rcLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Value
        SelectObject(hdc, fontVal);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT rcVal = {x + 24, y + 52, x + w - 24, y + h - 20};
        std::wstring valStr = std::to_wstring(value);
        DrawTextW(hdc, valStr.c_str(), -1, &rcVal, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    void draw_text_card(HDC hdc, int x, int y, int w, int h, const wchar_t* label,
                        const wchar_t* value, COLORREF accent, HFONT fontVal, HFONT fontLbl) {
        RECT rc = {x, y, x + w, y + h};
        urlscan::draw_rounded_rect(hdc, rc, 12, RGB(38, 38, 52));

        RECT rcAccent = {x + 16, y, x + w - 16, y + 4};
        HBRUSH barBrush = CreateSolidBrush(accent);
        FillRect(hdc, &rcAccent, barBrush);
        DeleteObject(barBrush);

        SelectObject(hdc, fontLbl);
        SetTextColor(hdc, RGB(160, 160, 180));
        SetBkMode(hdc, TRANSPARENT);
        RECT rcLabel = {x + 24, y + 20, x + w - 24, y + 46};
        DrawTextW(hdc, label, -1, &rcLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, fontVal);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT rcVal = {x + 24, y + 52, x + w - 24, y + h - 20};
        DrawTextW(hdc, value, -1, &rcVal, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    void draw_settings_card(HDC hdc, int x, int y, int w, int h, const wchar_t* title,
                            HFONT fontTitle) {
        RECT rc = {x, y, x + w, y + h};
        urlscan::draw_rounded_rect(hdc, rc, 12, RGB(38, 38, 52));

        SelectObject(hdc, fontTitle);
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);
        RECT rcTitle = {x + 24, y + 18, x + w - 24, y + 44};
        DrawTextW(hdc, title, -1, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    void update_tab_controls(DashboardState* state) {
        bool isSettings = (state->activeTab == 1);
        bool isLists = (state->activeTab == 2);
        ShowWindow(state->editSuspScore, isSettings ? SW_SHOW : SW_HIDE);
        ShowWindow(state->editSuspEntropy, isSettings ? SW_SHOW : SW_HIDE);
        ShowWindow(state->editActionScore, isSettings ? SW_SHOW : SW_HIDE);
        ShowWindow(state->editActionEntropy, isSettings ? SW_SHOW : SW_HIDE);
        ShowWindow(state->editKeywords, isSettings ? SW_SHOW : SW_HIDE);
        ShowWindow(state->editWhitelist, isLists ? SW_SHOW : SW_HIDE);
        ShowWindow(state->editBlacklist, isLists ? SW_SHOW : SW_HIDE);
        ShowWindow(state->btnSave, (isSettings || isLists) ? SW_SHOW : SW_HIDE);
    }
}

static LRESULT CALLBACK DashboardWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DashboardState* state = nullptr;
    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        state = reinterpret_cast<DashboardState*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    } else {
        state = reinterpret_cast<DashboardState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!state) return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_CREATE: {
        HINSTANCE hInst = GetModuleHandle(nullptr);

        state->btnMinimize = CreateWindowW(L"BUTTON", L"\u2212",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            632, 16, 28, 28, hwnd, reinterpret_cast<HMENU>(ID_DASH_MINIMIZE), hInst, nullptr);
        state->btnClose = CreateWindowW(L"BUTTON", L"\u2715",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            668, 16, 28, 28, hwnd, reinterpret_cast<HMENU>(ID_DASH_CLOSE), hInst, nullptr);

        state->btnTabDashboard = CreateWindowW(L"BUTTON", L"Dashboard",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            24, 88, 120, 36, hwnd, reinterpret_cast<HMENU>(ID_TAB_DASHBOARD), hInst, nullptr);
        state->btnTabSettings = CreateWindowW(L"BUTTON", L"Settings",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            156, 88, 120, 36, hwnd, reinterpret_cast<HMENU>(ID_TAB_SETTINGS), hInst, nullptr);
        state->btnTabLists = CreateWindowW(L"BUTTON", L"Lists",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            288, 88, 120, 36, hwnd, reinterpret_cast<HMENU>(ID_TAB_LISTS), hInst, nullptr);
        state->btnTabPerformance = CreateWindowW(L"BUTTON", L"Performance",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            420, 88, 120, 36, hwnd, reinterpret_cast<HMENU>(ID_TAB_PERFORMANCE), hInst, nullptr);

        state->editSuspScore = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | ES_NUMBER | WS_BORDER,
            52, 220, 72, 24, hwnd, reinterpret_cast<HMENU>(ID_EDIT_SUSP_SCORE), hInst, nullptr);
        state->editSuspEntropy = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_BORDER,
            250, 220, 72, 24, hwnd, reinterpret_cast<HMENU>(ID_EDIT_SUSP_ENTROPY), hInst, nullptr);
        state->editActionScore = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | ES_NUMBER | WS_BORDER,
            378, 220, 72, 24, hwnd, reinterpret_cast<HMENU>(ID_EDIT_ACT_SCORE), hInst, nullptr);
        state->editActionEntropy = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_BORDER,
            576, 220, 72, 24, hwnd, reinterpret_cast<HMENU>(ID_EDIT_ACT_ENTROPY), hInst, nullptr);
        state->editKeywords = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_BORDER | ES_MULTILINE | WS_VSCROLL | ES_AUTOVSCROLL,
            24, 368, 632, 140, hwnd, reinterpret_cast<HMENU>(ID_EDIT_KEYWORDS), hInst, nullptr);
        state->editWhitelist = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_BORDER | ES_MULTILINE | WS_VSCROLL | ES_AUTOVSCROLL,
            24, 164, 310, 340, hwnd, reinterpret_cast<HMENU>(ID_EDIT_WHITELIST), hInst, nullptr);
        state->editBlacklist = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_BORDER | ES_MULTILINE | WS_VSCROLL | ES_AUTOVSCROLL,
            354, 164, 302, 340, hwnd, reinterpret_cast<HMENU>(ID_EDIT_BLACKLIST), hInst, nullptr);
        state->btnSave = CreateWindowW(L"BUTTON", L"Save",
            WS_CHILD | BS_OWNERDRAW,
            24, 524, 120, 36, hwnd, reinterpret_cast<HMENU>(ID_BTN_SAVE), hInst, nullptr);

        wchar_t buf[64];
        swprintf(buf, 64, L"%d", g_config.suspicious_score.load());
        SetWindowTextW(state->editSuspScore, buf);
        swprintf(buf, 64, L"%.1f", g_config.suspicious_entropy.load());
        SetWindowTextW(state->editSuspEntropy, buf);
        swprintf(buf, 64, L"%d", g_config.action_score.load());
        SetWindowTextW(state->editActionScore, buf);
        swprintf(buf, 64, L"%.1f", g_config.action_entropy.load());
        SetWindowTextW(state->editActionEntropy, buf);

        std::filesystem::path kw_path = g_exe_dir / ".." / "config" / "keywords.csv";
        std::string kw_text = read_file_text(kw_path.string());
        SetWindowTextW(state->editKeywords, urlscan::utf8_to_wstring(kw_text).c_str());

        std::filesystem::path wl_path = g_exe_dir / ".." / "config" / "whitelist.txt";
        std::string wl_text = read_file_text(wl_path.string());
        SetWindowTextW(state->editWhitelist, urlscan::utf8_to_wstring(wl_text).c_str());

        std::filesystem::path bl_path = g_exe_dir / ".." / "config" / "blacklist.txt";
        std::string bl_text = read_file_text(bl_path.string());
        SetWindowTextW(state->editBlacklist, urlscan::utf8_to_wstring(bl_text).c_str());

        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = LoadIconW(nullptr, reinterpret_cast<LPCWSTR>(IDI_APPLICATION));
        wcscpy(nid.szTip, L"URL Scanner");
        Shell_NotifyIconW(NIM_ADD, &nid);

        SetTimer(hwnd, 1, 250, nullptr);
        return 0;
    }

    case WM_TIMER:
        if (wParam == 1 && (state->activeTab == 0 || state->activeTab == 3)) {
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ oldBmp = SelectObject(hdcMem, hbmMem);

        FillRect(hdcMem, &rc, state->brushBg);

        // Top accent bar
        RECT rcBar = {0, 0, rc.right, 5};
        HBRUSH barBrush = CreateSolidBrush(RGB(0, 180, 200));
        FillRect(hdcMem, &rcBar, barBrush);
        DeleteObject(barBrush);

        // Title
        SetTextColor(hdcMem, RGB(255, 255, 255));
        SetBkMode(hdcMem, TRANSPARENT);
        SelectObject(hdcMem, state->fontTitle);
        RECT rcTitle = {24, 22, 500, 56};
        DrawTextW(hdcMem, L"Scanner Dashboard", -1, &rcTitle, DT_LEFT | DT_TOP | DT_SINGLELINE);

        // Subtitle
        SelectObject(hdcMem, state->fontSmall);
        SetTextColor(hdcMem, RGB(130, 130, 150));
        RECT rcSub = {24, 58, rc.right - 24, 80};
        const wchar_t* subtitle = L"Live network traffic statistics";
        if (state->activeTab == 1) subtitle = L"Configure thresholds and keywords";
        else if (state->activeTab == 2) subtitle = L"Manage whitelist and blacklist domains";
        else if (state->activeTab == 3) subtitle = L"Process diagnostics and resource usage";
        DrawTextW(hdcMem, subtitle, -1, &rcSub, DT_LEFT | DT_TOP | DT_SINGLELINE);

        if (state->activeTab == 0) {
            // ---- DASHBOARD TAB ----
            int cardW = 306;
            int cardH = 170;
            int gap = 20;
            int x1 = 24;
            int x2 = x1 + cardW + gap;
            int y1 = 140;
            int y2 = y1 + cardH + gap;

            if (state->stats) {
                draw_modern_card(hdcMem, x1, y1, cardW, cardH, L"Total Captured",
                    state->stats->total.load(), RGB(100, 120, 140),
                    state->fontValue, state->fontLabel);
                draw_modern_card(hdcMem, x2, y1, cardW, cardH, L"Suspicious",
                    state->stats->suspicious.load(), RGB(230, 170, 60),
                    state->fontValue, state->fontLabel);
                draw_modern_card(hdcMem, x1, y2, cardW, cardH, L"Blocked",
                    state->stats->blocked.load(), RGB(220, 60, 70),
                    state->fontValue, state->fontLabel);
                draw_modern_card(hdcMem, x2, y2, cardW, cardH, L"Allowed",
                    state->stats->allowed.load(), RGB(50, 180, 90),
                    state->fontValue, state->fontLabel);
            }

            // Threshold note
            SelectObject(hdcMem, state->fontSmall);
            SetTextColor(hdcMem, RGB(110, 110, 130));
            RECT rcNote = {24, rc.bottom - 36, rc.right - 24, rc.bottom - 12};
            std::wostringstream note;
            note << L"Suspicious: Score >= " << g_config.suspicious_score.load()
                 << L"  |  Entropy >= " << std::fixed << std::setprecision(1) << g_config.suspicious_entropy.load()
                 << L"        Action: Score >= " << g_config.action_score.load()
                 << L"  |  Entropy >= " << g_config.action_entropy.load();
            DrawTextW(hdcMem, note.str().c_str(), -1, &rcNote, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        } else if (state->activeTab == 1) {
            // ---- SETTINGS TAB ----
            int cardW = 306;
            int cardH = 120;
            int gap = 20;

            draw_settings_card(hdcMem, 24, 140, cardW, cardH, L"Suspicious Threshold",
                state->fontLabel);
            draw_settings_card(hdcMem, 24 + cardW + gap, 140, cardW, cardH, L"Action Threshold",
                state->fontLabel);

            // Input labels inside cards
            SelectObject(hdcMem, state->fontSmall);
            SetTextColor(hdcMem, RGB(160, 160, 180));

            RECT rcS1 = {48, 190, 150, 214};
            DrawTextW(hdcMem, L"Score threshold", -1, &rcS1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            RECT rcS2 = {250, 190, 320, 214};
            DrawTextW(hdcMem, L"Entropy", -1, &rcS2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            RECT rcA1 = {374, 190, 476, 214};
            DrawTextW(hdcMem, L"Score threshold", -1, &rcA1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            RECT rcA2 = {576, 190, 646, 214};
            DrawTextW(hdcMem, L"Entropy", -1, &rcA2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Keywords label
            RECT rcKW = {24, 336, 400, 360};
            DrawTextW(hdcMem, L"Keywords (CSV format):", -1, &rcKW, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        } else if (state->activeTab == 2) {
            // ---- LISTS TAB ----
            SelectObject(hdcMem, state->fontSmall);
            SetTextColor(hdcMem, RGB(160, 160, 180));

            RECT rcWL = {24, 140, 310, 164};
            DrawTextW(hdcMem, L"Whitelist — ignored, no alerts:", -1, &rcWL, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            RECT rcBL = {354, 140, 656, 164};
            DrawTextW(hdcMem, L"Blacklist — auto-blocked:", -1, &rcBL, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        } else if (state->activeTab == 3) {
            // ---- PERFORMANCE TAB ----
            PROCESS_MEMORY_COUNTERS pmc = {};
            double ramMB = 0.0;
            double peakMB = 0.0;
            if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
                ramMB = pmc.WorkingSetSize / (1024.0 * 1024.0);
                peakMB = pmc.PeakWorkingSetSize / (1024.0 * 1024.0);
            }

            DWORD handleCount = 0;
            GetProcessHandleCount(GetCurrentProcess(), &handleCount);

            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - state->startTime).count();
            int hours = static_cast<int>(elapsed / 3600);
            int minutes = static_cast<int>((elapsed % 3600) / 60);
            int seconds = static_cast<int>(elapsed % 60);

            wchar_t buf[64];
            swprintf(buf, 64, L"%.1f MB", ramMB);
            draw_text_card(hdcMem, 24, 140, 306, 170, L"RAM Usage", buf,
                RGB(100, 120, 140), state->fontValue, state->fontLabel);

            swprintf(buf, 64, L"%.1f MB", peakMB);
            draw_text_card(hdcMem, 354, 140, 306, 170, L"Peak RAM", buf,
                RGB(230, 170, 60), state->fontValue, state->fontLabel);

            swprintf(buf, 64, L"%lu", handleCount);
            draw_text_card(hdcMem, 24, 330, 306, 170, L"Handle Count", buf,
                RGB(220, 60, 70), state->fontValue, state->fontLabel);

            swprintf(buf, 64, L"%02d:%02d:%02d", hours, minutes, seconds);
            draw_text_card(hdcMem, 354, 330, 306, 170, L"Uptime", buf,
                RGB(50, 180, 90), state->fontValue, state->fontLabel);
        }

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);

        SelectObject(hdcMem, oldBmp);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_COMMAND: {
        WORD id = LOWORD(wParam);
        if (id == ID_DASH_CLOSE) {
            ShowWindow(hwnd, SW_HIDE);
        } else if (id == ID_DASH_MINIMIZE) {
            ShowWindow(hwnd, SW_MINIMIZE);
        } else if (id == ID_TRAY_SHOW) {
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
        } else if (id == ID_TRAY_EXIT) {
            DestroyWindow(hwnd);
        } else if (id == ID_TAB_DASHBOARD && state->activeTab != 0) {
            state->activeTab = 0;
            update_tab_controls(state);
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == ID_TAB_SETTINGS && state->activeTab != 1) {
            state->activeTab = 1;
            update_tab_controls(state);
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == ID_TAB_LISTS && state->activeTab != 2) {
            state->activeTab = 2;
            update_tab_controls(state);
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == ID_TAB_PERFORMANCE && state->activeTab != 3) {
            state->activeTab = 3;
            update_tab_controls(state);
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (id == ID_BTN_SAVE) {
            wchar_t buf[64];
            GetWindowTextW(state->editSuspScore, buf, 64);
            g_config.suspicious_score = static_cast<int>(wcstol(buf, nullptr, 10));
            GetWindowTextW(state->editSuspEntropy, buf, 64);
            g_config.suspicious_entropy = wcstod(buf, nullptr);
            GetWindowTextW(state->editActionScore, buf, 64);
            g_config.action_score = static_cast<int>(wcstol(buf, nullptr, 10));
            GetWindowTextW(state->editActionEntropy, buf, 64);
            g_config.action_entropy = wcstod(buf, nullptr);

            std::filesystem::path thresholds_path = g_exe_dir / ".." / "config" / "thresholds.txt";
            save_thresholds(thresholds_path.string());

            int kwLen = GetWindowTextLengthW(state->editKeywords);
            std::wstring kwWide(kwLen, 0);
            GetWindowTextW(state->editKeywords, kwWide.data(), kwLen + 1);
            std::string kwUtf8 = urlscan::wstring_to_utf8(kwWide);

            std::filesystem::path kw_path = g_exe_dir / ".." / "config" / "keywords.csv";
            if (write_file_text(kw_path.string(), kwUtf8)) {
                g_scanner.load_keywords(kw_path.string());
            }

            int wlLen = GetWindowTextLengthW(state->editWhitelist);
            std::wstring wlWide(wlLen, 0);
            GetWindowTextW(state->editWhitelist, wlWide.data(), wlLen + 1);
            std::string wlUtf8 = urlscan::wstring_to_utf8(wlWide);

            std::filesystem::path wl_path = g_exe_dir / ".." / "config" / "whitelist.txt";
            if (write_file_text(wl_path.string(), wlUtf8)) {
                g_scanner.load_whitelist(wl_path.string());
            }

            int blLen = GetWindowTextLengthW(state->editBlacklist);
            std::wstring blWide(blLen, 0);
            GetWindowTextW(state->editBlacklist, blWide.data(), blLen + 1);
            std::string blUtf8 = urlscan::wstring_to_utf8(blWide);

            std::filesystem::path bl_path = g_exe_dir / ".." / "config" / "blacklist.txt";
            if (write_file_text(bl_path.string(), blUtf8)) {
                g_scanner.load_blacklist(bl_path.string());
            }
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (dis->CtlType != ODT_BUTTON) return TRUE;

        if (dis->CtlID == ID_DASH_CLOSE || dis->CtlID == ID_DASH_MINIMIZE) {
            COLORREF bg = (dis->itemState & ODS_SELECTED) ? RGB(70, 70, 90) : RGB(45, 45, 60);
            urlscan::draw_rounded_rect(dis->hDC, dis->rcItem, 6, bg);
            SetTextColor(dis->hDC, RGB(200, 200, 220));
            SetBkMode(dis->hDC, TRANSPARENT);
            SelectObject(dis->hDC, state->fontClose);
            const wchar_t* label = (dis->CtlID == ID_DASH_MINIMIZE) ? L"\u2212" : L"\u2715";
            DrawTextW(dis->hDC, label, -1, const_cast<LPRECT>(&dis->rcItem), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }

        if (dis->CtlID == ID_TAB_DASHBOARD || dis->CtlID == ID_TAB_SETTINGS || dis->CtlID == ID_TAB_LISTS || dis->CtlID == ID_TAB_PERFORMANCE) {
            bool isActive = (dis->CtlID == ID_TAB_DASHBOARD && state->activeTab == 0) ||
                            (dis->CtlID == ID_TAB_SETTINGS && state->activeTab == 1) ||
                            (dis->CtlID == ID_TAB_LISTS && state->activeTab == 2) ||
                            (dis->CtlID == ID_TAB_PERFORMANCE && state->activeTab == 3);
            COLORREF bg = isActive ? RGB(45, 45, 65) : RGB(30, 30, 46);
            COLORREF text = isActive ? RGB(255, 255, 255) : RGB(130, 130, 150);
            urlscan::draw_rounded_rect(dis->hDC, dis->rcItem, 18, bg);
            SetTextColor(dis->hDC, text);
            SetBkMode(dis->hDC, TRANSPARENT);
            SelectObject(dis->hDC, state->fontLabel);
            const wchar_t* label = L"Lists";
            if (dis->CtlID == ID_TAB_DASHBOARD) label = L"Dashboard";
            else if (dis->CtlID == ID_TAB_SETTINGS) label = L"Settings";
            else if (dis->CtlID == ID_TAB_PERFORMANCE) label = L"Performance";
            DrawTextW(dis->hDC, label, -1, const_cast<LPRECT>(&dis->rcItem), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            if (isActive) {
                RECT rcLine = {dis->rcItem.left + 24, dis->rcItem.bottom - 4, dis->rcItem.right - 24, dis->rcItem.bottom - 2};
                HBRUSH lineBrush = CreateSolidBrush(RGB(0, 180, 200));
                FillRect(dis->hDC, &rcLine, lineBrush);
                DeleteObject(lineBrush);
            }
            return TRUE;
        }

        if (dis->CtlID == ID_BTN_SAVE) {
            COLORREF bg = (dis->itemState & ODS_SELECTED) ? RGB(0, 150, 170) : RGB(0, 180, 200);
            urlscan::draw_rounded_rect(dis->hDC, dis->rcItem, 8, bg);
            SetTextColor(dis->hDC, RGB(255, 255, 255));
            SetBkMode(dis->hDC, TRANSPARENT);
            SelectObject(dis->hDC, state->fontLabel);
            DrawTextW(dis->hDC, L"Save", -1, const_cast<LPRECT>(&dis->rcItem), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }

        return TRUE;
    }

    case WM_CTLCOLOREDIT:
        SetBkColor((HDC)wParam, RGB(40, 40, 60));
        SetTextColor((HDC)wParam, RGB(220, 220, 240));
        return (LRESULT)state->brushEditBg;

    case WM_NCHITTEST: {
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd, &pt);
        if (pt.y < 48) return HTCAPTION;
        return HTCLIENT;
    }

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_LBUTTONDBLCLK) {
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
        } else if (lParam == WM_RBUTTONUP) {
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_SHOW, L"Show Dashboard");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(hMenu);
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        {
            NOTIFYICONDATAW nid = {};
            nid.cbSize = sizeof(nid);
            nid.hWnd = hwnd;
            nid.uID = 1;
            Shell_NotifyIconW(NIM_DELETE, &nid);
        }
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

inline void run_dashboard() {
    SetProcessDPIAware();

    HINSTANCE hInst = GetModuleHandle(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DashboardWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"URLScannerDashboard";
    wc.style = CS_DROPSHADOW;

    ATOM atom = RegisterClassExW(&wc);
    if (!atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return;
    }

    DashboardState state;
    state.stats = &g_stats;
    state.startTime = std::chrono::steady_clock::now();
    state.brushBg = CreateSolidBrush(RGB(25, 25, 35));
    state.brushEditBg = CreateSolidBrush(RGB(40, 40, 60));

    const int width = 705;
    const int height = 575;

    RECT rcWork;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0);
    int x = rcWork.left + (rcWork.right - rcWork.left - width) / 2;
    int y = rcWork.top + (rcWork.bottom - rcWork.top - height) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW,
        L"URLScannerDashboard", nullptr,
        WS_POPUP,
        x, y, width, height,
        nullptr, nullptr, hInst, &state);

    if (!hwnd) {
        DeleteObject(state.brushBg);
        DeleteObject(state.brushEditBg);
        if (atom) UnregisterClassW(L"URLScannerDashboard", hInst);
        return;
    }

    state.hwnd = hwnd;
    state.fontTitle = urlscan::create_font(28, FW_SEMIBOLD, L"Segoe UI");
    state.fontValue = urlscan::create_font(48, FW_BOLD, L"Segoe UI");
    state.fontLabel = urlscan::create_font(14, FW_SEMIBOLD, L"Segoe UI");
    state.fontSmall = urlscan::create_font(12, FW_NORMAL, L"Segoe UI");
    state.fontClose = urlscan::create_font(18, FW_NORMAL, L"Segoe UI");

    HRGN hRgn = CreateRoundRectRgn(0, 0, width, height, 16, 16);
    SetWindowRgn(hwnd, hRgn, TRUE);
    DeleteObject(hRgn);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(state.fontTitle);
    DeleteObject(state.fontValue);
    DeleteObject(state.fontLabel);
    DeleteObject(state.fontSmall);
    DeleteObject(state.fontClose);
    DeleteObject(state.brushBg);
    DeleteObject(state.brushEditBg);
    if (atom) UnregisterClassW(L"URLScannerDashboard", hInst);
}

#endif // DASHBOARD_HPP
