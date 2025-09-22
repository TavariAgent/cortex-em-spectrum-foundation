#pragma once
#ifdef _WIN32
#include <windows.h>
#endif
#include <vector>
#include <cstdint>
#include <string>
#include <iostream>

namespace cortex {

    struct RawImage {
        size_t width{0}, height{0};
        // BGRA, row-major, 4 bytes per pixel
        std::vector<uint8_t> bgra;
        bool ok() const { return width && height && bgra.size() == width*height*4; }
    };

    inline RawImage capture_primary_monitor_bgra() {
        RawImage out;

#ifndef _WIN32
        std::cout << "screen_capture_win.hpp: Non-Windows platform, capture stub.\n";
        return out;
#else
        const int w = GetSystemMetrics(SM_CXSCREEN);
        const int h = GetSystemMetrics(SM_CYSCREEN);
        if (w <= 0 || h <= 0) return out;

        const HDC hScreen = GetDC(nullptr);
        if (!hScreen) return out;

        const HDC hMem = CreateCompatibleDC(hScreen);
        if (!hMem) { ReleaseDC(nullptr, hScreen); return out; }

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h; // top-down DIB
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32; // BGRA
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        const HBITMAP hDIB = CreateDIBSection(hScreen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!hDIB || !bits) {
            if (hDIB) DeleteObject(hDIB);
            DeleteDC(hMem);
            ReleaseDC(nullptr, hScreen);
            return out;
        }

        const HGDIOBJ old = SelectObject(hMem, hDIB);
        const BOOL ok = BitBlt(hMem, 0, 0, w, h, hScreen, 0, 0, SRCCOPY | CAPTUREBLT);
        (void)ok;

        out.width = static_cast<size_t>(w);
        out.height = static_cast<size_t>(h);
        out.bgra.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
        memcpy(out.bgra.data(), bits, out.bgra.size());

        // Cleanup
        SelectObject(hMem, old);
        DeleteObject(hDIB);
        DeleteDC(hMem);
        ReleaseDC(nullptr, hScreen);

        return out;
#endif
    }

} // namespace cortex