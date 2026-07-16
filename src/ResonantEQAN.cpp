/* ResonantEQAN.cpp — Serge Resonant EQ (MONO ANALYZER, MULTI-OUT) for ER-301
 *
 * See ResonantEQAN.h; shared DSP in reseq_dsp.h.
 *
 * Per sample:
 *   x   = in + regen·softLimit(Main_prev)          (Regen scales per band via Main)
 *   run 10 Cytomic SVFs on x; contrib_i = coef_i·v1_i
 *   Main   = Σ contrib
 *   Env_i  = one-pole( |contrib_i| )  with Rise/Fall, per-band release floor
 *   feedback state ← Main   (one sample delayed by store-after-use)
 * No runtime libm trig; envelope coefficients from a polynomial map (no expf). */

#include "ResonantEQAN.h"

#include <algorithm>

namespace reseq {

// Map an [0,1] Rise/Fall knob to a one-pole coefficient (no libm): a cubic curve
// gives fine control at the slow end.  0 = slow/smooth … 1 = fast/snappy.
static inline float envCoef(float knob)
{
    const float k = knob < 0.0f ? 0.0f : (knob > 1.0f ? 1.0f : knob);
    const float c = k * k * k;                       // fine at the slow end
    return kEnvCoefMin + (kEnvCoefMax - kEnvCoefMin) * c;
}

ResonantEQAN::ResonantEQAN()
{
    addInput(mIn);
    for (int i = 0; i < kNumBands; ++i) addInput(mBand[i]);
    addInput(mRegenIn);
    addInput(mEnvRiseIn);
    addInput(mEnvFallIn);
    addInput(mEnvGainIn);
    addInput(mDriftIn);
    addInput(mLevelIn);
    addOutput(mMain);
    for (int i = 0; i < kNumBands; ++i) addOutput(mEnvOut[i]);

    for (int i = 0; i < kNumBands; ++i) mC[i] = computeBand(i, 0.0f);  // flat prime
}

ResonantEQAN::~ResonantEQAN() {}

void ResonantEQAN::process()
{
    const float *in   = mIn.buffer();
    float       *main = mMain.buffer();
    const int N = FRAMELENGTH;

    // Per-band coefficients + graphic snapshot.
    const float drift = std::max(0.0f, std::min(mDriftIn.buffer()[0], 1.0f));
    for (int i = 0; i < kNumBands; ++i) {
        const float cv = mBand[i].buffer()[0];
        mBandView[i] = cv < -1.0f ? -1.0f : (cv > 1.0f ? 1.0f : cv);
        mC[i] = computeBand(i, cv, drift);
    }

    const float regen   = std::max(0.0f, std::min(mRegenIn.buffer()[0], 1.0f)) * kRegenMax;
    const float envGain = std::max(0.0f, std::min(mEnvGainIn.buffer()[0], 4.0f));
    const float atk     = envCoef(mEnvRiseIn.buffer()[0]);
    const float rel     = envCoef(mEnvFallIn.buffer()[0]);
    const float level   = std::max(0.0f, std::min(mLevelIn.buffer()[0], 2.0f));
    const float lvStep  = (level - mLastLevel) * (1.0f / float(N));
    float lv = mLastLevel;

    // Per-band effective attack/release: floor the MAX speed by band frequency so
    // low bands don't ripple at the band period (attack ≤ 1 period, release ≤ ½).
    float atkB[kNumBands], relB[kNumBands];
    for (int i = 0; i < kNumBands; ++i) {
        const float f    = kBandFreqs[i];
        const float capA = f * (1.0f / float(kSampleRate));          // ~1 period
        const float capR = f * (0.5f / float(kSampleRate));          // ~2 periods
        atkB[i] = atk < capA ? atk : capA;
        relB[i] = rel < capR ? rel : capR;
    }

    // Envelope-output buffers (mono CV per band).
    float *env[kNumBands];
    for (int i = 0; i < kNumBands; ++i) env[i] = mEnvOut[i].buffer();

    float fbState = mFbState;

    for (int s = 0; s < N; ++s) {
        const float x = sanitize(in[s]) + regen * softLimit(fbState);
        float accMain = 0.0f;

        for (int i = 0; i < kNumBands; ++i) {
            const float a1 = mC[i].a1, a2 = mC[i].a2, a3 = mC[i].a3, c = mC[i].coef;

            const float v3 = x - mIc2[i];
            const float v1 = a1 * mIc1[i] + a2 * v3;
            const float v2 = mIc2[i] + a2 * mIc1[i] + a3 * v3;
            mIc1[i] = 2.0f * v1 - mIc1[i];
            mIc2[i] = 2.0f * v2 - mIc2[i];

            const float contrib = c * v1;
            accMain += contrib;

            // Envelope follower: rectify + asymmetric one-pole.
            const float r    = contrib < 0.0f ? -contrib : contrib;
            const float coef = r > mEnv[i] ? atkB[i] : relB[i];
            mEnv[i] += coef * (r - mEnv[i]);
            env[i][s] = sanitize(mEnv[i] * envGain);
        }

        fbState = sanitize(accMain);          // Main feeds the Regen path next sample

        lv += lvStep;
        main[s] = softLimit(sanitize(accMain)) * lv;
    }

    mFbState   = fbState;
    mLastLevel = level;
}

} // namespace reseq
