#include "pointer_art.h"

#include <algorithm>
#include <cmath>

namespace
{
struct Vec
{
    double x;
    double y;
};

// The arrowhead in unit space, tip at the origin, y down: the tip, the long
// left edge down to the bottom point, the notch cut into the base, the right
// point. The notch is the one concave corner.
constexpr Vec kShape[] = {{0.00, 0.00}, {0.30, 0.90}, {0.46, 0.57}, {0.90, 0.44}};
constexpr int kCorners = sizeof(kShape) / sizeof(kShape[0]);

// Proportions of the pointer size.
constexpr double kStroke = 0.085; // the cyan band, drawn inside the edge
constexpr double kRim    = 0.035; // dark band outside the edge, for contrast

// Colours as straight RGBA.
struct Rgba
{
    double r, g, b, a;
};
constexpr Rgba kStrokeColour{0x5E, 0xF6, 0xFF, 1.00};
constexpr Rgba kFillColour{0x06, 0x12, 0x1A, 0.45};
constexpr Rgba kRimColour{0x00, 0x00, 0x00, 0.85};

double SegmentDistance(Vec aP, Vec aA, Vec aB)
{
    const double dx = aB.x - aA.x;
    const double dy = aB.y - aA.y;
    const double len2 = dx * dx + dy * dy;
    double       t    = len2 > 0 ? ((aP.x - aA.x) * dx + (aP.y - aA.y) * dy) / len2 : 0;
    t                 = std::clamp(t, 0.0, 1.0);
    const double ex   = aA.x + t * dx - aP.x;
    const double ey   = aA.y + t * dy - aP.y;
    return std::sqrt(ex * ex + ey * ey);
}

bool Inside(Vec aP, const Vec* aPoly)
{
    bool in = false;
    for (int i = 0, j = kCorners - 1; i < kCorners; j = i++)
    {
        const Vec a = aPoly[i];
        const Vec b = aPoly[j];
        if ((a.y > aP.y) != (b.y > aP.y) && aP.x < (b.x - a.x) * (aP.y - a.y) / (b.y - a.y) + a.x)
            in = !in;
    }
    return in;
}
} // namespace

freecursor::PointerArt::Image freecursor::PointerArt::Render(int aSize)
{
    Image img;
    img.size = aSize;
    img.pixels.assign(static_cast<std::size_t>(aSize) * aSize, 0);

    const double stroke = std::max(2.0, kStroke * aSize);
    const double rim    = std::max(1.5, kRim * aSize);
    // The rim sits outside the shape, so the tip is inset by it; the tip
    // itself is the hotspot.
    const double pad   = rim + 1.0;
    const double scale = (aSize - 2.0 * pad) / 0.90;

    Vec poly[kCorners];
    for (int i = 0; i < kCorners; ++i)
        poly[i] = {pad + kShape[i].x * scale, pad + kShape[i].y * scale};

    img.hotX = static_cast<int>(poly[0].x);
    img.hotY = static_cast<int>(poly[0].y);

    // 4x4 supersampling: each sample lands in stroke, fill, rim or nothing,
    // and the pixel is the premultiplied average of its samples.
    constexpr int kSub = 4;
    for (int y = 0; y < aSize; ++y)
    {
        for (int x = 0; x < aSize; ++x)
        {
            double r = 0, g = 0, b = 0, a = 0;
            for (int sy = 0; sy < kSub; ++sy)
            {
                for (int sx = 0; sx < kSub; ++sx)
                {
                    const Vec p{x + (sx + 0.5) / kSub, y + (sy + 0.5) / kSub};
                    double    d = 1e9;
                    for (int i = 0, j = kCorners - 1; i < kCorners; j = i++)
                        d = std::min(d, SegmentDistance(p, poly[j], poly[i]));

                    const Rgba* c = nullptr;
                    if (Inside(p, poly))
                        c = d < stroke ? &kStrokeColour : &kFillColour;
                    else if (d < rim)
                        c = &kRimColour;
                    if (!c)
                        continue;
                    r += c->r * c->a;
                    g += c->g * c->a;
                    b += c->b * c->a;
                    a += c->a;
                }
            }
            if (a <= 0)
                continue;
            const double n     = kSub * kSub;
            const auto   alpha = static_cast<std::uint32_t>(std::lround(a / n * 255.0));
            const auto   red   = static_cast<std::uint32_t>(std::lround(r / a));
            const auto   green = static_cast<std::uint32_t>(std::lround(g / a));
            const auto   blue  = static_cast<std::uint32_t>(std::lround(b / a));
            img.pixels[static_cast<std::size_t>(y) * aSize + x] = (alpha << 24) | (red << 16) | (green << 8) | blue;
        }
    }
    return img;
}
