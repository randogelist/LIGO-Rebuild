# 06 — Optical model

The DX12 renderer is an optical visualization layered on top of the reduced binary state.

During the two-body phase it uses a time-dependent two-puncture scalar optical ansatz. Rays are integrated through that field and the binary state is evaluated at retarded optical travel time.

As the merger variable approaches unity, the two local optical objects are faded out and replaced by a single remnant optical field. The one-hole limit is constructed to recover the static Schwarzschild isotropic optical form outside the horizon.

Available visualization modes include:

- sky/background mode,
- optical-field diagnostic,
- capture-ID diagnostic,
- mirrored point-source detector,
- mirrored cylindrical-beam detector.

This optical layer is a renderer model. It should not be interpreted as a full Kerr ray-tracing solution of the dynamical binary spacetime.
