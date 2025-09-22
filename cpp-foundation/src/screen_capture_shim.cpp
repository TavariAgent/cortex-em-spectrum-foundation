#include <vector>
#include <cstdint>
#include <cstring>
#include <iostream>

#ifdef _WIN32
  #include <windows.h>
#endif

#include "screen_capture_win.hpp" // for cortex::RawImage

namespace cortex {

    RawImage capture_primary_monitor_bgra() {
        RawImage out;

#ifndef _WIN32
        std::cout << "screen_capture_shim: Non-Windows platform, capture stub.\n";
        return out;
#else
        const int w = GetSystemMetrics(SM_CXSCREEN);
        const int h = GetSystemMetrics(SM_CYSCREEN);
        if (w <= 0 || h <= 0) return out;

        HDC hScreen = GetDC(nullptr);
        if (!hScreen) return out;

        HDC hMem = CreateCompatibleDC(hScreen);
        if (!hMem) {
            ReleaseDC(nullptr, hScreen);
            return out;
        }

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = w;
        bmi.bmiHeader.biHeight      = -h; // top-down DIB
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32; // BGRA
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HBITMAP hDIB = CreateDIBSection(hScreen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!hDIB || !bits) {
            if (hDIB) DeleteObject(hDIB);
            DeleteDC(hMem);
            ReleaseDC(nullptr, hScreen);
            return out;
        }

        HGDIOBJ old = SelectObject(hMem, hDIB);
        BitBlt(hMem, 0, 0, w, h, hScreen, 0, 0, SRCCOPY | CAPTUREBLT);

        out.width  = static_cast<size_t>(w);
        out.height = static_cast<size_t>(h);
        out.bgra.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
        std::memcpy(out.bgra.data(), bits, out.bgra.size());

        SelectObject(hMem, old);
        DeleteObject(hDIB);
        DeleteDC(hMem);
        ReleaseDC(nullptr, hScreen);

        return out;

#endif
    }

} // namespace cortex