// bands_render — ASCII render of BandsGraphic via the host od stubs.
// Verifies (without hardware): bar per band at the right column, bipolar
// direction, resonant crest (v > 0.5) at full phosphor, centre/ring reference
// lines, phosphor decay trails after a value jump, follow/followMO both work.
//
// Build: g++ -std=c++17 -O2 -ffast-math -Dprivate=public -Itest/host -Isrc
//        src/ResonantEQ.cpp src/ResonantEQMO.cpp test/host/bands_render.cpp -o br
#include "BandsGraphic.h"
#include <cstdio>

using namespace reseq;

static void fill(od::Port &p, float v)
{
    for (int i = 0; i < FRAMELENGTH; ++i) p.buffer()[i] = v;
}

static void dump(od::FrameBuffer &fb, int w, int h, const char *title)
{
    printf("---- %s (top = +1, mid = flat, bottom = -1) ----\n", title);
    static const char ramp[17] = " .:-=+*#%@$&8B0M";
    for (int y = h - 1; y >= 0; --y) {          // world Y up → print top first
        putchar('|');
        for (int x = 0; x < w; ++x) putchar(ramp[fb.px[y * od::FrameBuffer::W + x] & 15]);
        printf("|\n");
    }
}

int main()
{
    const int W = 42, H = 64;                    // SECTION_PLY x panel height

    // ── Stereo unit: a recognisable pattern across the 10 bands ────────────
    ResonantEQ eq;
    const float cvs[10] = {-1.0f, -0.5f, -0.25f, 0.0f, 0.25f,
                            0.5f,  0.75f, 1.0f,  0.0f, -0.75f};
    for (int i = 0; i < 10; ++i) fill(eq.mBand[i], cvs[i]);
    for (int b = 0; b < 4; ++b) eq.process();    // publish mBandView

    BandsGraphic g(0, 0, W, H);
    g.follow(&eq);

    od::FrameBuffer fb;
    for (int f = 0; f < 8; ++f) g.draw(fb);      // settle phosphor
    dump(fb, W, H, "stereo EQ: -1 -.5 -.25 0 .25 .5 .75 +1 0 -.75");

    // ── Trails: jump band 8 (0-based idx 7) from +1 to 0, draw a few frames ─
    fill(eq.mBand[7], 0.0f);
    eq.process();
    for (int f = 0; f < 4; ++f) g.draw(fb);
    dump(fb, W, H, "4 frames after band8 +1 -> 0 (trail should linger)");

    // ── MO unit via followMO ───────────────────────────────────────────────
    ResonantEQMO mo;
    for (int i = 0; i < 10; ++i) fill(mo.mBand[i], (i % 2) ? 0.8f : -0.4f);
    for (int b = 0; b < 4; ++b) mo.process();
    BandsGraphic g2(0, 0, W, H);
    g2.followMO(&mo);
    od::FrameBuffer fb2;
    for (int f = 0; f < 8; ++f) g2.draw(fb2);
    dump(fb2, W, H, "MO: alternating -0.4 / +0.8");

    printf("bands_render done\n");
    return 0;
}
