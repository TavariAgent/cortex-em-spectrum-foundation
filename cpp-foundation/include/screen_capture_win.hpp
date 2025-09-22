#pragma once
#ifdef _WIN32
#include <windows.h>
#endif
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <optional>
#include <iostream>

namespace cortex {

struct RawImage {
    size_t width{0}, height{0};        // pixels
    // BGRA 8:8:8:8 row-major, top-down, stride = width*4
    std::vector<uint8_t> bgra;
    [[nodiscard]] bool ok() const { return width && height && bgra.size() == width * height * 4; }
};

struct MonitorInfo {
    std::string device_name;   // e.g. "\\\\.\\DISPLAY1"
    bool primary{false};
    int x{0}, y{0};            // position in virtual screen coords
    int width{0}, height{0};
};

#ifdef _WIN32

inline std::vector<MonitorInfo> enumerate_monitors() {
    std::vector<MonitorInfo> out;

    for (DWORD i = 0;; ++i) {
        DISPLAY_DEVICE dd{};
        dd.cb = sizeof(dd);
        if (!EnumDisplayDevicesA(nullptr, i, &dd, 0)) break;
        if (!(dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) continue;

        DEVMODEA dm{};
        dm.dmSize = sizeof(dm);
        if (!EnumDisplaySettingsExA(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm, 0)) continue;

        MonitorInfo mi;
        mi.device_name = dd.DeviceName; // "\\\\.\\DISPLAY1"
        mi.primary = (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0;
        mi.x = dm.dmPosition.x;
        mi.y = dm.dmPosition.y;
        mi.width = static_cast<int>(dm.dmPelsWidth);
        mi.height = static_cast<int>(dm.dmPelsHeight);
        out.push_back(mi);
    }

    return out;
}

// display_index: 1 => "\\\\.\\DISPLAY1", 2 => "\\\\.\\DISPLAY2" ...
inline std::optional<MonitorInfo> get_monitor_by_display_index(int display_index) {
    if (display_index <= 0) return std::nullopt;
    auto mons = enumerate_monitors();
    std::string wanted = "\\\\.\\DISPLAY" + std::to_string(display_index);
    for (const auto& m : mons) {
        if (_stricmp(m.device_name.c_str(), wanted.c_str()) == 0) return m;
    }
    return std::nullopt;
}

inline RawImage capture_monitor_bgra_by_display_index(int display_index) {
    RawImage out;
#ifndef _WIN32
    std::cout << "screen_capture_win: Non-Windows platform, capture not available.\n";
    return out;
#else
    auto mon = get_monitor_by_display_index(display_index);
    if (!mon) {
        std::cout << "screen_capture_win: DISPLAY" << display_index << " not found.\n";
        return out;
    }

    HDC hSrc = CreateDCA(mon->device_name.c_str(), mon->device_name.c_str(), nullptr, nullptr);
    if (!hSrc) {
        std::cout << "screen_capture_win: CreateDC failed for " << mon->device_name << "\n";
        return out;
    }

    HDC hMem = CreateCompatibleDC(hSrc);
    if (!hMem) { DeleteDC(hSrc); return out; }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = mon->width;
    bmi.bmiHeader.biHeight = -mon->height; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32; // BGRA
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hDIB = CreateDIBSection(hSrc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hDIB || !bits) {
        if (hDIB) DeleteObject(hDIB);
        DeleteDC(hMem);
        DeleteDC(hSrc);
        return out;
    }

    HGDIOBJ old = SelectObject(hMem, hDIB);
    // note: capture from source origin (0,0) for that device DC
    BOOL ok = BitBlt(hMem, 0, 0, mon->width, mon->height, hSrc, 0, 0, SRCCOPY | CAPTUREBLT);
    (void)ok;

    out.width = static_cast<size_t>(mon->width);
    out.height = static_cast<size_t>(mon->height);
    out.bgra.resize(out.width * out.height * 4);
    std::memcpy(out.bgra.data(), bits, out.bgra.size());

    // cleanup
    SelectObject(hMem, old);
    DeleteObject(hDIB);
    DeleteDC(hMem);
    DeleteDC(hSrc);

    return out;
#endif
}

#else

inline std::vector<MonitorInfo> enumerate_monitors() { return {}; }
inline std::optional<MonitorInfo> get_monitor_by_display_index(int) { return std::nullopt; }
inline RawImage capture_monitor_bgra_by_display_index(int) { return {}; }

#endif // _WIN32

} // namespace cortex