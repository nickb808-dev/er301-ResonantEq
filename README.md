# Resonant EQ

A ten-band fixed-frequency resonant filter bank, inspired by
the **Serge Resonant Equalizer**. Each band is an independently CV-controllable
resonant bandpass at a classic Serge centre frequency; sweep a band from a deep
cancellation notch, through flat, up to a boosted resonant peak that pings and
self-oscillates.

The package ships **three units** that share one DSP core:

- **Resonant EQ** — stereo, single all-bands output. The clean spectral shaper.
- **Resonant EQ MO** — mono, multi-output (Main + two combs) with per-comb
  feedback for regeneration and self-oscillation.
- **Resonant EQ AN** — mono analyzer: Main + an envelope follower on every band,
  a live 10-band spectral-CV bank (a resonant EQ and a vocoder analysis front-end
  in one).

Every unit also has **Drift** (analog component tolerance) and a live band-value
phosphor scope.

Package name on the ER-301: **Resonant EQ** (`reseq`), version 0.6.1.

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

### Resonant EQ AN (mono analyzer, multi-out)

The same ten resonant bands, plus an **envelope follower on each band** — a live
10-band spectral-CV bank:

```
In1 → In → [10 resonant bandpass, per-band CV]
              ├─ Σ all bands ────────────────────→ Main       → Out1  (audio)
              └─ envelope follower per band ──────→ Env1…Env10 → Out2…Out11
```

**11 sub-outs** (`channelCount = 11`): on stolmine firmware all are individually
pickable; vanilla degrades to `Main` + `Env1`. It's a Serge resonant EQ and a
10-band vocoder/spectral analysis front-end at once — patch the envelopes to
drive anything.

**Regen** is a single feedback knob that folds `Main` back into the input. Because
`Main` is the gain-weighted sum of the bands, feeding it back regenerates the
*boosted* bands more than the flat/cut ones — the regeneration follows your EQ
curve from one control, and pushes into bounded self-oscillation at the top.

The follower has **Env Rise** / **Env Fall** (attack/release) and **Env Gain**
(CV-bank scale). Each band's smoothing speed is floored by its own frequency, so
the 29 Hz band's envelope stays smooth while the highs still track fast.

Controls: ten band CVs, **Regen**, **Env Rise**, **Env Fall**, **Env Gain**,
**Drift**, **Level**.

#### Accessing the per-band envelope outputs

The ten followers are exposed as **sub-outputs** you pick from the **Local source
chooser** — the same screen you use to patch any in-chain signal into a
CV/modulation input. On [stolmine](https://github.com/stolmine/er-301-stolmine)
firmware:

1. Go to the parameter you want the envelope to drive — a modulation input on
   another unit (filter cutoff, VCA level, oscillator pitch, another EQ band…) —
   and open its source assignment, then switch to the **Local** sources tab.
2. **Focus the Resonant EQ AN unit** in that list. Because it's multi-out, an
   indicator appears showing `X/Y` and the sub-out label (e.g. `3/11  env2`).
3. Press **M6** (the sixth main button, under the scope) to **cycle** the
   sub-outs: `main → env1 → env2 → … → env10 → main`. The scope auditions the
   focused one, so you can watch the envelope move before committing.
4. **Enter** selects the shown sub-out as your source.

Sub-out map: `main` (Out1) = audio; `env1` = 29 Hz band envelope, `env2` = 61 Hz,
… `env10` = 11 kHz. The envelopes are unipolar CV (scaled by **Env Gain**) meant
for modulation — spectral-following: low-band envelope → a sub's VCA, presence
band → a reverb send, or the whole bank across a set of voices for a crude
vocoder.

> **Vanilla firmware:** `subOutLabels` is ignored and only sub-out 1 (**Main**,
> Out1) and sub-out 2 (**Env1**, Out2) surface as the stereo pair — the other
> nine envelopes aren't reachable. The M6 sub-out picker is a stolmine feature.

---

## Drift — analog component tolerance

Every unit has a **Drift** knob `[0, 1]`. Each band carries a fixed characteristic
offset in its centre frequency (±~3%) and Q (±~25%), and Drift scales how much is
applied: **0** = mathematically perfect (default, bit-identical to no Drift), **1**
= "vintage-loose", where every band pulls off its nominal tuning by its own
amount so the bank feels organic instead of clinical. The offsets are fixed (a
specific unit's "component values"), not randomised, and add **no harmonics** —
it's pure frequency/Q variety, not distortion.

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
    reseq_dsp.h        shared core: band table, kBandG trig-const table, Drift
                       tolerance tables, sanitize/softLimit, computeBand
    ResonantEQ.h/.cpp    stereo unit — all-bands Main, no feedback
    ResonantEQMO.h/.cpp  mono multi-out unit — Main/Comb1/Comb2 + per-comb feedback
    ResonantEQAN.h/.cpp  mono analyzer — Main + 10 env followers + Regen
    BandsGraphic.h       phosphor band-value scope (followable from a unit)
    libreseq.swig
    compat.cpp / compat_swig.h
  assets/
    toc.lua              package manifest
    ResonantEQ.lua       stereo unit wrapper
    ResonantEQMO.lua     multi-out unit wrapper (bands + fb1/fb2 + drift + level)
    ResonantEQAN.lua     analyzer wrapper (bands + regen + env + drift, 11 outs)
    BandsView.lua        band-value scope ViewControl
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
make pkg                                # → build/am335x/reseq-0.6.1.pkg
```

Copy **only** the new `reseq-0.6.1.pkg` to the card's `ER-301/packages/` folder
(remove any older `reseq-*.pkg` so they don't shadow it), then **reboot the
ER-301**. A package's compiled `.so` does not hot-swap — the old native library
stays resident until reboot, so skipping the reboot can leave you with updated
Lua controls that don't actually do anything.

### Host tests

The DSP is verified off-device:

```bash
g++ -std=c++17 -O2 -ffast-math -Dprivate=public -Itest/host -Isrc \
    src/ResonantEQ.cpp src/ResonantEQMO.cpp src/ResonantEQAN.cpp test/host/main.cpp -o t
./t sflat      # stereo flat response (~2.5 dB ripple)
./t sband 5    # a band boosts/cuts as expected
./t comb       # combs split odd/even; Main == Comb1 + Comb2
./t fb         # feedback lifts the peak; full feedback self-oscillates (bounded)
./t fbsplit    # each comb's feedback favours its own comb (independence)
./t drift      # Drift detunes a band's peak off nominal; bounded
./t antrack    # AN: a tone peaks the envelope of its matching band
./t anenv      # AN: env rise/fall; low-band envelope stays smooth
./t anstab     # AN: Regen self-oscillates bounded; all env outs finite
./t stab       # long noise run, max feedback → finite, bounded
./t nan        # NaN input → finite output
```

---

## Status

**v0.6.1** — host-verified. Three units (stereo / multi-out+feedback / analyzer),
per-comb feedback, the 10-band envelope-follower bank with Regen, and Drift are
all in place. The one thing that only proves out on hardware is how the analyzer's
**11 sub-outs** surface in the stolmine multi-out picker.

## Credits

Inspired by the Serge Modular Resonant Equalizer (Serge Tcherepnin; Eurorack
"RS-REQ" adaptation by Random*Source). DSP is an original implementation using
Andrew Simper's (Cytomic) state-variable filter. Built for the Orthogonal Devices
ER-301. "Serge" is a trademark of its owner; this is an independent tribute and
implies no affiliation. See `NOTICE.md` and `LICENSE`.
