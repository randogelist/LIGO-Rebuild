# 07 — Induced far-observer wave signal

The waveform panel is generated from the **same evolving state** that drives the optical renderer.

For inclination \(\iota\), define

\[
c_\iota=\cos\iota,
\qquad
x=(M|\Omega|)^{2/3}.
\]

The visualization-scale leading amplitude is

\[
A(t)=\frac{4\eta x}{R}\,G,
\]

where \(R\) is the panel's observer-distance parameter in model units and \(G\) is an explicit display gain.

The plotted polarizations are

\[
h_+(t)
=A(t)\frac{1+c_\iota^2}{2}\cos(2\phi),
\]

\[
h_\times(t)
=A(t)c_\iota\sin(2\phi).
\]

A simple projected observer channel is

\[
h_{\rm obs}=F_+h_+ + F_\times h_\times,
\]

with

\[
F_+=\cos(2\psi),
\qquad
F_\times=\sin(2\psi),
\]

where \(\psi\) is the live polarization control.

After the inspiral transition, the amplitude is smoothly enhanced through merger and damped with the fitted Kerr ringdown timescale.

The panel deliberately removes the constant propagation delay, so its horizontal axis is a retarded-time view aligned with the simulation.

## Important limitation

The panel's amplitude is **not calibrated to physical strain at Earth**. The distance parameter and gain are visualization controls, and the antenna projection is simplified. The meaningful comparison at this stage is chiefly morphology and characteristic time/frequency scale, not absolute strain amplitude.
