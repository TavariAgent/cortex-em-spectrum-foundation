# Gaussian Field Processing: Core Formulas and Invariants

Goals
- Preserve the image domain (no “shrinkage” of the frame).
- Preserve DC/mean (no unintended brightness drift).
- Respect scale-space causality (no new extrema from smoothing).
- Make σ a true scale parameter (semi-group: σ^2 additive over repeated smoothing).

1) Core Gaussian definitions
- 1D Gaussian (μ=0, σ):
  gσ(x) = (1 / (sqrt(2π) σ)) · exp( − x^2 / (2 σ^2) )

- 2D isotropic:
  Gσ(x, y) = (1 / (2π σ^2)) · exp( − (x^2 + y^2) / (2 σ^2) )

- 2D anisotropic with covariance Σ (SPD, 2×2):
  GΣ(p) = (1 / (2π sqrt(|Σ|))) · exp( − 1/2 pᵀ Σ⁻¹ p ),  where p = [x, y]ᵀ

  Axis-aligned: Σ = diag(σx², σy²).
  Rotated by θ: Σ = Rθ diag(σx², σy²) Rθᵀ, Rθ = [[cosθ, −sinθ],[sinθ, cosθ]].

- FWHM relation: FWHM = 2 sqrt(2 ln 2) σ ≈ 2.355 σ.

2) Discrete convolution (same-size, domain-preserving)
- 1D discrete “same” convolution with radius r = ceil(3σ):
  y[i] = ( Σ_{k=−r..r} w[k] · x[i−k]_pad ) / ( Σ_{k=−r..r} w_eff[i,k] )

  where:
    - w[k] = exp( − k² / (2σ²) )  (unnormalized for simplicity),
    - x[i−k]_pad is x with boundary padding,
    - w_eff[i,k] = w[k] if (i−k) is inside the domain after padding; else 0.
    - The denominator renormalizes per-pixel so the effective weights sum to 1 near edges.

- 2D separable (isotropic/axis-aligned):
  Apply 1D horizontally then vertically with the same σ (different σx, σy if needed).

- Boundary padding choices (preserve frame):
    - Reflect-101 (mirror without duplicating the edge): best for avoiding edge darkening.
    - Replicate (clamp-to-edge).
    - Wrap (periodic), valid for frequency work.

3) Invariants you must enforce
- Unit mass (DC preservation):
  Σ kernel = 1 in continuous; in discrete, ensure per-pixel renormalization near edges so Σ weights used = 1. This preserves average brightness (L1 sum under periodic padding).

- Zero-mean kernel for derivatives:
  For derivatives (∂G/∂x, LoG), Σ kernel = 0 by design; don’t use these if you want DC preserved—use them for detection, not for direct smoothing of intensity.

- Semi-group (scale additivity):
  Gσ1 * Gσ2 = Gσ, where σ² = σ1² + σ2².
  Implication: If you apply Gaussian blur multiple times, track σ² and avoid re-blurring from scratch. This avoids “step-wise reduction” artifacts and keeps your “G-frame” consistent.

- Causality (scale-space):
  Smoothing with Gaussian does not create new local extrema. As σ increases, structures only disappear (no spurious sharpening). This is the theoretical guarantee you want to avoid “convolutional loss of image space.”

4) Elliptical Gaussian (anisotropic) in practice
- Given Σ and pixel offset p = [dx, dy]:
  w(p) = exp( − 1/2 pᵀ Σ⁻¹ p )
  Normalize per-pixel:
  y[i,j] = Σ w(p) x[i−dx, j−dy]_pad / Σ w(p)_eff

- Efficient axis-aligned case:
  Use separable 1D with σx horizontally and σy vertically (fast and exact for axis-aligned Σ).

- Rotated case (θ ≠ 0):
  Either:
    - Sample 2D kernel directly over a small support (radius ≈ ceil(3 · max(σx, σy))).
    - Or rotate coordinates: p' = Rθᵀ p, then use axis-aligned exponent with σx, σy.

5) Frequency-domain intuition (why Gaussian is “safe”)
- Fourier transform remains Gaussian:  F{Gσ}(ω) ∝ exp( − σ² ||ω||² / 2 ).
- It’s a pure low-pass with no ringing. DC (ω=0) gain is 1 if kernel mass=1.
- Parseval says L2 energy decreases with smoothing (expected), but mean (DC) is preserved.

6) 1D mean (box) filter as a baseline
- Moving average of width M (odd):
  y[i] = (1/M) Σ_{k=−(M−1)/2..(M−1)/2} x[i−k]_pad
- Use integral images for O(1) per sample. Mean-preserving by construction if you renormalize at borders (or use reflect padding).

7) Spectral density (for analysis/operations)
- Power spectral density (periodogram):
  S(ωx, ωy) = | F{I}(ωx, ωy) |²
- For anisotropy estimation, smooth S with a Gaussian in frequency domain; this preserves total power near-DC and suppresses noise.

8) Gaussian beam optics overlay (intensity profile)
- Rayleigh range: zR = π w0² / λ
- Beam radius: w(z) = w0 √(1 + (z / zR)²)
- Intensity at (r, z):
  I(r, z) = I0 (w0 / w(z))² exp( − 2 r² / w(z)² )
- To conserve total radiant energy in the frame, normalize I so that Σ I_pixel matches a chosen target energy before blending.

9) Practical “no-loss-of-field” checklist
- Use “same-size” outputs with reflect-101 padding.
- Normalize kernels to unit mass; renormalize per-pixel near borders.
- Accumulate σ² (semi-group) instead of repeated ad-hoc blurs.
- Do all filtering in linear color space; apply gamma after reconstruction.
- For derivative operators (LoG/DoG), keep them in analysis paths (features), not as direct image replacement.