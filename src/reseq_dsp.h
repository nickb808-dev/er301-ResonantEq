/* reseq_dsp.h — shared DSP core for the Resonant EQ package (Serge clone).
 *
 * Used by both units:
 *   ResonantEQ    — stereo, single all-bands "Main" output (no combs/feedback)
 *   ResonantEQMO  — mono, multi-output (Main/Comb 1/Comb 2) + combs→input feedback
 *
 * Ten fixed-frequency resonant bandpass filters (Cytomic SVF).  Band centres are
 * the classic Serge set; g = tan(π·f/fs) is a PRECOMPUTED constexpr table
 * (kBandG) — NO libm trig at runtime (am335x package-trig bug: sinf/cosf from a
 * package .so miscompute; see the paulstretch trig note / habitat
 * feedback_package_trig_lut).  The audio path is pure add/mul/one-divide.
 */

#pragma once

#include <cstdint>
#include <cmath>     // sqrtf (HW instruction) for the resonance shaper

namespace reseq {

// ── Constants ────────────────────────────────────────────────────────────────

static constexpr int   kNumBands  = 10;
static constexpr int   kSampleRate = 48000;

// Classic Serge Resonant Equalizer band centres (Hz).
static constexpr float kBandFreqs[kNumBands] = {
    29.0f, 61.0f, 115.0f, 218.0f, 411.0f, 777.0f, 1500.0f, 2800.0f, 5200.0f, 11000.0f
};

// Precomputed g = tan(π·f/fs) at fs = 48 kHz (host-computed — no runtime trig).
static constexpr float kBandG[kNumBands] = {
    0.00189804784f, 0.00399246188f, 0.00752687454f, 0.0142690349f, 0.0269063773f,
    0.0508984162f, 0.0984914034f, 0.185339045f, 0.354118573f, 0.876976463f
};

// Comb membership: Comb 1 = bands 0,2,4,6,8 (29/115/411/1500/5200 Hz);
// Comb 2 = bands 1,3,5,7,9 (61/218/777/2800/11000 Hz).  (i & 1) selects.

// ── DRIFT (analog component tolerance) ───────────────────────────────────────
// Fixed per-band fractional offsets applied to each band's frequency (g) and Q,
// scaled by a Drift control [0,1].  Drift 0 = mathematically perfect (bit-
// identical to the untouched bank); Drift 1 = "vintage-loose": each band pulls
// slightly off its nominal centre and Q by its own characteristic amount, like
// resistor/capacitor tolerances, so the bank feels organic instead of clinical.
// These are FIXED (a specific unit's "component values"), not randomised at run.
static constexpr float kBandFreqDrift[kNumBands] = {   // ±~3 % centre pull
    +0.019f, -0.024f, +0.011f, +0.028f, -0.016f,
    -0.007f, +0.022f, -0.029f, +0.014f, -0.012f
};
static constexpr float kBandQDrift[kNumBands] = {      // ±~25 % Q tolerance
    -0.18f, +0.22f, +0.09f, -0.25f, +0.14f,
    -0.11f, +0.20f, -0.16f, +0.07f, +0.24f
};

// Voicing constants (host-tuned).  Overridable at compile time.
#ifndef RESEQ_G0
#define RESEQ_G0 0.49f            // centres the flat sum at ~0 dB
#endif
#ifndef RESEQ_QBASE
#define RESEQ_QBASE 0.85f         // broad bands overlap to ~flat (2.5 dB ripple)
#endif
#ifndef RESEQ_ALT_SIGN
#define RESEQ_ALT_SIGN 0          // straight sum measured flattest; kept as a lever
#endif
static constexpr float kG0    = RESEQ_G0;
static constexpr float kQBase = RESEQ_QBASE;
static constexpr float kQMax  = 60.0f;      // Q at the top of the resonant throw
static constexpr float kBoost = 6.0f;       // gain multiplier at CV = +0.5 (≈ +12 dB net)

// ── Shared helpers ────────────────────────────────────────────────────────────

// Finiteness guard (exponent bit-test, -ffast-math-proof).
static inline float sanitize(float x)
{
    union { float f; uint32_t u; } v;
    v.f = x;
    return ((v.u & 0x7F800000u) == 0x7F800000u) ? 0.0f : x;
}

// Output soft limiter — transparent < ±0.9, asymptote ±1.
static inline float softLimit(float x)
{
    const float T  = 0.9f;
    const float ax = (x >= 0.0f) ? x : -x;
    if (ax <= T) return x;
    const float e   = ax - T;
    const float lim = T + (1.0f - T) * (e / (e + (1.0f - T)));
    return (x >= 0.0f) ? lim : -lim;
}

// ── WEIGHT (nonlinear resonance) ─────────────────────────────────────────────
// Soft-saturate the band-pass state in the SVF resonance loop, blended by a
// Weight amount [0,1].  weight 0 = bypass (bit-identical).  As it rises, the
// resonance self-limits and thickens — the "analog weight" of a driven filter —
// instead of ringing as a pure sine.  The band-pass tap is at SIGNAL scale, so a
// fixed soft knee behaves sensibly across bands.  Rational (libm-free); the
// asymptote is finite so the loop stays bounded (energy-reducing → stable).
static inline float shapeRes(float x, float weight)
{
    if (weight <= 0.0f) return x;
    // Unity SLOPE at 0 (so low-level resonance still builds — no extra damping),
    // bending only as |x| grows past ~1: adds odd-harmonic content and a gentle
    // self-limit on the peaks (asymptote ±3).  sqrtf is a HW instruction.
    const float s = x / sqrtf(1.0f + (x * x) * (1.0f / 9.0f));   // A = 3
    return x + weight * (s - x);
}

// Per-band SVF coefficients + output tap, as a pure function of band index and
// its CV [-1,1] (0 = flat).  Shared by both units so voicing is identical.
struct BandCoeffs { float a1, a2, a3, coef; };

static inline BandCoeffs computeBand(int i, float cv, float drift = 0.0f)
{
    const float v = cv < -1.0f ? -1.0f : (cv > 1.0f ? 1.0f : cv);
    const float d = drift < 0.0f ? 0.0f : (drift > 1.0f ? 1.0f : drift);

    // Gain: deep cancellation notch → flat → boost, then held through resonance.
    float gain;
    if (v <= 0.0f)        gain = kG0 * (1.0f + 2.0f * v);      // -1 → -g0 (notch), 0 → g0
    else if (v <= 0.5f)   gain = kG0 * (1.0f + (kBoost - 1.0f) * (v * 2.0f));
    else                  gain = kG0 * kBoost;

    // Damping k = 1/Q: base until +0.5, then climb toward kQMax (resonant/ping).
    const float Q = (v <= 0.5f) ? kQBase
                                : kQBase + (v - 0.5f) * 2.0f * (kQMax - kQBase);
    // Drift pulls each band's Q and centre by its fixed tolerance (d = 0 → none).
    const float k  = (1.0f / Q) * (1.0f + d * kBandQDrift[i]);
    const float g  = kBandG[i]  * (1.0f + d * kBandFreqDrift[i]);   // tan(π f / fs) pulled
    const float a1 = 1.0f / (1.0f + g * (g + k));
    BandCoeffs c;
    c.a1 = a1;
    c.a2 = g * a1;
    c.a3 = g * c.a2;

    // Normalise by BASE damping so the resonant throw (k < kBase) grows the peak.
    static const float kBaseNorm = 1.0f / kQBase;
#if RESEQ_ALT_SIGN
    const float sign = (i & 1) ? -1.0f : 1.0f;
#else
    const float sign = 1.0f;
#endif
    c.coef = sign * gain * kBaseNorm;
    return c;
}

} // namespace reseq
