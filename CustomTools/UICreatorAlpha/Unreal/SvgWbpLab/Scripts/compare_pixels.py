"""Compare frozen browser PNGs to a fresh native Slate readback; zero tolerance."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument("bundle", type=Path)
parser.add_argument("--native", type=Path, help="Directory containing fresh native captures")
parser.add_argument("--output", type=Path, help="JSON result path")
args = parser.parse_args()
bundle = json.loads(args.bundle.read_text(encoding="utf-8-sig"))
root = args.bundle.parent
native_root = args.native or root / "native"
results = []
references = {**bundle["references"], "designer": bundle["references"]["canvas"]}
for state, reference in references.items():
    expected_file = root / reference
    actual_file = native_root / f"{state}.png"
    expected = np.array(Image.open(expected_file).convert("RGBA"))
    actual = np.array(Image.open(actual_file).convert("RGBA"))
    if expected.shape != actual.shape:
        raise SystemExit(f"Size mismatch: {state}: {expected.shape} vs {actual.shape}")
    difference = np.abs(actual.astype(np.int16) - expected.astype(np.int16))
    changed = difference.max(axis=2) > 0
    rgb_changed = difference[:, :, :3].max(axis=2) > 0
    ys, xs = np.where(changed)
    result = dict(state=state, pixels=int(changed.size), changedPixels=int(changed.sum()),
                  changedRgbPixels=int(rgb_changed.sum()), maxChannelError=int(difference.max()),
                  meanAbsoluteError=float(difference.mean()), exact=bool(not changed.any()),
                  referenceSha256=hashlib.sha256(expected_file.read_bytes()).hexdigest(),
                  nativeSha256=hashlib.sha256(actual_file.read_bytes()).hexdigest(),
                  differenceBounds=([int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())] if len(xs) else None))
    heat = np.zeros_like(expected)
    heat[:, :, 0] = np.minimum(difference[:, :, :3].max(axis=2) * 8, 255)
    heat[:, :, 3] = 255
    Image.fromarray(heat).save(native_root / f"{state}-diff.png")
    results.append(result)
report = dict(sourceHash=bundle["sourceHash"], viewport=bundle["viewport"],
              exact=all(row["exact"] for row in results), tolerance=0, states=results)
(args.output or root / "pixel-report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
print(json.dumps(report, indent=2))
raise SystemExit(0 if report["exact"] else 1)
