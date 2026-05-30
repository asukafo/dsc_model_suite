#!/usr/bin/env python3
"""DSC Verification: Compare C reference model trace vs SystemC perf model output."""

import subprocess, csv, os, sys, json, statistics
import numpy as np

ROOT   = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TB_DIR = os.path.join(ROOT, "..", "tb")
SC_DIR = ROOT
RES_DIR = os.path.join(ROOT, "traces")

def run_c_model(w=320, h=240, sh=48, bpc=8, bpp=8.0, pat="zones"):
    """Run C reference model with tracing, return trace CSV path."""
    os.makedirs(RES_DIR, exist_ok=True)
    prefix = os.path.join(RES_DIR, f"c_ref_{bpc}bpc_{int(bpp)}bpp_{pat}")
    tb_bin = os.path.join(TB_DIR, "dsc_tb_trace")
    if not os.path.exists(tb_bin):
        # Build if needed
        subprocess.run(["make", "-C", TB_DIR], capture_output=True)
    cmd = [tb_bin, "-p", pat, "-w", str(w), "-h", str(h), "-s", str(sh),
           "-bpc", str(bpc), "-bpp", str(bpp), "-o", prefix]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=120, cwd=TB_DIR)
    trace_path = prefix + "_trace.csv"
    return trace_path if os.path.exists(trace_path) else None

def run_sc_model(w=320, h=240, sh=48, bpc=8, bpp=8.0):
    """Run SystemC perf model, return trace CSV path."""
    os.makedirs(RES_DIR, exist_ok=True)
    trace_path = os.path.join(RES_DIR, f"sc_perf_{bpc}bpc_{int(bpp)}bpp.csv")
    sc_bin = os.path.join(SC_DIR, "dsc_perf")
    if not os.path.exists(sc_bin):
        subprocess.run(["make", "-C", SC_DIR], capture_output=True)
    env = os.environ.copy()
    env["DSC_PERF_TRACE"] = trace_path
    env["DYLD_LIBRARY_PATH"] = os.path.expanduser("~/opt/systemc-2.3.4/lib")
    r = subprocess.run([sc_bin], capture_output=True, text=True,
                       timeout=120, cwd=SC_DIR, env=env)
    return trace_path if os.path.exists(trace_path) else None

def load_trace(path):
    """Load trace CSV into list of dicts."""
    rows = []
    with open(path) as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append({k: int(v) for k, v in row.items()})
    return rows

def compare(c_trace, sc_trace):
    """Compare two traces, return comparison metrics."""
    c_qps  = [r["qp"] for r in c_trace]
    sc_qps = [r["qp"] for r in sc_trace]
    c_bits = [r["coded_bits"] for r in c_trace]
    sc_bits = [r["coded_bits"] for r in sc_trace]
    c_buf  = [r["buffer_fullness"] for r in c_trace]
    sc_buf = [r["buffer_fullness"] for r in sc_trace]

    report = {
        "c_groups": len(c_trace),
        "sc_groups": len(sc_trace),
        "qp": {
            "c_avg": statistics.mean(c_qps) if c_qps else 0,
            "sc_avg": statistics.mean(sc_qps) if sc_qps else 0,
            "c_min": min(c_qps) if c_qps else 0,
            "sc_min": min(sc_qps) if sc_qps else 0,
            "c_max": max(c_qps) if c_qps else 0,
            "sc_max": max(sc_qps) if sc_qps else 0,
        },
        "coded_bits": {
            "c_avg": statistics.mean(c_bits) if c_bits else 0,
            "sc_avg": statistics.mean(sc_bits) if sc_bits else 0,
            "c_total": sum(c_bits),
            "sc_total": sum(sc_bits),
        },
        "buffer": {
            "c_avg": statistics.mean(c_buf) if c_buf else 0,
            "sc_avg": statistics.mean(sc_buf) if sc_buf else 0,
        }
    }

    # QP distribution comparison (correlation)
    if len(c_qps) > 0 and len(sc_qps) > 0:
        min_len = min(len(c_qps), len(sc_qps))
        if min_len > 1:
            qp_corr = np.corrcoef(c_qps[:min_len], sc_qps[:min_len])[0,1]
            report["qp_correlation"] = float(qp_corr)
        else:
            report["qp_correlation"] = 0.0
    else:
        report["qp_correlation"] = 0.0

    return report

def print_report(report):
    print("\n" + "="*60)
    print("  DSC Verification Report: C Model vs SystemC Model")
    print("="*60)
    print(f"  Groups:  C={report['c_groups']}  SC={report['sc_groups']}")
    print(f"\n  QP:")
    print(f"    C:   avg={report['qp']['c_avg']:.1f}  [{report['qp']['c_min']}–{report['qp']['c_max']}]")
    print(f"    SC:  avg={report['qp']['sc_avg']:.1f}  [{report['qp']['sc_min']}–{report['qp']['sc_max']}]")
    print(f"    Correlation: {report['qp_correlation']:.3f}")
    print(f"\n  Coded bits/group:")
    print(f"    C:   avg={report['coded_bits']['c_avg']:.1f}  total={report['coded_bits']['c_total']}")
    print(f"    SC:  avg={report['coded_bits']['sc_avg']:.1f}  total={report['coded_bits']['sc_total']}")
    print(f"\n  Buffer fullness:")
    print(f"    C:   avg={report['buffer']['c_avg']:.1f}")
    print(f"    SC:  avg={report['buffer']['sc_avg']:.1f}")

    # Verdict
    qp_ok = abs(report['qp']['c_avg'] - report['qp']['sc_avg']) < 5.0
    print(f"\n  QP match:  {'PASS' if qp_ok else 'WARN (avg diff > 5)'}")
    print("="*60)

def main():
    print("DSC Verification: C Model vs SystemC Perf Model")
    c_trace_path = run_c_model()
    if not c_trace_path:
        print("ERROR: C model trace failed"); sys.exit(1)
    print(f"C model trace: {c_trace_path}")

    sc_trace_path = run_sc_model()
    if not sc_trace_path:
        print("ERROR: SC model trace failed"); sys.exit(1)
    print(f"SC model trace: {sc_trace_path}")

    c_data  = load_trace(c_trace_path)
    sc_data = load_trace(sc_trace_path)
    report  = compare(c_data, sc_data)
    print_report(report)

    # Save report
    with open(os.path.join(RES_DIR, "verify_report.json"), "w") as f:
        json.dump(report, f, indent=2)

if __name__ == "__main__":
    main()
