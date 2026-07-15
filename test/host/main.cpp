// Resonant EQ host verification (both units).  Build:
//   g++ -std=c++17 -O2 -ffast-math -Dprivate=public -Itest/host -Isrc \
//       src/ResonantEQ.cpp src/ResonantEQMO.cpp test/host/main.cpp -o t
//
// Modes:
//   sflat        stereo unit: Main response flat (band centres + ripple)
//   sband <i>    stereo unit: band i boost/cut
//   sstereo      stereo unit: L/R identical, finite
//   moflat       mono MO unit: Main response flat
//   comb         mono MO unit: Comb1=odd, Comb2=even, Main==C1+C2 (fb=0)
//   fb           mono MO unit: fb=0 == plain; rising fb builds; high fb bounded
//   stab         both: all bands +1 (+ MO max feedback) → finite, bounded
//   nan          both: NaN input → finite output
#include "ResonantEQ.h"
#include "ResonantEQMO.h"
#include <hal/fft.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <vector>

using namespace reseq;

static void fill(od::Port &p, float v) { for (int i = 0; i < FRAMELENGTH; ++i) p.buffer()[i] = v; }

// ── Frequency response via impulse → FFT ─────────────────────────────────────
static void toDb(const std::vector<float> &ir, int L, float A, std::vector<double> &magDb)
{
    handle_rfft_t fft = RFFT_allocate(L);
    std::vector<complex_float_t> spec(size_t(L / 2 + 1));
    RFFT_forward(spec.data(), ir.data(), fft);
    RFFT_destroy(fft);
    magDb.assign(size_t(L / 2 + 1), -120.0);
    for (int k = 0; k <= L / 2; ++k) {
        const double m = sqrt(double(spec[k].r) * spec[k].r + double(spec[k].i) * spec[k].i) / A;
        magDb[size_t(k)] = 20.0 * log10(m > 1e-12 ? m : 1e-12);
    }
}
static double atFreq(const std::vector<double> &H, int L, double f)
{
    int k = int(f * double(L) / double(kSampleRate) + 0.5);
    if (k < 0) k = 0; if (k > L / 2) k = L / 2;
    return H[size_t(k)];
}

static void measureStereo(const float cv[kNumBands], int L, std::vector<double> &H)
{
    ResonantEQ d; const float A = 0.001f;
    std::vector<float> ir(size_t(L), 0.0f);
    int got = 0; long blk = 0;
    while (got < L) {
        for (int i = 0; i < kNumBands; ++i) fill(d.mBand[i], cv[i]);
        fill(d.mLevelIn, 1.0f);
        for (int s = 0; s < FRAMELENGTH; ++s) { d.mInL.buffer()[s] = (blk==0&&s==0)?A:0.0f; d.mInR.buffer()[s]=0.0f; }
        d.process();
        for (int s = 0; s < FRAMELENGTH && got < L; ++s) ir[got++] = d.mOutL.buffer()[s];
        ++blk;
    }
    toDb(ir, L, A, H);
}
static void measureMO(const float cv[kNumBands], int L, std::vector<double> &H, int which, float fb = 0.0f)
{
    ResonantEQMO d; const float A = 0.001f;
    std::vector<float> ir(size_t(L), 0.0f);
    int got = 0; long blk = 0;
    while (got < L) {
        for (int i = 0; i < kNumBands; ++i) fill(d.mBand[i], cv[i]);
        fill(d.mComb1FbIn, fb); fill(d.mComb2FbIn, fb); fill(d.mLevelIn, 1.0f);
        for (int s = 0; s < FRAMELENGTH; ++s) d.mIn.buffer()[s] = (blk==0&&s==0)?A:0.0f;
        d.process();
        float *o = which==1 ? d.mComb1.buffer() : which==2 ? d.mComb2.buffer() : d.mMain.buffer();
        for (int s = 0; s < FRAMELENGTH && got < L; ++s) ir[got++] = o[s];
        ++blk;
    }
    toDb(ir, L, A, H);
}
// Like measureMO but with independent per-comb feedback amounts.
static void measureMO2(const float cv[kNumBands], int L, std::vector<double> &H,
                       int which, float fb1, float fb2)
{
    ResonantEQMO d; const float A = 0.001f;
    std::vector<float> ir(size_t(L), 0.0f);
    int got = 0; long blk = 0;
    while (got < L) {
        for (int i = 0; i < kNumBands; ++i) fill(d.mBand[i], cv[i]);
        fill(d.mComb1FbIn, fb1); fill(d.mComb2FbIn, fb2); fill(d.mLevelIn, 1.0f);
        for (int s = 0; s < FRAMELENGTH; ++s) d.mIn.buffer()[s] = (blk==0&&s==0)?A:0.0f;
        d.process();
        float *o = which==1 ? d.mComb1.buffer() : which==2 ? d.mComb2.buffer() : d.mMain.buffer();
        for (int s = 0; s < FRAMELENGTH && got < L; ++s) ir[got++] = o[s];
        ++blk;
    }
    toDb(ir, L, A, H);
}
static void ripple(const std::vector<double> &H, int L, const char *tag)
{
    double lo=1e9, hi=-1e9, sum=0; int n=0;
    for (double f=40; f<=11000; f*=1.02){ double d=atFreq(H,L,f); if(d<lo)lo=d; if(d>hi)hi=d; sum+=d; ++n; }
    printf("%s: 29Hz=%+.2f 11kHz=%+.2f  ripple=%.2f dB mean=%+.2f dB\n",
           tag, atFreq(H,L,29), atFreq(H,L,11000), hi-lo, sum/n);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "mode?\n"); return 2; }
    const int L = 32768;
    float cv[kNumBands]; for (int i = 0; i < kNumBands; ++i) cv[i] = 0.0f;

    if (!strcmp(argv[1], "sflat")) {
        std::vector<double> H; measureStereo(cv, L, H); ripple(H, L, "stereo Main");
        return 0;
    }
    if (!strcmp(argv[1], "sband")) {
        const int bi = argc>=3?atoi(argv[2]):5; const double f = kBandFreqs[bi];
        std::vector<double> H0,Hb,Hc; measureStereo(cv,L,H0);
        cv[bi]=0.5f; measureStereo(cv,L,Hb); cv[bi]=-1.0f; measureStereo(cv,L,Hc);
        const double base=atFreq(H0,L,f), boost=atFreq(Hb,L,f)-base, cut=atFreq(Hc,L,f)-base;
        const bool edge=(bi==0||bi==kNumBands-1);
        const bool ok = boost>9.0 && cut<(edge?-2.5:-8.0);
        printf("stereo band %d @ %.0f: boost=%+.2f cut=%+.2f  %s\n", bi, f, boost, cut, ok?"PASS":"check");
        return ok?0:1;
    }
    if (!strcmp(argv[1], "sstereo")) {
        ResonantEQ d; double maxDiff=0; long nf=0;
        for (int b=0;b<3000;++b){
            for(int i=0;i<kNumBands;++i) fill(d.mBand[i],(i%2==0)?0.4f:-0.3f);
            fill(d.mLevelIn,1.0f);
            for(int s=0;s<FRAMELENGTH;++s){ float x=0.3f*sinf(2*3.14159265f*300*(b*FRAMELENGTH+s)/48000.f); d.mInL.buffer()[s]=x; d.mInR.buffer()[s]=x; }
            d.process();
            for(int s=0;s<FRAMELENGTH;++s){ if(!std::isfinite(d.mOutL.buffer()[s])||!std::isfinite(d.mOutR.buffer()[s]))nf++; maxDiff=std::max(maxDiff,double(fabsf(d.mOutL.buffer()[s]-d.mOutR.buffer()[s]))); }
        }
        printf("stereo L/R diff=%.2e nonFinite=%ld  %s\n", maxDiff, nf, (maxDiff<1e-6&&nf==0)?"PASS":"FAIL");
        return (maxDiff<1e-6&&nf==0)?0:1;
    }

    if (!strcmp(argv[1], "moflat")) {
        std::vector<double> H; measureMO(cv, L, H, 0, 0.0f); ripple(H, L, "MO Main");
        return 0;
    }
    if (!strcmp(argv[1], "comb")) {
        std::vector<double> H1,H2; measureMO(cv,L,H1,1,0.0f); measureMO(cv,L,H2,2,0.0f);
        auto meanAt=[&](const std::vector<double>&H,bool oddSet){ double s=0;int n=0; for(int i=(oddSet?0:1);i<kNumBands;i+=2){s+=atFreq(H,L,kBandFreqs[i]);++n;} return s/n; };
        const double c1o=meanAt(H1,true), c1x=meanAt(H1,false), c2o=meanAt(H2,false), c2x=meanAt(H2,true);
        // Main = C1 + C2 (fb=0, small signal → no limiting)
        ResonantEQMO d; double sumErr=0;
        for(int b=0;b<2000;++b){ for(int i=0;i<kNumBands;++i) fill(d.mBand[i],(i%3==0)?0.4f:-0.2f); fill(d.mComb1FbIn,0.0f); fill(d.mComb2FbIn,0.0f); fill(d.mLevelIn,1.0f);
            for(int s=0;s<FRAMELENGTH;++s) d.mIn.buffer()[s]=0.05f*sinf(2*3.14159265f*220*(b*FRAMELENGTH+s)/48000.f);
            d.process();
            for(int s=0;s<FRAMELENGTH;++s) sumErr=std::max(sumErr,double(fabsf(d.mMain.buffer()[s]-(d.mComb1.buffer()[s]+d.mComb2.buffer()[s])))); }
        const bool ok=(c1o-c1x)>2.0&&(c2o-c2x)>2.0&&sumErr<1e-5;
        printf("comb: C1 Δ%.1f  C2 Δ%.1f  Main-(C1+C2)=%.2e  %s\n", c1o-c1x, c2o-c2x, sumErr, ok?"PASS":"FAIL");
        return ok?0:1;
    }
    if (!strcmp(argv[1], "fb")) {
        // (a) fb=0 identical to plain multiout; (b) rising fb lifts the resonant
        // peak; (c) high fb self-oscillates but stays bounded & finite.
        for (int i=0;i<kNumBands;++i) cv[i]=0.3f;               // mild boost on all
        std::vector<double> H0,H1; measureMO(cv,L,H0,0,0.0f); measureMO(cv,L,H1,0,0.7f);
        const double peak0=atFreq(H0,L,777), peak1=atFreq(H1,L,777);
        // self-oscillation at full fb, no input, resonant bands:
        ResonantEQMO d; long nf=0; float peak=0; double energy=0;
        for(int b=0;b<8000;++b){ for(int i=0;i<kNumBands;++i) fill(d.mBand[i],0.6f); fill(d.mComb1FbIn,1.0f); fill(d.mComb2FbIn,1.0f); fill(d.mLevelIn,1.0f);
            for(int s=0;s<FRAMELENGTH;++s) d.mIn.buffer()[s]=(b==0&&s==0)?0.3f:0.0f;   // single ping, then silence
            d.process();
            for(int s=0;s<FRAMELENGTH;++s){ float y=d.mMain.buffer()[s]; if(!std::isfinite(y))nf++; else{ if(fabsf(y)>peak)peak=fabsf(y); if(b>4000)energy+=double(y)*y; } } }
        const bool sustains = energy > 1.0;   // still ringing long after the ping → self-oscillating
        const bool ok = nf==0 && peak<=1.001f && peak1>peak0+3.0 && sustains;
        printf("fb: peak@777 fb0=%+.1f fb.7=%+.1f (Δ%+.1f)  selfOsc peak=%.3f sustainE=%.1f nf=%ld  %s\n",
               peak0, peak1, peak1-peak0, peak, energy, nf, ok?"PASS":"FAIL");
        return ok?0:1;
    }

    if (!strcmp(argv[1], "fbsplit")) {
        // Per-comb feedback is INDEPENDENT: each control regenerates ITS OWN comb
        // more than the other.  The two combs share one filter input, so there is
        // some cross-coupling (and near self-oscillation a global mode dominates),
        // but at moderate feedback each comb's own band is clearly favoured:
        //   Comb1FB lifts Comb1@411 (odd set) MORE than Comb2@777 (even set),
        //   Comb2FB lifts Comb2@777 MORE than Comb1@411.
        for (int i=0;i<kNumBands;++i) cv[i]=0.3f;
        const float fb = 0.3f;
        std::vector<double> H0c1,H0c2, H1c1,H1c2, H2c1,H2c2;
        measureMO2(cv,L,H0c1,1,0.0f,0.0f); measureMO2(cv,L,H0c2,2,0.0f,0.0f);  // baseline
        measureMO2(cv,L,H1c1,1,fb,0.0f);   measureMO2(cv,L,H1c2,2,fb,0.0f);    // only fb1
        measureMO2(cv,L,H2c1,1,0.0f,fb);   measureMO2(cv,L,H2c2,2,0.0f,fb);    // only fb2
        const double d1_c1 = atFreq(H1c1,L,411) - atFreq(H0c1,L,411);  // fb1 on Comb1
        const double d1_c2 = atFreq(H1c2,L,777) - atFreq(H0c2,L,777);  // fb1 on Comb2
        const double d2_c2 = atFreq(H2c2,L,777) - atFreq(H0c2,L,777);  // fb2 on Comb2
        const double d2_c1 = atFreq(H2c1,L,411) - atFreq(H0c1,L,411);  // fb2 on Comb1
        const bool ok = d1_c1 > 3.0 && d1_c1 > d1_c2 + 3.0 &&          // fb1 favours Comb1
                        d2_c2 > 3.0 && d2_c2 > d2_c1 + 3.0;            // fb2 favours Comb2
        printf("fbsplit: fb1→ C1@411 %+.1f (>C2@777 %+.1f) | fb2→ C2@777 %+.1f (>C1@411 %+.1f)  %s\n",
               d1_c1, d1_c2, d2_c2, d2_c1, ok?"PASS":"FAIL");
        return ok?0:1;
    }

    if (!strcmp(argv[1], "stab")) {
        // Stereo all bands +1 + MO all bands +1 with MAX feedback, long noise run.
        ResonantEQ ds; ResonantEQMO dm; uint32_t rng=0x1234567u; long nf=0; float peak=0;
        for(int b=0;b<20000;++b){
            for(int i=0;i<kNumBands;++i){ fill(ds.mBand[i],1.0f); fill(dm.mBand[i],1.0f); }
            fill(ds.mLevelIn,1.0f); fill(dm.mLevelIn,1.0f); fill(dm.mComb1FbIn,1.0f); fill(dm.mComb2FbIn,1.0f);
            for(int s=0;s<FRAMELENGTH;++s){ rng^=rng<<13;rng^=rng>>17;rng^=rng<<5; float x=(float(rng&0xFFFF)/32768.f-1.f)*0.5f; ds.mInL.buffer()[s]=x; ds.mInR.buffer()[s]=x; dm.mIn.buffer()[s]=x; }
            ds.process(); dm.process();
            for(int s=0;s<FRAMELENGTH;++s){ float a=ds.mOutL.buffer()[s],b1=dm.mMain.buffer()[s],b2=dm.mComb1.buffer()[s],b3=dm.mComb2.buffer()[s];
                if(!std::isfinite(a)||!std::isfinite(b1)||!std::isfinite(b2)||!std::isfinite(b3))nf++;
                peak=std::max(peak,std::max(fabsf(a),std::max(fabsf(b1),std::max(fabsf(b2),fabsf(b3))))); }
        }
        printf("stab: nonFinite=%ld peak=%.3f  %s\n", nf, peak, (nf==0&&peak<=1.001f)?"PASS":"FAIL");
        return (nf==0&&peak<=1.001f)?0:1;
    }
    if (!strcmp(argv[1], "nan")) {
        ResonantEQ ds; ResonantEQMO dm; long nf=0;
        for(int b=0;b<3000;++b){ const bool burst=(b>=500&&b<510);
            for(int i=0;i<kNumBands;++i){ fill(ds.mBand[i],0.3f); fill(dm.mBand[i],0.3f); }
            fill(ds.mLevelIn,1.0f); fill(dm.mLevelIn,1.0f); fill(dm.mComb1FbIn,0.8f); fill(dm.mComb2FbIn,0.8f);
            for(int s=0;s<FRAMELENGTH;++s){ float x=0.3f*sinf(2*3.14159265f*440*(b*FRAMELENGTH+s)/48000.f); if(burst&&(s&3)==0)x=0.0f/0.0f; ds.mInL.buffer()[s]=x; ds.mInR.buffer()[s]=x; dm.mIn.buffer()[s]=x; }
            ds.process(); dm.process();
            for(int s=0;s<FRAMELENGTH;++s){ if(!std::isfinite(ds.mOutL.buffer()[s])||!std::isfinite(dm.mMain.buffer()[s]))nf++; } }
        printf("nan: nonFinite=%ld  %s\n", nf, nf?"FAIL":"PASS");
        return nf?1:0;
    }

    fprintf(stderr, "unknown mode\n");
    return 2;
}
