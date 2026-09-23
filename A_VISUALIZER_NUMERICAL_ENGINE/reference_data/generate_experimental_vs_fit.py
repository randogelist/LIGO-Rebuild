#!/usr/bin/env python3
import math
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "generated"
OUT.mkdir(exist_ok=True)

H1_URL = "https://gwosc.org/GW150914data/P1500229/H1_reconstructions.txt"
L1_URL = "https://gwosc.org/GW150914data/P1500229/L1_reconstructions.txt"
PEAK_GPS = 1126259462.42
FIT = {
    "label": "v0.4.0 primary two-anchor fit",
    "M_total_msun": 72.9306,
    "eta": 0.1352,
    "M_chirp_msun": 21.9496,
    "M1_msun": 61.1797,
    "M2_msun": 11.7509,
    "M_final_msun": 71.6953,
    "a_final": 0.4067,
    "f220_hz": 199.9978,
    "tau_s": 0.0040,
    "H1_to_L1_delay_s": 0.00732421875,
    "H1_to_L1_scale": -0.710068045028,
}
MSUN_SEC = 4.92549095e-6


def download(url: str, path: Path):
    if path.exists():
        return
    print(f"DOWNLOADING={url}")
    urllib.request.urlretrieve(url, path)


def parse_reconstruction(path: Path):
    t = []
    cwb = []
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            t.append(float(parts[0]))
            cwb.append(float(parts[3]))
    return t, cwb


def interp_linear(xs, ys, x):
    if x <= xs[0] or x >= xs[-1]:
        return 0.0
    lo, hi = 0, len(xs) - 1
    while hi - lo > 1:
        m = (lo + hi) // 2
        if xs[m] <= x:
            lo = m
        else:
            hi = m
    x0, x1 = xs[lo], xs[hi]
    y0, y1 = ys[lo], ys[hi]
    a = (x - x0) / (x1 - x0)
    return y0 * (1 - a) + y1 * a


def make_coherent(h1_t, h1_y, l1_t, l1_y):
    d = FIT["H1_to_L1_delay_s"]
    s = FIT["H1_to_L1_scale"]
    t_rel = []
    y = []
    for th, yh in zip(h1_t, h1_y):
        tr = th - PEAK_GPS
        if tr < -0.22 or tr > 0.03:
            continue
        yl = interp_linear(l1_t, l1_y, th - d)
        yc = 0.5 * (yh + s * yl)
        t_rel.append(tr)
        y.append(yc)
    return t_rel, y


def base_model_phase_amp(t_rel):
    mc_sec = FIT["M_chirp_msun"] * MSUN_SEC
    tmatch = -0.003
    fm = FIT["f220_hz"]
    fmatch = (1.0 / math.pi) * ((5.0 / (256.0 * (mc_sec ** (5.0 / 3.0)) * (-tmatch))) ** (3.0 / 8.0))
    freqs = []
    amps = []
    for t in t_rel:
        if t <= tmatch:
            fi = (1.0 / math.pi) * ((5.0 / (256.0 * (mc_sec ** (5.0 / 3.0)) * max(1e-6, -t))) ** (3.0 / 8.0))
            fi = min(fi, fm)
            ai = (fi / 40.0) ** (2.0 / 3.0)
        elif t < 0.0:
            u = (t - tmatch) / (0.0 - tmatch)
            fi = (1 - u) * fmatch + u * fm
            ai = (1 - u) * (fmatch / 40.0) ** (2.0 / 3.0) + u * 1.9
        else:
            fi = fm
            ai = 1.9 * math.exp(-t / FIT["tau_s"])
        freqs.append(fi)
        amps.append(ai)
    phase = [0.0] * len(t_rel)
    for i in range(1, len(t_rel)):
        dt = t_rel[i] - t_rel[i - 1]
        phase[i] = phase[i - 1] + 2.0 * math.pi * 0.5 * (freqs[i] + freqs[i - 1]) * dt
    return freqs, amps, phase


def model_with_shift_and_phase(t_grid, dt_shift, phi0):
    shifted = [t - dt_shift for t in t_grid]
    _, amps, phase = base_model_phase_amp(shifted)
    vals = []
    ring_phase0 = None
    for t, a, p in zip(shifted, amps, phase):
        if t < 0.0:
            vals.append(a * math.cos(p + phi0))
            ring_phase0 = p + phi0
        else:
            if ring_phase0 is None:
                ring_phase0 = phi0
            vals.append(a * math.cos(2.0 * math.pi * FIT["f220_hz"] * t + ring_phase0))
    return vals


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def norm(a):
    return math.sqrt(max(1e-30, dot(a, a)))


def best_fit(exp_t, exp_y):
    mu = sum(exp_y) / len(exp_y)
    ey = [v - mu for v in exp_y]
    best = None
    for dt_shift in [x * 0.25e-3 for x in range(-40, 41)]:
        for k in range(181):
            phi0 = 2.0 * math.pi * k / 180.0
            m = model_with_shift_and_phase(exp_t, dt_shift, phi0)
            denom = dot(m, m)
            if denom <= 1e-30:
                continue
            A = dot(ey, m) / denom
            fit = [A * v for v in m]
            corr = dot(ey, fit) / (norm(ey) * norm(fit))
            if best is None or corr > best["corr"]:
                best = {"dt": dt_shift, "phi0": phi0, "A": A, "corr": corr, "fit": fit, "exp": ey}
    return best


def save_csv(ts, expy, fity, path: Path):
    with path.open("w", encoding="utf-8") as f:
        f.write("t_s,h_exp,h_fit,residual\n")
        for t, e, m in zip(ts, expy, fity):
            f.write(f"{t:.9f},{e:.12e},{m:.12e},{(e-m):.12e}\n")


def polyline(points):
    return " ".join(f"{x:.2f},{y:.2f}" for x, y in points)


def build_svg(ts, expy, fity, path: Path, meta: dict):
    W, H = 1200, 820
    margin = 70
    pw = W - 2 * margin
    panels = [
        (120, 180, "Experimental GWOSC reconstruction"),
        (350, 180, "Our fitted reduced-model waveform"),
        (580, 180, "Overlay: experiment vs our fit"),
        (650, 120, "Residual: experiment - fit"),
    ]
    amin = min(min(expy), min(fity))
    amax = max(max(expy), max(fity))
    scale = max(abs(amin), abs(amax), 1e-9) * 1.15
    resid = [e - m for e, m in zip(expy, fity)]
    rscale = max(max(abs(v) for v in resid), 1e-9) * 1.15
    t0 = min(ts)
    t1 = max(ts)

    def map_points(vals, y0, h, s):
        pts = []
        for t, v in zip(ts, vals):
            x = margin + (t - t0) / (t1 - t0) * pw
            y = y0 + h / 2 - (v / s) * (0.44 * h)
            pts.append((x, y))
        return pts

    exp1 = map_points(expy, 120, 180, scale)
    fit2 = map_points(fity, 350, 180, scale)
    exp3 = map_points(expy, 580, 180, scale)
    fit3 = map_points(fity, 580, 180, scale)
    res4 = map_points(resid, 650, 120, rscale)

    svg = []
    svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">')
    svg.append('<rect width="100%" height="100%" fill="#0b1020"/>')
    svg.append('<text x="70" y="40" fill="#f4f7ff" font-size="30" font-family="Arial" font-weight="700">GW150914: experimental reconstruction vs our fitted waveform</text>')
    svg.append(f'<text x="70" y="68" fill="#b8c2e0" font-size="16" font-family="Arial">Fit source: {FIT["label"]}. Correlation after nuisance alignment = {meta["corr"]:.4f}. Only global time shift, phase shift, and amplitude are refit here.</text>')
    svg.append('<text x="70" y="92" fill="#96a3c5" font-size="14" font-family="Arial">Public data: GWOSC H1/L1 cWB reconstructions. Model: current reduced PN/chirp plus Kerr ringdown waveform built from our fit parameters.</text>')
    for y0, h, title in panels:
        svg.append(f'<rect x="{margin}" y="{y0}" width="{pw}" height="{h}" rx="12" fill="none" stroke="#33415f" stroke-width="1.5"/>')
        svg.append(f'<text x="{margin + 16}" y="{y0 + 24}" fill="#ffffff" font-size="22" font-family="Arial" font-weight="700">{title}</text>')
        for i in range(1, 6):
            x = margin + pw * i / 6.0
            svg.append(f'<line x1="{x:.2f}" y1="{y0}" x2="{x:.2f}" y2="{y0 + h}" stroke="#25314b" stroke-width="1"/>')
        for i in range(1, 4):
            yy = y0 + h * i / 4.0
            svg.append(f'<line x1="{margin}" y1="{yy:.2f}" x2="{margin + pw}" y2="{yy:.2f}" stroke="#25314b" stroke-width="1"/>')
        yy = y0 + h / 2.0
        svg.append(f'<line x1="{margin}" y1="{yy:.2f}" x2="{margin + pw}" y2="{yy:.2f}" stroke="#49597f" stroke-width="1.2"/>')
    svg.append(f'<polyline points="{polyline(exp1)}" fill="none" stroke="#79d4ff" stroke-width="2.5" stroke-linejoin="round" stroke-linecap="round"/>')
    svg.append(f'<polyline points="{polyline(fit2)}" fill="none" stroke="#ffd166" stroke-width="2.5" stroke-linejoin="round" stroke-linecap="round"/>')
    svg.append(f'<polyline points="{polyline(exp3)}" fill="none" stroke="#79d4ff" stroke-width="2.3" stroke-linejoin="round" stroke-linecap="round"/>')
    svg.append(f'<polyline points="{polyline(fit3)}" fill="none" stroke="#ffd166" stroke-width="2.3" stroke-linejoin="round" stroke-linecap="round" opacity="0.92"/>')
    svg.append(f'<polyline points="{polyline(res4)}" fill="none" stroke="#ff8ea3" stroke-width="2.3" stroke-linejoin="round" stroke-linecap="round"/>')
    svg.append('<text x="900" y="144" fill="#79d4ff" font-size="15" font-family="Arial">GWOSC coherent reconstruction</text>')
    svg.append('<text x="900" y="374" fill="#ffd166" font-size="15" font-family="Arial">our reduced-model fitted waveform</text>')
    svg.append('<text x="860" y="604" fill="#79d4ff" font-size="15" font-family="Arial">experiment</text>')
    svg.append('<text x="980" y="604" fill="#ffd166" font-size="15" font-family="Arial">our fit</text>')
    svg.append('<text x="960" y="674" fill="#ff8ea3" font-size="15" font-family="Arial">residual</text>')
    svg.append('</svg>')
    path.write_text("\n".join(svg), encoding="utf-8")


def build_html(svg_name: str, meta: dict, path: Path):
    html = f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"/><title>GW150914 experimental vs our fitted waveform</title>
<style>body{{font-family:Arial,Helvetica,sans-serif;background:#0b1020;color:#eef2ff;margin:0}} .wrap{{max-width:1200px;margin:24px auto;padding:0 18px}} a{{color:#8fd3ff}}</style></head>
<body><div class="wrap"><h1>GW150914 experimental reconstruction vs our fitted waveform</h1>
<p>This file was generated from the public GWOSC H1/L1 cWB reconstructions and the current accepted BKQR inverse fit parameters ({FIT['label']}). Only a global amplitude, time shift, and phase shift are adjusted during the overlay step.</p>
<ul>
<li>Correlation after nuisance alignment: <b>{meta['corr']:.4f}</b></li>
<li>Time shift: <b>{meta['dt'] * 1000:.3f} ms</b></li>
<li>Phase shift: <b>{meta['phi0']:.3f} rad</b></li>
<li>Amplitude scale: <b>{meta['A']:.6g}</b></li>
</ul>
<p><img src="{svg_name}" alt="GW150914 experimental vs fit waveform" style="max-width:100%;height:auto;border:1px solid #2b3654;border-radius:12px;background:#11182c"/></p>
<p>Generated outputs in the same folder: CSV, SVG, HTML, and a metadata text file.</p>
</div></body></html>"""
    path.write_text(html, encoding="utf-8")


def main():
    h1_path = ROOT / "H1_reconstructions.txt"
    l1_path = ROOT / "L1_reconstructions.txt"
    download(H1_URL, h1_path)
    download(L1_URL, l1_path)
    h1_t, h1_y = parse_reconstruction(h1_path)
    l1_t, l1_y = parse_reconstruction(l1_path)
    ts, expy = make_coherent(h1_t, h1_y, l1_t, l1_y)
    best = best_fit(ts, expy)
    csv_path = OUT / "gw150914_experimental_vs_our_fit.csv"
    svg_path = OUT / "gw150914_experimental_vs_our_fit.svg"
    html_path = OUT / "gw150914_experimental_vs_our_fit.html"
    meta_path = OUT / "gw150914_experimental_vs_our_fit_meta.txt"
    save_csv(ts, best["exp"], best["fit"], csv_path)
    build_svg(ts, best["exp"], best["fit"], svg_path, best)
    build_html(svg_path.name, best, html_path)
    meta = [
        "GW150914_EXPERIMENTAL_VS_OUR_FIT",
        f"FIT_LABEL={FIT['label']}",
        f"CORRELATION={best['corr']:.8f}",
        f"TIME_SHIFT_S={best['dt']:.9f}",
        f"PHASE_SHIFT_RAD={best['phi0']:.9f}",
        f"AMPLITUDE_SCALE={best['A']:.12e}",
        f"MTOTAL_MSUN={FIT['M_total_msun']}",
        f"ETA={FIT['eta']}",
        f"CHIRP_MSUN={FIT['M_chirp_msun']}",
        f"F220_HZ={FIT['f220_hz']}",
        f"TAU_S={FIT['tau_s']}",
        f"CSV={csv_path.name}",
        f"SVG={svg_path.name}",
        f"HTML={html_path.name}",
    ]
    meta_path.write_text("\n".join(meta) + "\n", encoding="utf-8")
    print(f"CSV={csv_path}")
    print(f"SVG={svg_path}")
    print(f"HTML={html_path}")
    print(f"META={meta_path}")


if __name__ == "__main__":
    main()
