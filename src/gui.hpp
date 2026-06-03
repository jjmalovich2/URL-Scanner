#pragma once
#ifndef GUI_HPP
#define GUI_HPP

#include "gui_common.hpp"
#include <mutex>
#include <sstream>
#include <iomanip>

#define ID_BTN_ALLOW  1001
#define ID_BTN_BLOCK  1002
#define ID_BTN_CLOSE  1003

struct AlertState {
    HWND hwnd = nullptr;
    HWND btnAllow = nullptr;
    HWND btnBlock = nullptr;
    HWND btnClose = nullptr;
    HFONT fontTitle = nullptr;
    HFONT fontBody = nullptr;
    HFONT fontMono = nullptr;
    HFONT fontBtn = nullptr;
    HFONT fontClose = nullptr;
    HBRUSH brushBg = nullptr;
    std::string url;
    int score = 0;
    double entropy = 0.0;
    int choice = 0; // 0=none, 1=allow, 2=block
};

inline std::mutex g_alert_mutex;

static LRESULT CALLBACK AlertWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AlertState* state = nullptr;
    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        state = reinterpret_cast<AlertState*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    } else {
        state = reinterpret_cast<AlertState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!state) return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_CREATE: {
        HINSTANCE hInst = GetModuleHandle(nullptr);
        state->btnAllow = CreateWindowW(L"BUTTON", L"Allow",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            280, 255, 100, 36, hwnd, reinterpret_cast<HMENU>(ID_BTN_ALLOW), hInst, nullptr);
        state->btnBlock = CreateWindowW(L"BUTTON", L"Block",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            400, 255, 100, 36, hwnd, reinterpret_cast<HMENU>(ID_BTN_BLOCK), hInst, nullptr);
        state->btnClose = CreateWindowW(L"BUTTON", L"\u2715",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            484, 12, 24, 24, hwnd, reinterpret_cast<HMENU>(ID_BTN_CLOSE), hInst, nullptr);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        FillRect(hdc, &rc, state->brushBg);

        RECT rcBar = {0, 0, rc.right, 5};
        HBRUSH barBrush = CreateSolidBrush(RGB(220, 53, 69));
        FillRect(hdc, &rcBar, barBrush);
        DeleteObject(barBrush);

        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);
        SelectObject(hdc, state->fontTitle);
        RECT rcTitle = {24, 24, 460, 56};
        DrawTextW(hdc, L"Security Alert", -1, &rcTitle, DT_LEFT | DT_TOP | DT_SINGLELINE);

        SelectObject(hdc, state->fontBody);
        SetTextColor(hdc, RGB(180, 180, 200));
        RECT rcSub = {24, 60, rc.right - 24, 84};
        DrawTextW(hdc, L"A suspicious network request was detected.", -1, &rcSub, DT_LEFT | DT_TOP | DT_SINGLELINE);

        SetTextColor(hdc, RGB(140, 140, 160));
        RECT rcUrlLabel = {24, 100, rc.right - 24, 120};
        DrawTextW(hdc, L"Target URL:", -1, &rcUrlLabel, DT_LEFT | DT_TOP | DT_SINGLELINE);

        RECT rcUrlBox = {24, 124, rc.right - 24, 164};
        urlscan::draw_rounded_rect(hdc, rcUrlBox, 4, RGB(40, 40, 60));

        SelectObject(hdc, state->fontMono);
        SetTextColor(hdc, RGB(100, 200, 255));
        RECT rcUrlText = {rcUrlBox.left + 12, rcUrlBox.top + 8, rcUrlBox.right - 12, rcUrlBox.bottom - 8};
        std::wstring wurl = urlscan::utf8_to_wstring(state->url);
        DrawTextW(hdc, wurl.c_str(), -1, &rcUrlText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        SelectObject(hdc, state->fontBody);
        SetTextColor(hdc, RGB(180, 180, 200));
        std::wostringstream stats;
        stats << L"Score: " << state->score << L" / 100    |    Entropy: " << std::fixed << std::setprecision(2) << state->entropy;
        RECT rcStats = {24, 178, rc.right - 24, 202};
        DrawTextW(hdc, stats.str().c_str(), -1, &rcStats, DT_LEFT | DT_TOP | DT_SINGLELINE);

        SetTextColor(hdc, RGB(220, 220, 240));
        RECT rcQ = {24, 216, rc.right - 24, 240};
        DrawTextW(hdc, L"How would you like to handle this request?", -1, &rcQ, DT_LEFT | DT_TOP | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_COMMAND: {
        if (HIWORD(wParam) == BN_CLICKED) {
            WORD id = LOWORD(wParam);
            if (id == ID_BTN_ALLOW) {
                state->choice = 1;
                DestroyWindow(hwnd);
            } else if (id == ID_BTN_BLOCK) {
                state->choice = 2;
                DestroyWindow(hwnd);
            } else if (id == ID_BTN_CLOSE) {
                state->choice = 2;
                DestroyWindow(hwnd);
            }
        }
        return 0;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (dis->CtlType != ODT_BUTTON) return TRUE;

        if (dis->CtlID == ID_BTN_CLOSE) {
            COLORREF bg = (dis->itemState & ODS_SELECTED) ? RGB(80, 80, 100) : RGB(50, 50, 70);
            urlscan::draw_rounded_rect(dis->hDC, dis->rcItem, 4, bg);
            SetTextColor(dis->hDC, RGB(200, 200, 200));
            SetBkMode(dis->hDC, TRANSPARENT);
            SelectObject(dis->hDC, state->fontClose);
            DrawTextW(dis->hDC, L"\u2715", -1, const_cast<LPRECT>(&dis->rcItem), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }

        COLORREF base, dark;
        if (dis->CtlID == ID_BTN_ALLOW) {
            base = RGB(40, 167, 69);
            dark = RGB(30, 126, 52);
        } else {
            base = RGB(220, 53, 69);
            dark = RGB(180, 40, 55);
        }

        COLORREF bg = (dis->itemState & ODS_SELECTED) ? dark : base;
        urlscan::draw_rounded_rect(dis->hDC, dis->rcItem, 6, bg);

        SetTextColor(dis->hDC, RGB(255, 255, 255));
        SetBkMode(dis->hDC, TRANSPARENT);
        SelectObject(dis->hDC, state->fontBtn);

        const wchar_t* label = (dis->CtlID == ID_BTN_ALLOW) ? L"Allow" : L"Block";
        DrawTextW(dis->hDC, label, -1, const_cast<LPRECT>(&dis->rcItem), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        if (dis->itemState & ODS_FOCUS) {
            RECT rcFocus = dis->rcItem;
            InflateRect(&rcFocus, -4, -4);
            DrawFocusRect(dis->hDC, &rcFocus);
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
        state->choice = 2;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

inline bool show_security_alert(const std::string& url, int score, double entropy) {
    std::lock_guard<std::mutex> lock(g_alert_mutex);

    SetProcessDPIAware();

    HINSTANCE hInst = GetModuleHandle(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = AlertWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"URLScannerAlert";
    wc.style = CS_DROPSHADOW;

    ATOM atom = RegisterClassExW(&wc);
    if (!atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    AlertState state;
    state.url = url;
    state.score = score;
    state.entropy = entropy;
    state.brushBg = CreateSolidBrush(RGB(30, 30, 46));

    const int width = 520;
    const int height = 320;

    RECT rcWork;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0);
    int x = rcWork.left + (rcWork.right - rcWork.left - width) / 2;
    int y = rcWork.top + (rcWork.bottom - rcWork.top - height) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST,
        L"URLScannerAlert", nullptr,
        WS_POPUP,
        x, y, width, height,
        nullptr, nullptr, hInst, &state);

    if (!hwnd) {
        DeleteObject(state.brushBg);
        if (atom) UnregisterClassW(L"URLScannerAlert", hInst);
        return false;
    }

    state.hwnd = hwnd;
    state.fontTitle = urlscan::create_font(24, FW_SEMIBOLD, L"Segoe UI");
    state.fontBody = urlscan::create_font(14, FW_NORMAL, L"Segoe UI");
    state.fontMono = urlscan::create_font(14, FW_NORMAL, L"Consolas");
    state.fontBtn = urlscan::create_font(15, FW_SEMIBOLD, L"Segoe UI");
    state.fontClose = urlscan::create_font(16, FW_NORMAL, L"Segoe UI");

    HRGN hRgn = CreateRoundRectRgn(0, 0, width, height, 12, 12);
    SetWindowRgn(hwnd, hRgn, TRUE);
    DeleteObject(hRgn);

    MessageBeep(MB_ICONEXCLAMATION);

    FLASHWINFO fi = {sizeof(fi), hwnd, FLASHW_ALL | FLASHW_TIMERNOFG, 3, 0};
    FlashWindowEx(&fi);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(state.fontTitle);
    DeleteObject(state.fontBody);
    DeleteObject(state.fontMono);
    DeleteObject(state.fontBtn);
    DeleteObject(state.fontClose);
    DeleteObject(state.brushBg);
    if (atom) UnregisterClassW(L"URLScannerAlert", hInst);

    return state.choice == 2;
}

#endif // GUI_HPP
