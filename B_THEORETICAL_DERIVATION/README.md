# B — Theoretical derivation

This folder documents the exact reduced-model chain implemented in `A_VISUALIZER_NUMERICAL_ENGINE/renderer/binary_driver.h` and the induced-wave panel in `renderer/main.cpp`.

The layers are separated so the implemented assumptions are visible:

1. binary kinematics and normalized variables,
2. 3PN circular binding energy and angular momentum,
3. 3.5PN flux and balance-law inspiral,
4. strong-field handoff,
5. NR-calibrated remnant and Kerr ringdown,
6. optical approximation,
7. induced far-observer waveform,
8. comparison with GW150914.

The key distinction is between **derived dynamics** and **model closure**. The PN inspiral is integrated from explicit balance equations. The merger/remnant portion is a calibrated closure, not a direct numerical solution of Einstein's field equations.
