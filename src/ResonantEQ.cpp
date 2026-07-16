/* ResonantEQ.cpp — Serge Resonant Equalizer clone (STEREO) for ER-301 v0.5.0
 *
 * See ResonantEQ.h; shared DSP in reseq_dsp.h.  Ten parallel Cytomic SVFs per
 * channel, summed to OutL / OutR.  No runtime libm trig. */

#include "ResonantEQ.h"

#include <algorithm>

namespace reseq {

ResonantEQ::ResonantEQ()
{
    addInput(mInL);
    addInput(mInR);
    for (int i = 0; i < kNumBands; ++i) addInput(mBand[i]);
    addInput(mDriftIn);
    addInput(mLevelIn);
    addOutput(mOutL);
    addOutput(mOutR);

    for (int i = 0; i < kNumBands; ++i) mC[i] = computeBand(i, 0.0f);  // flat prime
}

ResonantEQ::~ResonantEQ() {}

void ResonantEQ::process()
{
    const float *inL = mInL.buffer();
    const float *inR = mInR.buffer();
    float *outL = mOutL.buffer();
    float *outR = mOutR.buffer();
    const int N = FRAMELENGTH;

    const float drift = std::max(0.0f, std::min(mDriftIn.buffer()[0], 1.0f));
    for (int i = 0; i < kNumBands; ++i) {
        const float cv = mBand[i].buffer()[0];
        mBandView[i] = cv < -1.0f ? -1.0f : (cv > 1.0f ? 1.0f : cv);
        mC[i] = computeBand(i, cv, drift);
    }

    const float level  = std::max(0.0f, std::min(mLevelIn.buffer()[0], 2.0f));
    const float lvStep = (level - mLastLevel) * (1.0f / float(N));
    float lv = mLastLevel;

    for (int s = 0; s < N; ++s) {
        const float xL = sanitize(inL[s]);
        const float xR = sanitize(inR[s]);
        float accL = 0.0f, accR = 0.0f;

        for (int i = 0; i < kNumBands; ++i) {
            const float a1 = mC[i].a1, a2 = mC[i].a2, a3 = mC[i].a3, c = mC[i].coef;

            float v3 = xL - mIc2L[i];
            float v1 = a1 * mIc1L[i] + a2 * v3;
            float v2 = mIc2L[i] + a2 * mIc1L[i] + a3 * v3;
            mIc1L[i] = 2.0f * v1 - mIc1L[i];
            mIc2L[i] = 2.0f * v2 - mIc2L[i];
            accL += c * v1;

            v3 = xR - mIc2R[i];
            v1 = a1 * mIc1R[i] + a2 * v3;
            v2 = mIc2R[i] + a2 * mIc1R[i] + a3 * v3;
            mIc1R[i] = 2.0f * v1 - mIc1R[i];
            mIc2R[i] = 2.0f * v2 - mIc2R[i];
            accR += c * v1;
        }

        lv += lvStep;
        outL[s] = softLimit(sanitize(accL)) * lv;
        outR[s] = softLimit(sanitize(accR)) * lv;
    }

    mLastLevel = level;
}

} // namespace reseq
