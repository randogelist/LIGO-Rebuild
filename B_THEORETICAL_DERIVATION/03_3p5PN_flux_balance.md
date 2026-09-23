# 03 — 3.5PN gravitational-wave flux and inspiral

The engine uses the nonspinning circular gravitational-wave luminosity through relative 3.5PN order,

\[
\mathcal F(x)=\frac{32}{5}\eta^2x^5\,C_{3.5\mathrm{PN}}(x,\eta),
\]

where `renderer/binary_driver.h` contains the full coefficient expansion through \(x^{7/2}\), including the tail and logarithmic terms.

The inspiral follows the balance law

\[
\frac{dE}{dt}=-\mathcal F.
\]

Since \(E=E(x)\),

\[
\boxed{
\frac{dx}{dt}
=-\frac{\mathcal F(x)}{dE/dx}
}
\]

and the orbital phase evolves as

\[
\boxed{
\frac{d\phi}{dt}
=\operatorname{sgn}(L_{\rm orb})\frac{x^{3/2}}{M}.
}
\]

The radiated angular-momentum flux is obtained from circular balance,

\[
\frac{dJ_{\rm rad}}{dt}
=\operatorname{sgn}(L_{\rm orb})
\frac{\mathcal F}{|\Omega|}.
\]

The implementation integrates these equations with adaptive RK4 and keeps explicit energy and angular-momentum ledgers. The regression tests check that the PN balance residuals remain numerically small.
