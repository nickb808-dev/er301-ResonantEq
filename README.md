# Resonant EQ

A ten-band fixed-frequency resonant filter bank for the
[ER-301 Sound Computer](https://www.orthogonaldevices.com/er-301), inspired by
the **Serge Resonant Equalizer**. Each band is an independently CV-controllable
resonant bandpass at a classic Serge centre frequency; sweep a band from a deep
cancellation notch, through flat, up to a boosted resonant peak that pings and
self-oscillates.

The package ships **two units** that share one DSP core:

- **Resonant EQ** — stereo, single all-bands output. The clean spectral shaper.
- **Resonant EQ MO** — mono, multi-output (Main + two combs) with per-comb
  feedback for regeneration and self-oscillation.

Package name on the ER-301: **Resonant EQ** (`reseq`), version 0.4.0.

---

## Bands

Ten resonant bandpass filters at the Serge centre frequencies:

```
29 · 61 · 115 · 218 · 411 · 777 · 1500 · 2800 · 5200 · 11000 Hz
```

Each band has one CV-able control over the range `[-1, 1]`:

- **−1** → deep cancellation notch (band removed)
- **0** → flat (bands sum to a near-unity, ~2.5 dB-ripple response)
- **0 … +0.5** → progressive boost (up to ~+12 dB)
- **+0.5 … +1** → the boost is held while damping climbs, so the band tightens
  into a resonant peak that rings and, at the top, self-oscillates.

Because the bands are broad and overlap, the flat setting is genuinely flat —
you shape from there, cutting and resonating individual regions.

---

## The two units

### Resonant EQ (stereo)

`In1`/`In2 → OutL`/`OutR`, all ten bands summed to a single **Main** output. No
combs, no feedback — just the clean stereo EQ. Controls: the ten band CVs plus
**Level** (output gain, unity = 1).

### Resonant EQ MO (mono, multi-out + feedback)

Exposes the Serge hardware's separate comb outputs as multi-output sub-outs:

```
In1 → In → [10 resonant bandpass, per-band CV]
              ├─ Σ all bands ──────────────────────→ Main   → Out1
              ├─ Σ odd bands  (29/115/411/1.5k/5.2k)→ Comb 1 → Out2
              └─ Σ even bands (61/218/777/2.8k/11k) → Comb 2 → Out3
```

On the [stolmine](https://github.com/stolmine/er-301-stolmine) multi-out firmware
the three outs are individually pickable sub-outs; on vanilla firmware they
degrade gracefully to `Main` + `Comb 1` as a stereo pair.

**Per-comb feedback (Comb1FB / Comb2FB).** Two independent CV-able amounts fold
each comb back into the filter input, one sample delayed:

```
x = in + Comb1FB · softLimit(Comb1_prev) + Comb2FB · softLimit(Comb2_prev)
```

Because the two combs span disjoint band sets, you can regenerate — or drive to
self-oscillation — one half of the spectrum while the other stays clean. Rising
feedback reinforces that comb's resonances and, near/above unity loop gain,
self-oscillates at its band (comb-tooth) frequencies. Each fed-back path is
soft-limited so the loop stays bounded (a musical howl, not a blow-up). Both
amounts at 0 reproduces the plain multi-output EQ exactly; equal amounts behave
like a single global feedback.

Controls: ten band CVs, **fb1** (Comb 1 feedback), **fb2** (Comb 2 feedback),
and **Level**.

---

## How it works (DSP)

Ten parallel [Cytomic](https://cytomic.com/technical-papers/) state-variable
filters (Andrew Simper's topology), one per band, summed with a per-band
CV → gain/Q voicing map. The audio path is pure add/multiply/one-divide.

### am335x trig-safe

The ER-301 runs a TI am335x (Cortex-A8), where runtime `sinf`/`cosf` called from
a package `.so` miscompute (a package→firmware libm call-boundary bug). The band
frequencies are fixed, so the only trig — `g = tan(π·f/fs)` for the ten centres —
is a compile-time `constexpr` table (`kBandG`). No libm trig runs at runtime;
`nm` confirms it. Both input and output are NaN/Inf sanitised, and outputs (and
each feedback path) are soft-limited so resonant and self-oscillating signals
stay bounded.

---

## Layout

```
res-e/  (package name: reseq)
  Makefile / Dockerfile
  LICENSE / NOTICE.md
  src/
    reseq_dsp.h        shared core: band table, kBandG trig-const table,
                       sanitize/softLimit, computeBand (CV → coeffs)
    ResonantEQ.h/.cpp    stereo unit — all-bands Main, no feedback
    ResonantEQMO.h/.cpp  mono multi-out unit — Main/Comb1/Comb2 + per-comb feedback
    libreseq.swig
    compat.cpp / compat_swig.h
  assets/
    toc.lua              package manifest
    ResonantEQ.lua       stereo unit wrapper
    ResonantEQMO.lua     multi-out unit wrapper (bands + fb1/fb2 + level)
  test/host/             host simulation harness + od stubs
```

---

## Build

Requires the ER-301 SDK and Docker (cross-compiles to am335x). From the `res-e/`
folder:

```bash
make docker-image                       # first time only (shared image)
make swig-docker  ER301_SDK=~/er-301    # generate the SWIG Lua wrapper
make docker-build ER301_SDK=~/er-301    # cross-compile the .so
make pkg                                # → build/am335x/reseq-0.4.0.pkg
```

Copy **only** the new `reseq-0.4.0.pkg` to the card's `ER-301/packages/` folder
(remove any older `reseq-*.pkg` so they don't shadow it), then **reboot the
ER-301**. A package's compiled `.so` does not hot-swap — the old native library
stays resident until reboot, so skipping the reboot can leave you with updated
Lua controls that don't actually do anything.

### Host tests

The DSP is verified off-device:

```bash
g++ -std=c++17 -O2 -ffast-math -Dprivate=public -Itest/host -Isrc \
    src/ResonantEQ.cpp src/ResonantEQMO.cpp test/host/main.cpp -o t
./t sflat      # stereo flat response (~2.5 dB ripple)
./t sband 5    # a band boosts/cuts as expected
./t sstereo    # stereo L/R bit-identical
./t moflat     # multi-out Main flat
./t comb       # combs split odd/even; Main == Comb1 + Comb2
./t fb         # feedback lifts the peak; full feedback self-oscillates (bounded)
./t fbsplit    # each comb's feedback favours its own comb (independence)
./t stab       # long noise run, max feedback → finite, bounded
./t nan        # NaN input → finite output
```

---

## Status

**v0.4.0** — host-verified, running on hardware. The stereo and multi-out units,
combs, and independent per-comb feedback are all in place.

## Credits

Inspired by the Serge Modular Resonant Equalizer (Serge Tcherepnin; Eurorack
"RS-REQ" adaptation by Random*Source). DSP is an original implementation using
Andrew Simper's (Cytomic) state-variable filter. Built for the Orthogonal Devices
ER-301. "Serge" is a trademark of its owner; this is an independent tribute and
implies no affiliation. See `NOTICE.md` and `LICENSE`.
