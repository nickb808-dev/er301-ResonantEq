// BandsGraphic — phosphor band-value scope for the Resonant EQ (both units).
//
// Ten bipolar bars, one per Serge band (29 Hz … 11 kHz left→right, matching
// the control strip order): X = band, Y = the band's CV value.
//
//   centre line   = 0 (flat)
//   bar up        = boost (0 … +0.5) then RESONANCE (+0.5 … +1, crest at 15 —
//                   the dashed "ring line" marks the +0.5 threshold)
//   bar down      = cut toward the −0.5 removal point, below it the band
//                   INVERTS into a cancellation notch (down to −1)
//
// A phosphor buffer with −1/frame decay makes moving CVs leave trails, so a
// swept or modulated band reads as a smear between its recent values — the
// res-e analogue of Dirac's grain field and Varia's slope scope, at the same
// one-control width (SECTION_PLY).
//
// Modeled directly on Dirac's DiracFieldGraphic / Varia's SlopeGraphic
// (Habitat MirrorPhosphorGraphic pattern): header-only with INLINE virtuals, a
// uint8 brightness buffer as a CLASS MEMBER (never a stack array — Cortex-A8
// :64 trap), fade −1 per frame, additive hits, fb.fill(BLACK) + fb.pixel.
//
// am335x TRIG: pure integer/float arithmetic — no libm at all in this header.
// The band values are block-rate snapshots (getBand) published by process(),
// same cross-thread display-read pattern as Varia's getFL/getFH/getK.

#pragma once

#include "ResonantEQ.h"
#include "ResonantEQMO.h"
#include <od/graphics/Graphic.h>
#include <od/graphics/constants.h>
#include <string.h>

namespace reseq
{

  class BandsGraphic : public od::Graphic
  {
  public:
    BandsGraphic(int left, int bottom, int width, int height)
        : od::Graphic(left, bottom, width, height), mpEQ(0), mpMO(0) {}

    virtual ~BandsGraphic()
    {
      if (mpEQ) mpEQ->release();
      if (mpMO) mpMO->release();
    }

    // Follow the stereo unit…
    void follow(ResonantEQ *p)
    {
      if (mpEQ) mpEQ->release();
      if (mpMO) { mpMO->release(); mpMO = 0; }
      mpEQ = p;
      if (mpEQ) mpEQ->attach();
    }

    // …or the mono multi-out unit (distinct name — SWIG-friendly dispatch).
    void followMO(ResonantEQMO *p)
    {
      if (mpMO) mpMO->release();
      if (mpEQ) { mpEQ->release(); mpEQ = 0; }
      mpMO = p;
      if (mpMO) mpMO->attach();
    }

  private:
    ResonantEQ   *mpEQ;
    ResonantEQMO *mpMO;

    static const int kMaxW = 128;
    static const int kMaxH = 64;
    uint8_t mPixels[kMaxW * kMaxH];
    bool    mCleared = false;

    inline void addPix(int w, int h, int x, int y, int amt)
    {
      if (x < 0 || x >= w || y < 0 || y >= h) return;
      int idx = y * w + x;
      int b = mPixels[idx] + amt;
      if (b > 15) b = 15;
      mPixels[idx] = (uint8_t)b;
    }

  public:
    virtual void draw(od::FrameBuffer &fb)
    {
      int w = mWidth  < kMaxW ? mWidth  : kMaxW;
      int h = mHeight < kMaxH ? mHeight : kMaxH;

      if (!mCleared) { memset(mPixels, 0, sizeof(mPixels)); mCleared = true; }

      // Phosphor decay — every pixel fades one level per frame (trails).
      for (int i = 0; i < w * h; i++)
        if (mPixels[i] > 0) mPixels[i]--;

      if (mpEQ || mpMO)
      {
        const int midY  = h / 2;              // CV 0 (flat)
        const int span  = midY - 1;           // px per unit CV, each direction
        const int ringY = midY + span / 2;    // +0.5 — top of boost, start of ring

        // PHOSPHOR PHYSICS: with a −1/frame decay, any repeated splat ≥ 2
        // saturates to 15 at steady state; a splat of exactly 1 holds at a
        // steady dim level.  So static geometry uses amt 1 (stems, reference
        // dashes) and only the VALUE TIP is driven bright — a moving tip
        // leaves the classic decaying trail, a still one reads as a crisp dot.
        for (int x = 0; x < w; x += 3)
        {
          addPix(w, h, x, midY, 1);                        // centre (flat) line
          addPix(w, h, x, ringY, 1);                       // +0.5 ring threshold
        }

        for (int i = 0; i < kNumBands; i++)
        {
          float v = mpEQ ? mpEQ->getBand(i) : mpMO->getBand(i);
          if (!(v == v)) continue;                          // NaN guard
          if (v < -1.0f) v = -1.0f; else if (v > 1.0f) v = 1.0f;

          // Bar column: band i owns [x0, x1) of the width; 1 px gutter.
          const int x0 = (i * w) / kNumBands + 1;
          const int x1 = ((i + 1) * w) / kNumBands;         // exclusive

          const int tipY = midY + (int)(v * (float)span + (v >= 0.0f ? 0.5f : -0.5f));
          const int y0 = tipY < midY ? tipY : midY;
          const int y1 = tipY < midY ? midY : tipY;

          const bool ring = (v > 0.5f);
          for (int x = x0; x < x1; x++)
          {
            for (int yy = y0; yy <= y1; yy++) addPix(w, h, x, yy, 1);  // dim stem
            addPix(w, h, x, tipY, ring ? 15 : 9);                     // value tip
            if (ring)                                       // ringing band: fat
              addPix(w, h, x, tipY - 1, 12);                // 2-px tip, unmissable
          }
        }
      }

      // Background + phosphor render (monochrome; v is the 4-bit intensity).
      fb.fill(BLACK, mWorldLeft, mWorldBottom,
              mWorldLeft + mWidth - 1, mWorldBottom + mHeight - 1);
      for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
          uint8_t v = mPixels[y * w + x];
          if (v > 0)
            fb.pixel(v, mWorldLeft + x, mWorldBottom + y);
        }
    }
  };

} // namespace reseq
