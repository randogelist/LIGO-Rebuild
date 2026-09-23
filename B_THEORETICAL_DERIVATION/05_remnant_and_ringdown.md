# 05 — Remnant mass, spin, and Kerr ringdown

The engine maps the binary mass ratio and aligned component spins to an NR-calibrated radiated-energy fraction and final dimensionless spin.

Schematically,

\[
(m_1,m_2,\chi_1,\chi_2)
\longrightarrow
\epsilon_{\rm rad},a_f,
\]

then

\[
M_f=M(1-\epsilon_{\rm rad}).
\]

The exact rational/polynomial coefficients used by the current implementation are in `renderer/binary_driver.h` in

- `uib_radiated_fraction`, and
- `uib_final_spin`.

For the dominant Kerr \((2,2,0)\) mode the code uses

\[
M_f\omega_{220}
=1.5251-1.1568(1-|a_f|)^{0.1292},
\]

and

\[
Q_{220}=0.7000+1.4187(1-|a_f|)^{-0.4990}.
\]

Therefore

\[
\omega_{220}
=\frac{1.5251-1.1568(1-|a_f|)^{0.1292}}{M_f},
\]

\[
\tau_{220}=\frac{2Q_{220}}{\omega_{220}}.
\]

These are the scales used by the merger/ringdown continuation and by the diagnostic comparisons with GW150914.
