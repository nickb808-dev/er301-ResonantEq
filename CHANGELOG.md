# Changelog — Resonant EQ (`reseq`)

All notable feature changes. This file covers **0.5.2 → 0.6.1**.

---

## 0.6.1 — Drift (analog component tolerance)

### Added
- **Drift** control `[0, 1]` on **all three units** (Resonant EQ, Resonant EQ MO,
  Resonant EQ AN). Each of the ten bands carries a fixed characteristic offset in
  its centre frequency (±~3%) and Q (±~25%); the Drift knob scales how much is
  applied.
  - **0** = mathematically perfect — bit-identical to no Drift (the default, so
    existing patches are unchanged).
  - **1** = "vintage-loose" — every band pulls off its nominal Serge tuning by its
    own amount, so the bank feels organic instead of clinical.
  - The offsets are fixed (a specific unit's "component values"), not randomised,
    and add **no harmonics** — it's pure frequency/Q variety, not distortion.

### Notes
- Drift is CV-able and lives in the shared DSP core, so it behaves identically
  across the stereo, multi-out, and analyzer units.

---

## 0.6.0 — Resonant EQ AN (analyzer, per-band envelope followers)

### Added
- **New third unit: Resonant EQ AN** (mono). The same ten resonant Serge bands,
  plus an **envelope follower on every band** — a live 10-band spectral-CV bank
  (a resonant EQ and a vocoder / spectral-analysis front-end in one).
  - **11 multi-output sub-outs**: `Main` (audio) + `Env1…Env10`. On stolmine
    multi-out firmware all eleven are individually pickable; on vanilla firmware
    it degrades to `Main` + `Env1`. (This is the largest multi-out in the suite.)
  - **Regen** — a single feedback knob that folds `Main` back into the input.
    Because `Main` is the gain-weighted sum of the bands, feeding it back
    regenerates the *boosted* bands more than the flat/cut ones, so the
    regeneration follows your EQ curve from one control; pushed up it self-
    oscillates at the emphasised frequencies (bounded/soft-limited).
  - **Env Rise** / **Env Fall** (follower attack/release) and **Env Gain**
    (CV-bank output scale). Each band's smoothing speed is floored by its own
    frequency, so the 29 Hz band's envelope stays smooth while the highs still
    track fast.
- Package manifest now lists an **author** (`nickb808`).

### Notes
- Reuses the shared `reseq_dsp.h` band core; the envelope-follower coefficients
  use a polynomial map (no `expf`), keeping the am335x-safe "no runtime libm"
  property intact.

---

## 0.5.2 — baseline for this changelog

Sub-display copy cleanup: each band parameter shows just its frequency in Hz, and
the band-value scope's sub-screen carries a Daisaku Ikeda epigraph. (Included here
only as the starting point; see earlier tags for its own changes.)
