#!/usr/bin/env python3
import argparse
import glob
import os
import re
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import List, Tuple

NUM_RE = re.compile(r'(\d+)(?=\D*$)')  # last run of digits before non-digits / end

def natural_index(fname: str) -> int:
    """Extract trailing numeric index (frame_000123.bmp -> 123). Return -1 if none."""
    m = NUM_RE.search(Path(fname).stem)
    return int(m.group(1)) if m else -1

def list_images(directory: Path, pattern: str) -> List[Path]:
    pats = (directory / pattern).as_posix()
    files = [Path(p) for p in glob.glob(pats)]
    files = [f for f in files if f.is_file()]
    if not files:
        return []
    return files

def sort_by_index(files: List[Path]) -> List[Path]:
    with_index = [(natural_index(f.name), f) for f in files]
    # Filter out anything without an index
    with_index = [p for p in with_index if p[0] >= 0]
    with_index.sort(key=lambda x: x[0])
    return [f for _, f in with_index]

def sort_by_mtime(files: List[Path]) -> List[Path]:
    return sorted(files, key=lambda f: f.stat().st_mtime)

def expand_by_index(files: List[Path], fps: int) -> List[Path]:
    """
    Given unique frames (deduped) where filename indices represent the *original* frame numbers,
    repeat each file according to the gap to the next index (or 1 for the last).
    """
    if not files:
        return []
    expanded: List[Path] = []
    idxs = [natural_index(f.name) for f in files]
    for i, f in enumerate(files):
        cur_idx = idxs[i]
        next_idx = idxs[i+1] if i + 1 < len(files) else (cur_idx + 1)
        gap = max(1, next_idx - cur_idx)
        expanded.extend([f] * gap)
    return expanded

def expand_by_timestamp(files: List[Path], fps: int) -> List[Path]:
    """
    Repeat each file based on time delta to next (converted to frame count via fps).
    Last frame gets at least 1 repeat.
    """
    if not files:
        return []
    expanded: List[Path] = []
    mtimes = [f.stat().st_mtime for f in files]
    for i, f in enumerate(files):
        cur_t = mtimes[i]
        next_t = mtimes[i+1] if i + 1 < len(files) else (cur_t + 1.0 / fps)
        dt = max(0.0, next_t - cur_t)
        frames = max(1, round(dt * fps))
        expanded.extend([f] * frames)
    return expanded

def write_concat_manifest(expanded: List[Path], base_dir: Path, out_path: Path):
    """
    Writes a concat demuxer manifest where each line is:
      file 'relative/path/to/frame.bmp'
    (No duration lines; repeat lines handle timing.)
    """
    rel = []
    for f in expanded:
        rp = f.relative_to(base_dir)
        rel.append(rp)
    with open(out_path, 'w', encoding='utf-8') as mf:
        for r in rel:
            # Quote single quotes inside names (unlikely) by escaping
            s = str(r).replace("'", r"'\''")
            mf.write(f"file '{s}'\n")

def parse_args():
    ap = argparse.ArgumentParser(
        description="Reconstruct real-time frame sequence from deduped captures into an ffmpeg concat manifest."
    )
    ap.add_argument("--dir", required=True, help="Directory containing deduped image frames.")
    ap.add_argument("--pattern", default="frame_*.bmp", help="Glob pattern (default: frame_*.bmp).")
    ap.add_argument("--fps", type=int, default=30, help="Target playback FPS (original capture FPS).")
    ap.add_argument("--mode", choices=["index", "timestamp"], default="index",
                    help="index: use filename index gaps; timestamp: use file mtime gaps.")
    ap.add_argument("--output", "-o", default="", help="Optional explicit manifest path (.txt).")
    ap.add_argument("--dry-run", action="store_true", help="Show stats only; do not write manifest.")
    ap.add_argument("--limit", type=int, default=0, help="Optional: limit number of *unique* input frames processed.")
    return ap.parse_args()

def main():
    args = parse_args()
    base_dir = Path(args.dir).resolve()
    if not base_dir.is_dir():
        print(f"[ERROR] Directory not found: {base_dir}", file=sys.stderr)
        return 1

    files = list_images(base_dir, args.pattern)
    if not files:
        print(f"[WARN] No files matched pattern '{args.pattern}' in {base_dir}")
        return 0

    original_count = len(files)
    if args.mode == "index":
        files = sort_by_index(files)
    else:
        files = sort_by_mtime(files)

    if args.limit > 0:
        files = files[:args.limit]

    if not files:
        print("[WARN] No usable frames after filtering.")
        return 0

    if args.mode == "index":
        expanded = expand_by_index(files, args.fps)
    else:
        expanded = expand_by_timestamp(files, args.fps)

    timestamp_tag = datetime.now().strftime("%Y%m%d_%H%M%S")
    if args.output:
        out_path = Path(args.output)
    else:
        suffix = "_frames_index.txt" if args.mode == "index" else "_frames_timestamp.txt"
        out_path = base_dir / f"{timestamp_tag}{suffix}"

    print("=== Manifest Generation Summary ===")
    print(f" Base directory      : {base_dir}")
    print(f" Pattern             : {args.pattern}")
    print(f" Mode                : {args.mode}")
    print(f" FPS (target)        : {args.fps}")
    print(f" Unique frames (in)  : {len(files)} (raw matched: {original_count})")
    print(f" Expanded frame count: {len(expanded)}")
    print(f" Output manifest     : {out_path if not args.dry_run else '(dry-run)'}")

    if args.dry_run:
        return 0

    # Ensure parent exists if user supplied relative path outside base_dir
    out_path.parent.mkdir(parents=True, exist_ok=True)
    write_concat_manifest(expanded, base_dir, out_path)
    print(f"[OK] Wrote concat manifest: {out_path}")
    print("Next: ffmpeg -f concat -safe 0 -i \"{}\" -vf \"scale=trunc(iw/2)*2:trunc(ih/2)*2\" -c:v libx264 -pix_fmt yuv420p -crf 18 out.mp4".format(out_path))
    return 0

if __name__ == "__main__":
    sys.exit(main())