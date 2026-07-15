/* ResonantEQMO.cpp — Serge Resonant EQ (MONO, MULTI-OUT + FEEDBACK) for ER-301
 *
 * See ResonantEQMO.h; shared DSP in reseq_dsp.h.
 *
 * Per sample:
 *   x   = in + fb1·softLimit(Comb1_prev) + fb2·softLimit(Comb2_prev)
 *   run 10 Cytomic SVFs on x
 *   Main   = Σ all,   Comb1 = Σ even-idx bands,  Comb2 = Σ odd-idx bands
 *   feedback states ← Comb1, Comb2   (delayed one sample by store-after-use)
 * The one-sample delay breaks the algebraic loop; the per-comb soft-limit bounds
 * self-oscillation.  No runtime libm trig. */

#include "ResonantEQMO.h"

#include <algorithm>

namespace reseq {

ResonantEQMO::ResonantEQMO()
{
    addInput(mIn);
    for (int i = 0; i < kNumBands; ++i) addInput(mBand[i]);
    addInput(mComb1FbIn);
    addInput(mComb2FbIn);
    addInput(mLevelIn);
    addOutput(mMain);
    addOutput(mComb1);
    addOutput(mComb2);

    for (int i = 0; i < kNumBands; ++i) mC[i] = computeBand(i, 0.0f);  // flat prime
}

ResonantEQMO::~ResonantEQMO() {}

void ResonantEQMO::process()
{
    const float *in    = mIn.buffer();
    float       *main  = mMain.buffer();
    float       *comb1 = mComb1.buffer();
    float       *comb2 = mComb2.buffer();
    const int N = FRAMELENGTH;

    for (int i = 0; i < kNumBands; ++i)
        mC[i] = computeBand(i, mBand[i].buffer()[0]);

    const float fb1    = std::max(0.0f, std::min(mComb1FbIn.buffer()[0], 1.0f)) * kFbMax;
    const float fb2    = std::max(0.0f, std::min(mComb2FbIn.buffer()[0], 1.0f)) * kFbMax;
    const float level  = std::max(0.0f, std::min(mLevelIn.buffer()[0], 2.0f));
    const float lvStep = (level - mLastLevel) * (1.0f / float(N));
    float lv = mLastLevel;
    float fb1State = mFb1State;
    float fb2State = mFb2State;

    for (int s = 0; s < N; ++s) {
        // each comb → input, one sample delayed, soft-limited so the loop is bounded.
        const float x = sanitize(in[s]) + fb1 * softLimit(fb1State) + fb2 * softLimit(fb2State);
        float accMain = 0.0f, accC1 = 0.0f, accC2 = 0.0f;

        for (int i = 0; i < kNumBands; ++i) {
            const float a1 = mC[i].a1, a2 = mC[i].a2, a3 = mC[i].a3, c = mC[i].coef;

            const float v3 = x - mIc2[i];
            const float v1 = a1 * mIc1[i] + a2 * v3;
            const float v2 = mIc2[i] + a2 * mIc1[i] + a3 * v3;
            mIc1[i] = 2.0f * v1 - mIc1[i];
            mIc2[i] = 2.0f * v2 - mIc2[i];

            const float contrib = c * v1;
            accMain += contrib;
            if (i & 1) accC2 += contrib;   // odd bands → Comb 2
            else       accC1 += contrib;   // even bands → Comb 1
        }

        fb1State = sanitize(accC1);   // each comb feeds its own path next sample
        fb2State = sanitize(accC2);

        lv += lvStep;
        main[s]  = softLimit(sanitize(accMain)) * lv;
        comb1[s] = softLimit(sanitize(accC1))  * lv;
        comb2[s] = softLimit(sanitize(accC2))  * lv;
    }

    mFb1State  = fb1State;
    mFb2State  = fb2State;
    mLastLevel = level;
}

} // namespace reseq
