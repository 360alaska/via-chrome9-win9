#!/usr/bin/env python3
"""Summarize VXTLP CSV or the bundled D3D8PROF's Present wall-time log.

python tools/frame_stats.py A.CSV B.CSV
python tools/frame_stats.py A.TXT B.TXT --skip 300 --min-draws 200
Percentiles use nearest rank. Slow frames are retained; no outlier trimming.
"""
import argparse
import csv
import io
import json
import math
import pathlib
import re


def read_frames(path):
    text = path.read_text(encoding='utf-8-sig', errors='replace')
    if text.startswith('frame,ms'):
        return [(int(r['frame']), float(r['ms']), None) for r in csv.DictReader(io.StringIO(text))]
    rx = re.compile(r'^F (\d+) wall ([\d.]+).*?\| draws (\d+)\b', re.M)
    return [(int(f), float(ms), int(draws)) for f, ms, draws in rx.findall(text)]


def summarize(path, skip=0, min_draws=0):
    frames = read_frames(path)
    if not frames:
        raise ValueError('%s: no frames' % path)
    if any(b[0] <= a[0] for a, b in zip(frames, frames[1:])):
        raise ValueError('%s: non-increasing frame IDs; use one run per file' % path)
    if any(not math.isfinite(ms) or ms < 0 for _, ms, _ in frames):
        raise ValueError('%s: invalid frame time' % path)
    if min_draws and any(d is None for _, _, d in frames):
        raise ValueError('--min-draws requires a D3D8PROF log')
    kept = [ms for _, ms, draws in frames[skip:] if not min_draws or draws >= min_draws]
    if not kept:
        raise ValueError('%s: no frames after filters' % path)
    v = sorted(kept)
    return dict(file=str(path), frames_read=len(frames), skip_first=skip, min_draws=min_draws,
                frames_kept=len(v), mean_ms=sum(v) / len(v), median_ms=v[math.ceil(len(v)*.5)-1],
                p95_ms=v[math.ceil(len(v)*.95)-1], max_ms=v[-1],
                definition='Present wall intervals; nearest-rank percentiles; no outlier removal')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('files', nargs='+', type=pathlib.Path)
    p.add_argument('--skip', type=int, default=0, help='discard this many initial records before draw filtering')
    p.add_argument('--min-draws', type=int, default=0)
    a = p.parse_args()
    if a.skip < 0 or a.min_draws < 0:
        p.error('filters must be nonnegative')
    print(json.dumps([summarize(f, a.skip, a.min_draws) for f in a.files], indent=2))
