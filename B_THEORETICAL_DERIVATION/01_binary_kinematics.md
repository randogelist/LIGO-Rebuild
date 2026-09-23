# 01 — Binary kinematics

Let

\[
M=m_1+m_2,\qquad
\mu=\frac{m_1m_2}{M},\qquad
\eta=\frac{\mu}{M}=\frac{m_1m_2}{M^2}.
\]

The aligned-spin orbital angular momentum used by the code is

\[
L_{\rm orb}=J_{\rm tot}-S_{1z}-S_{2z}.
\]

Its sign fixes the orientation of the circular orbit. The gauge-invariant PN variable is

\[
x=(M|\Omega|)^{2/3}.
\]

For a circular orbit the renderer uses

\[
r=\frac{M}{x},
\qquad
\Omega=\operatorname{sgn}(L_{\rm orb})\frac{x^{3/2}}{M}.
\]

The center-of-mass positions are

\[
\mathbf x_1=\frac{m_2}{M}\,r\,\mathbf e_r,
\qquad
\mathbf x_2=-\frac{m_1}{M}\,r\,\mathbf e_r,
\]

with corresponding velocities obtained from the radial and azimuthal components of the relative motion.

The default GW150914 visualizer preset normalizes the GWOSC central source-frame mass ratio \(34.6:30.0\) so that \(M=1\). This means the renderer preserves the mass ratio and symmetric mass ratio while expressing lengths and times in total-mass units.
