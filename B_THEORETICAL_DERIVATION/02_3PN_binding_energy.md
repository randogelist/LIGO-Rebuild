# 02 — 3PN circular binding energy

The circular 3PN binding energy implemented in the engine is

\[
E(x)=-\frac{M\eta x}{2}
\left(1+e_1x+e_2x^2+e_3x^3\right),
\]

with

\[
e_1=-\frac34-\frac{\eta}{12},
\]

\[
e_2=-\frac{27}{8}+\frac{19}{8}\eta-\frac{\eta^2}{24},
\]

\[
e_3=-\frac{675}{64}
+\left(\frac{34445}{576}-\frac{205\pi^2}{96}\right)\eta
-\frac{155}{96}\eta^2
-\frac{35}{5184}\eta^3.
\]

Hence

\[
\frac{dE}{dx}
=-\frac{M\eta}{2}
\left(1+2e_1x+3e_2x^2+4e_3x^3\right).
\]

The associated circular orbital angular momentum is reconstructed from \(dE=\Omega\,dJ\):

\[
J_{\rm orb}(x)
=\eta M^2x^{-1/2}
\left(1-2e_1x-e_2x^2-\frac45e_3x^3\right).
\]

The input \(J_{\rm tot}\) is inverted numerically on the weak-field circular branch to determine the initial \(x_0\), hence \(r_0=M/x_0\).
