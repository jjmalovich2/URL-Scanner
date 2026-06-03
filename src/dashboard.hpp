#pragma once
#ifndef DASHBOARD_HPP
#define DASHBOARD_HPP

#include "gui_common.hpp"
#include "stats.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

#define ID_DASH_CLOSE    2001
#define ID_DASH_MINIMIZE 2002

struct DashboardState {
    HWND hwnd = nullptr;
    HWND btnClose = nullptr;
    HWND btnMinimize = nullptr;
    HFONT fontTitle = nullptr;
    HFONT fontValue = nullptr;
    HFONT fontLabel = nullptr;
    HFONT fontClose = nullptr;
    HBRUSH brushBg = nullptr;
    ScanStats* stats = nullptr;
};

inline ScanStats g_stats;

namespace {
    void draw_stat_card(HDC hdc, int x, int y, int w, int h, const wchar_t* label, int value, COLORREF accent, HFONT fontVal, HFONT fontLbl) {
        RECT rc = {x, y, x + w, y + h};
        urlscan::draw_rounded_rect(hdc, rc, 8, RGB(45, 45, 65));

        RECT rcAccent = {x, y, x + w, y + 4};
        HBRUSH brush = CreateSolidBrush(accent);
        FillRect(hdc, &rcAccent, brush);
        DeleteObject(brush);

        SelectObject(hdc, fontVal);
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);
        RECT rcVal = {x, y + 18, x + w, y + 58};
        std::wstring valStr = std::to_wstring(value);
        DrawTextW(hdc, valStr.c_str(), -1, &rcVal, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, fontLbl);
        SetTextColor(hdc, RGB(180, 180, 200));
        RECT rcLbl = {x, y + 64, x + w, y + 88};
        DrawTextW(hdc, label, -1, &rcLbl, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
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
            412, 12, 24, 24, hwnd, reinterpret_cast<HMENU>(ID_DASH_MINIMIZE), hInst, nullptr);
        state->btnClose = CreateWindowW(L"BUTTON", L"\u2715",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            444, 12, 24, 24, hwnd, reinterpret_cast<HMENU>(ID_DASH_CLOSE), hInst, nullptr);
        SetTimer(hwnd, 1, 200, nullptr);
        return 0;
    }

    case WM_TIMER:
        if (wParam == 1) {
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

        RECT rcBar = {0, 0, rc.right, 5};
        HBRUSH barBrush = CreateSolidBrush(RGB(0, 180, 200));
        FillRect(hdcMem, &rcBar, barBrush);
        DeleteObject(barBrush);

        SetTextColor(hdcMem, RGB(255, 255, 255));
        SetBkMode(hdcMem, TRANSPARENT);
        SelectObject(hdcMem, state->fontTitle);
        RECT rcTitle = {24, 24, 380, 56};
        DrawTextW(hdcMem, L"Scanner Dashboard", -1, &rcTitle, DT_LEFT | DT_TOP | DT_SINGLELINE);

        SelectObject(hdcMem, state->fontLabel);
        SetTextColor(hdcMem, RGB(140, 140, 160));
        RECT rcSub = {24, 58, rc.right - 24, 80};
        DrawTextW(hdcMem, L"Live network traffic statistics", -1, &rcSub, DT_LEFT | DT_TOP | DT_SINGLELINE);

        int cardW = 208;
        int cardH = 96;
        int gap = 16;
        int x1 = 24;
        int x2 = x1 + cardW + gap;
        int y1 = 92;
        int y2 = y1 + cardH + gap;

        if (state->stats) {
            draw_stat_card(hdcMem, x1, y1, cardW, cardH, L"Total Captured", state->stats->total.load(), RGB(100, 100, 120), state->fontValue, state->fontLabel);
            draw_stat_card(hdcMem, x2, y1, cardW, cardH, L"Suspicious", state->stats->suspicious.load(), RGB(220, 150, 50), state->fontValue, state->fontLabel);
            draw_stat_card(hdcMem, x1, y2, cardW, cardH, L"Blocked", state->stats->blocked.load(), RGB(220, 53, 69), state->fontValue, state->fontLabel);
            draw_stat_card(hdcMem, x2, y2, cardW, cardH, L"Allowed", state->stats->allowed.load(), RGB(40, 167, 69), state->fontValue, state->fontLabel);
        }

        SetTextColor(hdcMem, RGB(120, 120, 140));
        RECT rcNote = {24, rc.bottom - 32, rc.right - 24, rc.bottom - 8};
        DrawTextW(hdcMem, L"Suspicious threshold: Score >= 40  |  Entropy >= 3.0", -1, &rcNote, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

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
            DestroyWindow(hwnd);
        } else if (id == ID_DASH_MINIMIZE) {
            ShowWindow(hwnd, SW_MINIMIZE);
        }
        return 0;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (dis->CtlType != ODT_BUTTON) return TRUE;

        if (dis->CtlID == ID_DASH_CLOSE || dis->CtlID == ID_DASH_MINIMIZE) {
            COLORREF bg = (dis->itemState & ODS_SELECTED) ? RGB(80, 80, 100) : RGB(50, 50, 70);
            urlscan::draw_rounded_rect(dis->hDC, dis->rcItem, 4, bg);
            SetTextColor(dis->hDC, RGB(200, 200, 200));
            SetBkMode(dis->hDC, TRANSPARENT);
            SelectObject(dis->hDC, state->fontClose);
            const wchar_t* label = (dis->CtlID == ID_DASH_MINIMIZE) ? L"\u2212" : L"\u2715";
            DrawTextW(dis->hDC, label, -1, const_cast<LPRECT>(&dis->rcItem), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        return TRUE;
    }

    case WM_NCHITTEST: {
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd, &pt);
        if (pt.y < 40) return HTCAPTION;
        return HTCLIENT;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
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
    state.brushBg = CreateSolidBrush(RGB(30, 30, 46));

    const int width = 480;
    const int height = 320;

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
        if (atom) UnregisterClassW(L"URLScannerDashboard", hInst);
        return;
    }

    state.hwnd = hwnd;
    state.fontTitle = urlscan::create_font(24, FW_SEMIBOLD, L"Segoe UI");
    state.fontValue = urlscan::create_font(36, FW_BOLD, L"Segoe UI");
    state.fontLabel = urlscan::create_font(13, FW_NORMAL, L"Segoe UI");
    state.fontClose = urlscan::create_font(16, FW_NORMAL, L"Segoe UI");

    HRGN hRgn = CreateRoundRectRgn(0, 0, width, height, 12, 12);
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
    DeleteObject(state.fontClose);
    DeleteObject(state.brushBg);
    if (atom) UnregisterClassW(L"URLScannerDashboard", hInst);
}

#endif // DASHBOARD_HPP
