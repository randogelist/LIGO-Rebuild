# 04 — Strong-field handoff

A truncated PN expansion should not be evolved arbitrarily deep into the strong-field regime.

The engine determines the 3PN minimum-energy circular orbit by solving

\[
\frac{dE}{dx}(x_{\rm MECO})=0.
\]

The nominal transition variable is

\[
x_{\rm tr}=0.92\,x_{\rm MECO},
\]

subject also to the user-selected safety radius.

At the transition, the code stores

\[
t_{\rm tr},\quad x_{\rm tr},\quad r_{\rm tr},\quad
\phi_{\rm tr},\quad \Omega_{\rm tr},\quad
E_{\rm rad,tr},\quad J_{\rm rad,tr}.
\]

The post-transition trajectory is then a smooth calibrated continuation. Separation is collapsed toward a single remnant field while the energy and angular-momentum ledgers asymptote to the fitted remnant values.

This handoff is a **model closure**. It is not a numerical-relativity evolution of the spacetime metric.
