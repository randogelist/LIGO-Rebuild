# A — Visualizer and numerical engine

This folder contains the runnable Windows-native reduced binary-black-hole model and DX12 optical renderer.

## Run the GW150914 preset

From a native PowerShell / CMD environment with MSVC Build Tools available:

```powershell
.\run_gw150914.cmd
```

The runner executes the numerical/static test suite, compiles the renderer, and opens the interactive window.

The GW150914 preset uses the current GWOSC/GWTC-2.1 central source-frame mass ratio,

\[
34.6:30.0,
\]

normalized internally to

\[
m_1=0.5356037152,\qquad m_2=0.4643962848,\qquad M=1.
\]

The initial orbit is set to \(r_0=12M\) through the matching 3PN circular angular momentum.

## Main numerical path

`renderer/binary_driver.h` contains the reduced dynamics:

- 3PN circular binding energy and angular momentum,
- 3.5PN GW flux,
- balance-law inspiral,
- strong-field handoff,
- NR-calibrated remnant mass/spin fit,
- Kerr \((2,2,0)\) ringdown frequency/damping scale.

`renderer/main.cpp` adds:

- DX12 optical/lensing visualization,
- point-source detector mode,
- cylindrical beam mode,
- induced far-observer waveform panel showing \(h_+\), \(h_\times\), and projected \(h_{\rm obs}\).

## Claim boundary

This is a **PN/NR-calibrated reduced model**. It is not a BSSN/Z4c numerical-relativity evolution and the waveform-panel amplitude is a visualization-scale far-field model, not a calibrated reproduction of the strain at Earth.
