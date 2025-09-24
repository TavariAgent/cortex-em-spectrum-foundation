# ================================================================ Current CLI Flags (capture main)

`--capture N` _(required to enter capture mode; N = display index, usually 1)_ `--live` Show a live window 
`--fps F Target capture FPS` **(default 30)** `--seconds S Duration;` **0 or negative => single snapshot frame** `--resize WxH` Resize each captured frame before processing
`--record` **base Save non-duplicate frames as base_000000.bmp** `--no-static-gate` **Disable static scene preflight (default gate ON)** `--static-sec` **X Required continuous stability seconds (default 1.0)** 
`--static-timeout T` **Abort static wait after T seconds (default 10.0)** `--static-tolerant` **Use signature (looser) equality in static detection (allows tiny noise)** 
`--filters` **Enables filter chain (if any specific filter flags are also present)** `--grayscale` **Persistent grayscale correction (also implies filters)** `--gamma G` **Apply gamma correction (e.g. 2.2)** 
`--brightness B` **Add brightness ([-1.0, 1.0]; 0.2 = +20% approx)** `--contrast C` **Multiply contrast (1.0 = identity; 1.2 = +20%)**

_(Width/height leading args are ignored in pure capture mode—you only hit spectrum render path if you omit --capture.)_

### ================================================================ 1. Minimal “it works” Command

**Capture 5 seconds of DISPLAY1 at 30 FPS, show window:** `cortex_em_cli --capture 1 --live`

**Same, but explicit duration and record frames (deduped) to captures/frame_*.bmp:** `cortex_em_cli --capture 1 --live --seconds 5 --fps 30 --record captures/frame`

### ================================================================ 2. Static Scene Gate Variants

**Default gate (require 1s stability, timeout 10s is already default):** `cortex_em_cli --capture 1 --live`

**Stricter gate (need 2.5 seconds stable):** `cortex_em_cli --capture 1 --static-sec 2.5 --static-timeout 15 --live`

**Disable gate entirely:** `cortex_em_cli --capture 1 --no-static-gate --live`

**Tolerant mode (ignore micro-pixel flicker):** `cortex_em_cli --capture 1 --static-tolerant --static-sec 1.0 --live`

### ================================================================ 3. Resizing & Higher FPS

Resize to 1280x720, 60 FPS, record unique frames: cortex_em_cli --capture 1 --fps 60 --seconds 10 --resize 1280x720 --record captures/frame

**Snapshot (one frame only) resized to 1024x640:** `cortex_em_cli --capture 1 --seconds 0 --resize 1024x640 --record snapshots/snap`

### ================================================================ 4. Filters / Corrections (Continuous)

**Grayscale live preview with modest contrast and gamma:** `cortex_em_cli --capture 1 --live --seconds 8 --grayscale --contrast 1.15 --gamma 2.0`

**Brightness + contrast tweak only:** `cortex_em_cli --capture 1 --seconds 5 --brightness 0.1 --contrast 1.2`

_(If you want filters applied even without specifying individual ones, use --filters plus the parameters.)_

================================================================ 7. Understanding “Duplicate frames skipped”
**You’ll see:** Capture complete. Duplicate frames skipped=NN That counter increments when a frame’s computed operand map + byte match equals the previous frame _(after resize & filters)._ 
Only non‑identical frames are written with --record. LLMFramePool also coalesces identical runs internally _(for future streaming / export)._