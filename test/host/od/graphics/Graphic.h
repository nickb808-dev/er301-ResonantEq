// Host-test stub of od/graphics/Graphic.h — just enough to instantiate and
// draw BandsGraphic into an in-memory framebuffer (ASCII-dumpable).
#pragma once
#include <cstdint>
#include <cstring>

namespace od {

struct FrameBuffer {
    static const int W = 256, H = 64;
    uint8_t px[W * H];
    FrameBuffer() { memset(px, 0, sizeof(px)); }
    void fill(int c, int x0, int y0, int x1, int y1)
    {
        for (int y = y0; y <= y1 && y < H; ++y)
            for (int x = x0; x <= x1 && x < W; ++x)
                if (x >= 0 && y >= 0) px[y * W + x] = (uint8_t)c;
    }
    void pixel(int v, int x, int y)
    {
        if (x >= 0 && x < W && y >= 0 && y < H) px[y * W + x] = (uint8_t)v;
    }
};

class Graphic {
public:
    Graphic(int left, int bottom, int width, int height)
        : mWorldLeft(left), mWorldBottom(bottom), mWidth(width), mHeight(height) {}
    virtual ~Graphic() {}
    virtual void draw(FrameBuffer &) {}
protected:
    int mWorldLeft, mWorldBottom, mWidth, mHeight;
};

} // namespace od
