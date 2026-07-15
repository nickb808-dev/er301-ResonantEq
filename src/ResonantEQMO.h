/* ResonantEQMO.h — Serge Resonant Equalizer clone (MONO, MULTI-OUT + FEEDBACK)
 *                  for ER-301 v0.3.0
 *
 * The mono, multi-output sibling of ResonantEQ.  Ten fixed-frequency resonant
 * bandpass filters (shared band table / voicing in reseq_dsp.h) with three outs:
 *   Main   = Σ all ten bands
 *   Comb 1 = Σ odd Serge bands  (29 · 115 · 411 · 1500 · 5200 Hz)
 *   Comb 2 = Σ even Serge bands (61 · 218 · 777 · 2800 · 11000 Hz)
 * exposed as multi-output sub-outs (Lua: channelCount=3 + subOutLabels; vanilla
 * firmware degrades to Main + Comb 1 stereo).
 *
 * FEEDBACK (v0.4.0) — combs → input, per comb
 * ────────────────────────────────────────────
 * Two independent CV-able amounts fold each comb back into the filter input, one
 * sample delayed: Comb1FB scales Comb 1, Comb2FB scales Comb 2.  Because the two
 * combs span disjoint band sets, feeding them back separately lets you regenerate
 * one half of the spectrum without the other — e.g. howl on the odd-band comb
 * while the even-band comb stays clean.  Rising feedback regenerates the
 * resonances and, pushed toward/above unity loop gain, self-oscillates at that
 * comb's band (comb-tooth) frequencies.  Each fed-back signal is soft-limited so
 * the loop stays BOUNDED (musical howl, not a blow-up).  Both = 0 reproduces the
 * plain multi-output EQ exactly; equal amounts match the old single Feedback.
 *
 * TRIG: no runtime libm trig (kBandG constexpr table — am335x package-trig fix).
 */

#pragma once

#include <od/objects/Object.h>
#include <od/config.h>
#include "reseq_dsp.h"

namespace reseq {

// Each feedback inlet [0,1] maps to a loop gain of [0, kFbMax]; kFbMax > 1
// permits self-oscillation (bounded by the soft-limited loop).
static constexpr float kFbMax = 1.20f;

class ResonantEQMO : public od::Object
{
public:
    ResonantEQMO();
    virtual ~ResonantEQMO();

#ifndef SWIGLUA
    void process() override;

private:
    od::Inlet  mIn {"In"};
    od::Inlet  mBand[kNumBands] {
        od::Inlet{"Band1"}, od::Inlet{"Band2"}, od::Inlet{"Band3"},
        od::Inlet{"Band4"}, od::Inlet{"Band5"}, od::Inlet{"Band6"},
        od::Inlet{"Band7"}, od::Inlet{"Band8"}, od::Inlet{"Band9"},
        od::Inlet{"Band10"}
    };
    od::Inlet  mComb1FbIn {"Comb1FB"};
    od::Inlet  mComb2FbIn {"Comb2FB"};
    od::Inlet  mLevelIn    {"Level"};

    od::Outlet mMain  {"Main"};      // Out1 (primary) — all bands
    od::Outlet mComb1 {"Comb1"};     // Out2 — odd bands
    od::Outlet mComb2 {"Comb2"};     // Out3 — even bands

    BandCoeffs mC[kNumBands];        // per-block band coefficients
    float mIc1[kNumBands] = {0}, mIc2[kNumBands] = {0};
    float mFb1State = 0.0f;          // last sample's Comb1, for its feedback path
    float mFb2State = 0.0f;          // last sample's Comb2, for its feedback path
    float mLastLevel = 1.0f;

#endif // SWIGLUA
};

} // namespace reseq
