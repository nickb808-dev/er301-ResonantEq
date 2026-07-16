/* ResonantEQAN.h — Serge Resonant Equalizer clone (MONO ANALYZER, MULTI-OUT)
 *                  for ER-301 v0.6.0
 *
 * The analyzer sibling of ResonantEQ / ResonantEQMO.  Ten fixed-frequency
 * resonant bandpass filters (shared band table / voicing in reseq_dsp.h), and:
 *   Main       = Σ all ten bands (the audio out)
 *   Env1…Env10 = an ENVELOPE FOLLOWER on each band — a live 10-band spectral-CV
 *                bank (a Serge resonant EQ + a 10-band vocoder analysis front-end
 *                in one).
 * Exposed as multi-output sub-outs (Lua: channelCount=11 + subOutLabels; vanilla
 * firmware degrades to Main + Env1).
 *
 * REGEN (single control, scaled per band)
 * ────────────────────────────────────────
 * One Regen amount folds the Main sum back into the filter input, one sample
 * delayed and soft-limited.  Because Main is already the GAIN-WEIGHTED sum of the
 * bands (each band's contribution scales with the level you dialed), feeding back
 * Regen·Main regenerates the BOOSTED bands more than the flat/cut ones — the
 * regeneration follows your EQ curve from one knob, instead of ten.  Pushed up it
 * self-oscillates at the emphasised band frequencies, bounded by the soft-limit.
 *
 * ENVELOPE FOLLOWERS
 * ──────────────────
 * Each band's band-pass contribution is full-wave rectified and one-pole smoothed
 * with shared Rise / Fall times (EnvRise, EnvFall) and an output EnvGain.  The
 * smoothing coefficients come from a small polynomial map (NO expf — am335x
 * clean), and each band's MAX smoothing speed is floored by its own frequency so
 * the 29 Hz band's envelope doesn't ripple at the band period while the highs
 * still track fast.
 *
 * TRIG: no runtime libm trig (kBandG constexpr table — am335x package-trig fix).
 */

#pragma once

#include <od/objects/Object.h>
#include <od/config.h>
#include "reseq_dsp.h"

namespace reseq {

// Regen [0,1] maps to a loop gain of [0, kRegenMax]; > 1 permits self-oscillation
// (bounded by the soft-limited loop).
static constexpr float kRegenMax = 1.20f;

// Envelope one-pole coefficient range (per sample): slow … fast.
static constexpr float kEnvCoefMin = 3.0e-5f;   // ≈ 0.7 s time constant
static constexpr float kEnvCoefMax = 3.0e-2f;   // ≈ 1 ms time constant

class ResonantEQAN : public od::Object
{
public:
    ResonantEQAN();
    virtual ~ResonantEQAN();

#ifndef SWIGLUA
    void process() override;

    // Live band value for a bands graphic (block-rate snapshot, like MO).
    float getBand(int i) const
    {
        return (i >= 0 && i < kNumBands) ? mBandView[i] : 0.0f;
    }

private:
    od::Inlet  mIn {"In"};
    od::Inlet  mBand[kNumBands] {
        od::Inlet{"Band1"}, od::Inlet{"Band2"}, od::Inlet{"Band3"},
        od::Inlet{"Band4"}, od::Inlet{"Band5"}, od::Inlet{"Band6"},
        od::Inlet{"Band7"}, od::Inlet{"Band8"}, od::Inlet{"Band9"},
        od::Inlet{"Band10"}
    };
    od::Inlet  mRegenIn   {"Regen"};
    od::Inlet  mEnvRiseIn {"EnvRise"};
    od::Inlet  mEnvFallIn {"EnvFall"};
    od::Inlet  mEnvGainIn {"EnvGain"};
    od::Inlet  mDriftIn   {"Drift"};
    od::Inlet  mLevelIn   {"Level"};

    od::Outlet mMain {"Main"};       // Out1 (primary) — all bands
    od::Outlet mEnvOut[kNumBands] {  // Out2…Out11 — per-band envelope followers
        od::Outlet{"Env1"},  od::Outlet{"Env2"},  od::Outlet{"Env3"},
        od::Outlet{"Env4"},  od::Outlet{"Env5"},  od::Outlet{"Env6"},
        od::Outlet{"Env7"},  od::Outlet{"Env8"},  od::Outlet{"Env9"},
        od::Outlet{"Env10"}
    };

    BandCoeffs mC[kNumBands];         // per-block band coefficients
    float mBandView[kNumBands] = {0};
    float mIc1[kNumBands] = {0}, mIc2[kNumBands] = {0};
    float mEnv[kNumBands] = {0};      // envelope-follower state per band
    float mFbState = 0.0f;            // last sample's Main, for Regen feedback
    float mLastLevel = 1.0f;

#endif // SWIGLUA
};

} // namespace reseq
