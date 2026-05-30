#!/usr/bin/env python3
"""DSC Configuration Scanner — runs C model tb across bpc/bpp/pattern combos,
   collects results, and optionally compares with SystemC perf model output."""

import subprocess, json, csv, os, sys
from itertools import product

TB_DIR  = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TB_BIN  = os.path.join(TB_DIR, "dsc_tb")
OUT_DIR = os.path.join(TB_DIR, "results")

# Scan configurations
SCAN = {
    "bpc":   [8, 10, 12],
    "bpp":   [8, 10, 12],
    "pattern": ["bars", "ramp", "zones", "random"],
    "pic_w":  [640],
    "pic_h":  [480],
    "slice_h":[48],
}

def run_one(bpc, bpp, pat, w, h, sh):
    """Run dsc_tb with given config, return parsed results."""
    out_csv = os.path.join(OUT_DIR, f"scan_{bpc}bpc_{bpp}bpp_{pat}.csv")
    cmd = [TB_BIN, "-bpc", str(bpc), "-bpp", str(bpp),
           "-w", str(w), "-h", str(h), "-s", str(sh),
           "-p", pat, "-o", out_csv]
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=60, cwd=TB_DIR)
        # Parse output
        result = {"bpc":bpc, "bpp":bpp, "pattern":pat,
                  "w":w, "h":h, "slice_h":sh, "status":"OK"}
        for line in r.stdout.split("\n"):
            try:
                if "PSNR (roundtrip)" in line:
                    result["psnr"] = float(line.strip().split(":")[-1].strip().split()[0])
                elif "Compression:" in line:
                    # "Compression:     3.0 : 1"
                    result["compression"] = float(line.strip().split(":")[1].strip().split()[0])
                elif "Total enc bits:" in line:
                    parts = line.strip().split()
                    result["total_bits"]  = int(parts[3])
                    result["total_bytes"] = int(parts[4].strip("()"))
                elif "Max diff:" in line:
                    import re
                    m = re.search(r'R=(\d+)\s+G=(\d+)\s+B=(\d+)', line)
                    if m:
                        result["max_diff_r"] = int(m.group(1))
                        result["max_diff_g"] = int(m.group(2))
                        result["max_diff_b"] = int(m.group(3))
                elif "PASS" in line and "---" not in line:
                    result["pass"] = True
                elif "FAIL" in line and "---" not in line:
                    result["pass"] = False
                elif "Slices:" in line:
                    result["num_slices"] = int(line.strip().split()[1])
            except (IndexError, ValueError) as e:
                print(f"  parse warning: {e} in line: {line.strip()[:80]}", file=sys.stderr)
        return result
    except subprocess.TimeoutExpired:
        return {"bpc":bpc,"bpp":bpp,"pattern":pat,"status":"TIMEOUT"}
    except Exception as e:
        return {"bpc":bpc,"bpp":bpp,"pattern":pat,"status":f"ERROR:{e}"}

def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    if not os.path.exists(TB_BIN):
        print(f"ERROR: {TB_BIN} not found. Run 'make' in {TB_DIR} first.")
        sys.exit(1)

    results = []
    combos = list(product(SCAN["bpc"], SCAN["bpp"], SCAN["pattern"],
                          SCAN["pic_w"], SCAN["pic_h"], SCAN["slice_h"]))

    print(f"DSC Configuration Scan — {len(combos)} combinations")
    print(f"{'bpc':>3s} {'bpp':>4s} {'pat':>8s} {'PSNR':>8s} {'Ratio':>6s} {'MaxDiff':>10s} {'Status'}")
    print("-" * 65)

    for bpc, bpp, pat, w, h, sh in combos:
        r = run_one(bpc, bpp, pat, w, h, sh)
        results.append(r)

        psnr  = f"{r.get('psnr',0):.1f}dB" if 'psnr' in r else "N/A"
        ratio = f"{r.get('compression',0):.1f}:1" if 'compression' in r else "N/A"
        md    = f"{r.get('max_diff_r',0)},{r.get('max_diff_g',0)},{r.get('max_diff_b',0)}" if 'max_diff_r' in r else "N/A"
        ok    = "PASS" if r.get('pass') else ("FAIL" if 'pass' in r else r.get('status','?'))
        print(f"{bpc:3d} {bpp:4.1f} {pat:>8s} {psnr:>8s} {ratio:>6s} {md:>10s} {ok}")

    # Save summary
    summary = os.path.join(OUT_DIR, "scan_summary.json")
    with open(summary, "w") as f:
        json.dump(results, f, indent=2)
    print(f"\nSummary saved: {summary}")

    # Quick stats
    passed = sum(1 for r in results if r.get('pass'))
    print(f"Passed: {passed}/{len(results)}")

if __name__ == "__main__":
    main()
