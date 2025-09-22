#pragma once
#ifdef _WIN32
#include <windows.h>
#endif
#include <string>
#include <cstdint>
#include "screen_capture_win.hpp"

namespace cortex {

// Minimal GDI live viewer window that displays BGRA frames.
class LiveViewerWin {
public:
    LiveViewerWin() = default;

    bool create(int width, int height, const std::string& title = "Cortex Live Viewer") {
#ifndef _WIN32
        (void)width; (void)height; (void)title;
        return false;
#else
        W = width; H = height;

        WNDCLASSA wc{};
        wc.lpfnWndProc = &LiveViewerWin::WndProcStatic;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = "CortexLiveViewerWnd";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassA(&wc);

        hwnd = CreateWindowA(wc.lpszClassName, title.c_str(),
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, width + 16, height + 39,
                             nullptr, nullptr, wc.hInstance, this);
        if (!hwnd) return false;

        ShowWindow(hwnd, SW_SHOW);

        // Create DIB section for blitting
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = W;
        bmi.bmiHeader.biHeight = -H; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        hdc = GetDC(hwnd);
        memdc = CreateCompatibleDC(hdc);
        bits = nullptr;
        hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        oldbmp = (HBITMAP)SelectObject(memdc, hbmp);
        return hbmp && bits;
#endif
    }

    void update(const RawImage& frame) {
#ifdef _WIN32
        if (!hwnd || !bits || !frame.ok()) return;

        // If size changed, recreate DIB
        if ((int)frame.width != W || (int)frame.height != H) {
            destroy_dib();
            W = static_cast<int>(frame.width);
            H = static_cast<int>(frame.height);
            bmi = {};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = W;
            bmi.bmiHeader.biHeight = -H;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            bits = nullptr;
            hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
            oldbmp = (HBITMAP)SelectObject(memdc, hbmp);
        }

        const size_t bytes = frame.width * frame.height * 4;
        std::memcpy(bits, frame.bgra.data(), bytes);

        RECT rc; GetClientRect(hwnd, &rc);
        StretchDIBits(hdc,
                      0, 0, rc.right - rc.left, rc.bottom - rc.top,
                      0, 0, W, H,
                      bits, &bmi, DIB_RGB_COLORS, SRCCOPY);
        // Process a few messages
        MSG msg;
        while (PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessageA(&msg);
        }
#else
        (void)frame;
#endif
    }

    void destroy() {
#ifdef _WIN32
        if (hwnd) {
            destroy_dib();
            if (hdc) { ReleaseDC(hwnd, hdc); hdc = nullptr; }
            DestroyWindow(hwnd);
            hwnd = nullptr;
        }
#endif
    }

    ~LiveViewerWin() { destroy(); }

private:
#ifdef _WIN32
    static LRESULT CALLBACK WndProcStatic(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
        if (msg == WM_NCCREATE) {
            CREATESTRUCTA* cs = reinterpret_cast<CREATESTRUCTA*>(lp);
            auto* self = reinterpret_cast<LiveViewerWin*>(cs->lpCreateParams);
            SetWindowLongPtrA(hWnd, GWLP_USERDATA, (LONG_PTR)self);
        }
        auto* self = reinterpret_cast<LiveViewerWin*>(GetWindowLongPtrA(hWnd, GWLP_USERDATA));
        if (self) return self->WndProc(hWnd, msg, wp, lp);
        return DefWindowProcA(hWnd, msg, wp, lp);
    }

    LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
            case WM_DESTROY: PostQuitMessage(0); return 0;
            default: return DefWindowProcA(hWnd, msg, wp, lp);
        }
    }

    void destroy_dib() {
        if (memdc) {
            if (oldbmp) { SelectObject(memdc, oldbmp); oldbmp = nullptr; }
            if (hbmp)   { DeleteObject(hbmp); hbmp = nullptr; }
            DeleteDC(memdc); memdc = nullptr;
        }
    }

    HWND hwnd{nullptr};
    HDC hdc{nullptr}, memdc{nullptr};
    BITMAPINFO bmi{};
    HBITMAP hbmp{nullptr}, oldbmp{nullptr};
    void* bits{nullptr};
    int W{0}, H{0};
#endif
};

} // namespace cortex