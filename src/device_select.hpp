#pragma once
#ifndef DEVICE_SELECT_HPP
#define DEVICE_SELECT_HPP

#include "gui_common.hpp"
#include <pcap.h>
#include <iostream>

#define ID_DEVICE_LIST  6001
#define ID_BTN_START    6002
#define ID_DEV_BTN_CLOSE 6003

struct DeviceSelectState {
    HWND hwnd = nullptr;
    HWND hList = nullptr;
    HWND btnStart = nullptr;
    HWND btnClose = nullptr;
    HFONT fontTitle = nullptr;
    HFONT fontBody = nullptr;
    HFONT fontBtn = nullptr;
    HBRUSH brushBg = nullptr;
    int result = -1;
};

static LRESULT CALLBACK DeviceSelectWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DeviceSelectState* state = nullptr;
    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        state = reinterpret_cast<DeviceSelectState*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    } else {
        state = reinterpret_cast<DeviceSelectState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!state) return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_CREATE: {
        HINSTANCE hInst = GetModuleHandle(nullptr);

        state->btnClose = CreateWindowW(L"BUTTON", L"\u2715",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            596, 16, 28, 28, hwnd, reinterpret_cast<HMENU>(ID_DEV_BTN_CLOSE), hInst, nullptr);

        state->hList = CreateWindowW(L"LISTBOX", nullptr,
            WS_CHILD | WS_VISIBLE | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOTIFY | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
            24, 88, 592, 280, hwnd, reinterpret_cast<HMENU>(ID_DEVICE_LIST), hInst, nullptr);

        state->btnStart = CreateWindowW(L"BUTTON", L"Start Capture",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            250, 384, 140, 40, hwnd, reinterpret_cast<HMENU>(ID_BTN_START), hInst, nullptr);
        return 0;
    }

    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* mis = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (mis->CtlType == ODT_LISTBOX) {
            mis->itemHeight = 40;
        }
        return TRUE;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (dis->CtlType == ODT_LISTBOX) {
            bool isSelected = (dis->itemState & ODS_SELECTED);
            bool isFocus = (dis->itemState & ODS_FOCUS);

            COLORREF bg = isSelected ? RGB(0, 100, 120) : RGB(40, 40, 60);
            HBRUSH brush = CreateSolidBrush(bg);
            FillRect(dis->hDC, &dis->rcItem, brush);
            DeleteObject(brush);

            // Recommended indicator strip on left
            wchar_t itemText[256];
            SendMessageW(dis->hwndItem, LB_GETTEXT, dis->itemID, (LPARAM)itemText);
            bool isRecommended = (wcsstr(itemText, L"Recommended") != nullptr);

            if (isRecommended && !isSelected) {
                RECT rcRec = {dis->rcItem.left, dis->rcItem.top, dis->rcItem.left + 4, dis->rcItem.bottom};
                HBRUSH recBrush = CreateSolidBrush(RGB(0, 180, 200));
                FillRect(dis->hDC, &rcRec, recBrush);
                DeleteObject(recBrush);
            }

            SetTextColor(dis->hDC, RGB(220, 220, 240));
            SetBkMode(dis->hDC, TRANSPARENT);
            HFONT font = urlscan::create_font(14, FW_NORMAL, L"Segoe UI");
            SelectObject(dis->hDC, font);
            RECT rcText = dis->rcItem;
            rcText.left += 14;
            rcText.right -= 12;
            DrawTextW(dis->hDC, itemText, -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            DeleteObject(font);

            if (isFocus) {
                RECT rcFocus = dis->rcItem;
                InflateRect(&rcFocus, -2, -2);
                DrawFocusRect(dis->hDC, &rcFocus);
            }
            return TRUE;
        }

        if (dis->CtlType == ODT_BUTTON) {
            if (dis->CtlID == ID_DEV_BTN_CLOSE) {
                COLORREF bg = (dis->itemState & ODS_SELECTED) ? RGB(80, 80, 100) : RGB(50, 50, 70);
                urlscan::draw_rounded_rect(dis->hDC, dis->rcItem, 4, bg);
                SetTextColor(dis->hDC, RGB(200, 200, 200));
                SetBkMode(dis->hDC, TRANSPARENT);
                SelectObject(dis->hDC, state->fontBody);
                DrawTextW(dis->hDC, L"\u2715", -1, const_cast<LPRECT>(&dis->rcItem), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                return TRUE;
            }

            if (dis->CtlID == ID_BTN_START) {
                COLORREF base = RGB(0, 180, 200);
                COLORREF dark = RGB(0, 140, 160);
                COLORREF bg = (dis->itemState & ODS_SELECTED) ? dark : base;
                urlscan::draw_rounded_rect(dis->hDC, dis->rcItem, 6, bg);
                SetTextColor(dis->hDC, RGB(255, 255, 255));
                SetBkMode(dis->hDC, TRANSPARENT);
                SelectObject(dis->hDC, state->fontBtn);
                DrawTextW(dis->hDC, L"Start Capture", -1, const_cast<LPRECT>(&dis->rcItem), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                return TRUE;
            }
        }
        return TRUE;
    }

    case WM_COMMAND: {
        WORD id = LOWORD(wParam);
        WORD code = HIWORD(wParam);
        if (id == ID_DEVICE_LIST && code == LBN_DBLCLK) {
            int sel = (int)SendMessageW(state->hList, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR) {
                state->result = sel;
                DestroyWindow(hwnd);
            }
        } else if (id == ID_BTN_START) {
            int sel = (int)SendMessageW(state->hList, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR) {
                state->result = sel;
                DestroyWindow(hwnd);
            }
        } else if (id == ID_DEV_BTN_CLOSE) {
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_ERASEBKGND: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, state->brushBg);
        return 1;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        FillRect(hdc, &rc, state->brushBg);

        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);
        SelectObject(hdc, state->fontTitle);
        RECT rcTitle = {24, 24, rc.right - 24, 56};
        DrawTextW(hdc, L"Select Network Adapter", -1, &rcTitle, DT_LEFT | DT_TOP | DT_SINGLELINE);

        SelectObject(hdc, state->fontBody);
        SetTextColor(hdc, RGB(180, 180, 200));
        RECT rcSub = {24, 60, rc.right - 24, 84};
        DrawTextW(hdc, L"Choose the interface to monitor for suspicious URLs.", -1, &rcSub, DT_LEFT | DT_TOP | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
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
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

inline int show_device_selector(pcap_if_t* alldevs) {
    SetProcessDPIAware();

    // Helper: classify adapter by description/name
    auto classify = [](pcap_if_t* d) -> std::string {
        std::string text;
        if (d->description) text = d->description;
        else if (d->name) text = d->name;
        for (char& c : text) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        return text;
    };

    // Find best adapter: skip Bluetooth, WAN Miniport, loopback, and APIPA
    int count = 0;
    int recommended = -1;
    for (pcap_if_t* d = alldevs; d != nullptr; d = d->next) {
        std::string info = classify(d);
        if (info.find("bluetooth") != std::string::npos ||
            info.find("wan miniport") != std::string::npos ||
            info.find("loopback") != std::string::npos) {
            count++;
            continue;
        }
        for (pcap_addr* addr = d->addresses; addr != nullptr; addr = addr->next) {
            if (addr->addr && addr->addr->sa_family == AF_INET) {
                struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(addr->addr);
                uint8_t* ip = reinterpret_cast<uint8_t*>(&sin->sin_addr);
                if (ip[0] != 127 && !(ip[0] == 169 && ip[1] == 254)) {
                    recommended = count;
                    break;
                }
            }
        }
        if (recommended != -1) break;
        count++;
    }

    HINSTANCE hInst = GetModuleHandle(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DeviceSelectWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"URLScannerDeviceSelect";
    wc.style = CS_DROPSHADOW;

    ATOM atom = RegisterClassExW(&wc);
    if (!atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return -1;
    }

    DeviceSelectState state;
    state.brushBg = CreateSolidBrush(RGB(30, 30, 46));

    const int width = 640;
    const int height = 440;

    RECT rcWork;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0);
    int x = rcWork.left + (rcWork.right - rcWork.left - width) / 2;
    int y = rcWork.top + (rcWork.bottom - rcWork.top - height) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST,
        L"URLScannerDeviceSelect", nullptr,
        WS_POPUP,
        x, y, width, height,
        nullptr, nullptr, hInst, &state);

    if (!hwnd) {
        DeleteObject(state.brushBg);
        if (atom) UnregisterClassW(L"URLScannerDeviceSelect", hInst);
        return -1;
    }

    state.hwnd = hwnd;
    state.fontTitle = urlscan::create_font(24, FW_SEMIBOLD, L"Segoe UI");
    state.fontBody = urlscan::create_font(14, FW_NORMAL, L"Segoe UI");
    state.fontBtn = urlscan::create_font(15, FW_SEMIBOLD, L"Segoe UI");

    // Populate list — strip Npcap's verbose wrapper so names fit
    int idx = 0;
    for (pcap_if_t* d = alldevs; d != nullptr; d = d->next) {
        std::wstring text;
        if (d->description) {
            std::string raw = d->description;
            size_t p = raw.find("Network adapter '");
            if (p != std::string::npos) {
                raw = raw.substr(p + 17); // skip prefix
            }
            size_t s = raw.find("' on local host");
            if (s != std::string::npos) {
                raw = raw.substr(0, s); // skip suffix
            }
            text = urlscan::ascii_to_wstring_direct(raw);
            std::cout << "[GUI] dev " << idx << " display: '" << raw << "'" << std::endl;
        }
        if (text.empty()) {
            text = urlscan::utf8_to_wstring(d->name);
        }
        if (idx == recommended) {
            text += L"  \u2014  Recommended";
        }
        SendMessageW(state.hList, LB_ADDSTRING, 0, (LPARAM)text.c_str());
        idx++;
    }

    if (recommended >= 0) {
        SendMessageW(state.hList, LB_SETCURSEL, recommended, 0);
    } else if (count > 0) {
        SendMessageW(state.hList, LB_SETCURSEL, 0, 0);
    }

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
    DeleteObject(state.fontBody);
    DeleteObject(state.fontBtn);
    DeleteObject(state.brushBg);
    if (atom) UnregisterClassW(L"URLScannerDeviceSelect", hInst);

    return state.result;
}

#endif // DEVICE_SELECT_HPP
