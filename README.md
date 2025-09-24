# 🌈 Cortex Electromagnetic Spectrum Foundation

**Where EM sight combines LLM based compute.**

## Python Optimization Foundations

This project translates advanced Python optimization algorithms to C++:

### 💎 Core Optimization Systems:
- **🧠 Intelligent Term Delegator** → Complexity-based routing (2-group delegation)
- **🎯 Precision Safe Threading** → Zero-drift 141-decimal threading
- **🤯 Context Overflow Guard** → Recursive self-capturing protection
- **🚀 Adaptive GPU Delegation** → GTX 1060 vs RTX 4070 Super optimization
- **🖼️ Static Frame Generator** → Electromagnetic spectrum visual processing

**Build and run (Windows, Ninja)**
* Configure
`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`

* Build CLI
`cmake --build build --target cortex_em_cli -j`

* Run CLI
`build\cortex_em_cli.exe`

* Options (CMake cache variables)
_CORTEX_EM_SPECTRUM_BUILD_CLI: Build CLI sample (default ON)_
`-DCORTEX_EM_SPECTRUM_BUILD_CLI=OFF to disable`
_CORTEX_EM_SPECTRUM_BUILD_VIZ: Build visualization sample (default ON)_
`-DCORTEX_EM_SPECTRUM_BUILD_VIZ=OFF to disable`
_CORTEX_EM_SPECTRUM_ENABLE_CUDA: Enable CUDA build (default OFF)_

**Example:**
`-DCORTEX_EM_SPECTRUM_ENABLE_CUDA=ON`
`-DCUDAToolkit_ROOT="C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.9"`
`-DCMAKE_CUDA_COMPILER="C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.9/bin/nvcc.exe"`
`-DCMAKE_CUDA_ARCHITECTURES=61`

_CORTEX_EM_SPECTRUM_PRECISION: CosmicPrecision digits (default 141)_
`-DCORTEX_EM_SPECTRUM_PRECISION=141`

**Windows capture linkage**
The CLI links required desktop libraries:
`user32 gdi32`
_The capture shim implementation is compiled into the CLI target._
_If you later switch to DXGI/D3D capture, also link dxgi d3d11 (and initialize COM)._

**Common one-liners**
Clean configure + build + run (Release):
`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`
`cmake --build build --target cortex_em_cli -j`
`.\build\cortex_em_cli.exe`
`.\build\cortex_em_cli.exe --capture 1 --live --seconds 10 --fps 30 --resize 1680x720 --record capture\frame`

With CUDA enabled:
`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCORTEX_EM_SPECTRUM_ENABLE_CUDA=ON -DCUDAToolkit_ROOT="C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.9" -DCMAKE_CUDA_COMPILER="C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.9/bin/nvcc.exe" -DCMAKE_CUDA_ARCHITECTURES=61`
`cmake --build build --target cortex_em_cli -j`
`.\build\cortex_em_cli.exe`

**Install**
`cmake --install build --prefix install`
_Installs built executables into install/bin_

## Convert captured frames to video (preserve real-time)

By default, capture dedupes static frames (only saves unique images). To make a normal-speed video, build an ffmpeg concat manifest that encodes each unique frame for the right duration (using frame index gaps), then encode.

1) Generate the manifest (from the `captures` folder):
```powershell
Set-Location F:\cortex-em-spectrum-foundation\captures
python ..\python-helpers\create_manifest.py --dir . --pattern "frame_*.bmp" --fps 30 --mode index
```

This writes a file like `2025YYYY_MMDDHHMMSS_frames_index.txt`.

2) Encode to MP4 (preserves timing):
```powershell
$mf = Get-ChildItem *_frames_index.txt | Sort-Object LastWriteTime -Descending | Select-Object -ExpandProperty FullName -First 1
ffmpeg -f concat -safe 0 -i "$mf" -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -pix_fmt yuv420p -crf 18 out_index.mp4
```

(Single-line)
`$mf = Get-ChildItem *_frames_index.txt | Sort-Object LastWriteTime -Descending | Select-Object -ExpandProperty FullName -First 1; ffmpeg -f concat -safe 0 -i "$mf" -c:v libx264 -pix_fmt yuv420p -crf 18 out_index.mp4`

Notes
- If you prefer spacing by file timestamps, generate a time-based manifest instead:
  ```
  python ..\python-helpers\create_manifest.py --dir . --pattern "frame_*.bmp" --fps 30 --mode timestamp
  ```
- The dedupe behavior keeps capture light and fast; the manifest reconstructs static spans into the right durations so your video doesn’t “accelerate.”

**Notes**
Use `-DCMAKE_BUILD_TYPE=Debug` for debugging.
If Ninja isn’t available, use your preferred generator (e.g., -G "Visual Studio 17 2022").

![img.png](img.png)