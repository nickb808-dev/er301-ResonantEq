/* ResonantEQ.h — Serge Resonant Equalizer clone (STEREO) for ER-301 v0.5.0
 *
 * The plain stereo Resonant EQ: ten fixed-frequency resonant bandpass filters
 * summed to a single output, per channel.  Bands at the classic Serge set
 * (29 · 61 · 115 · 218 · 411 · 777 · 1500 · 2800 · 5200 · 11000 Hz), each with a
 * fully CV-able amount (cut / flat / boost / resonate).  Stereo in → stereo out;
 * both channels share one set of per-band gains/Qs but keep separate filter
 * state, so a stereo signal is EQ'd coherently.
 *
 * This is the no-combs, no-feedback unit.  Its sibling **ResonantEQMO** is mono
 * with the Main / Comb 1 / Comb 2 multi-output and combs→input feedback.
 *
 * Shared DSP (band table, precomputed g = tan(π·f/fs) — NO runtime libm trig,
 * sanitize/softLimit, per-band coefficients) lives in reseq_dsp.h.
 */

#pragma once

#include <od/objects/Object.h>
#include <od/config.h>
#include "reseq_dsp.h"

namespace reseq {

class ResonantEQ : public od::Object
{
public:
    ResonantEQ();
    virtual ~ResonantEQ();

#ifndef SWIGLUA
    void process() override;

    // Live band value for the bands graphic (read each UI frame; block-rate
    // snapshot published by process() — Varia getFL/getFH pattern).
    float getBand(int i) const
    {
        return (i >= 0 && i < kNumBands) ? mBandView[i] : 0.0f;
    }

private:
    od::Inlet  mInL {"InL"};
    od::Inlet  mInR {"InR"};
    od::Inlet  mBand[kNumBands] {
        od::Inlet{"Band1"}, od::Inlet{"Band2"}, od::Inlet{"Band3"},
        od::Inlet{"Band4"}, od::Inlet{"Band5"}, od::Inlet{"Band6"},
        od::Inlet{"Band7"}, od::Inlet{"Band8"}, od::Inlet{"Band9"},
        od::Inlet{"Band10"}
    };
    od::Inlet  mLevelIn {"Level"};

    od::Outlet mOutL {"OutL"};
    od::Outlet mOutR {"OutR"};

    BandCoeffs mC[kNumBands];                      // per-block band coefficients
    float mBandView[kNumBands] = {0};              // clamped CVs, for the graphic
    float mIc1L[kNumBands] = {0}, mIc2L[kNumBands] = {0};
    float mIc1R[kNumBands] = {0}, mIc2R[kNumBands] = {0};
    float mLastLevel = 1.0f;

#endif // SWIGLUA
};

} // namespace reseq
