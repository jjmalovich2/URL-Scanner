#pragma once
#ifndef GUI_COMMON_HPP
#define GUI_COMMON_HPP

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <windows.h>
#include <string>

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

namespace urlscan {
    inline std::wstring utf8_to_wstring(const std::string& str) {
        if (str.empty()) return {};
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
        if (size_needed <= 0) return {};
        std::wstring wstr(size_needed - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wstr.data(), size_needed);
        return wstr;
    }

    inline HFONT create_font(int height, int weight, const wchar_t* face) {
        return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, face);
    }

    inline void draw_rounded_rect(HDC hdc, const RECT& rc, int radius, COLORREF fill) {
        HBRUSH brush = CreateSolidBrush(fill);
        HGDIOBJ old_brush = SelectObject(hdc, brush);
        HPEN pen = CreatePen(PS_SOLID, 1, fill);
        HGDIOBJ old_pen = SelectObject(hdc, pen);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
        SelectObject(hdc, old_brush);
        SelectObject(hdc, old_pen);
        DeleteObject(brush);
        DeleteObject(pen);
    }
}

#endif // GUI_COMMON_HPP
