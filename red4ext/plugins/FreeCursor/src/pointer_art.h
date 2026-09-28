#pragma once

#include <cstdint>
#include <vector>

namespace freecursor::PointerArt
{
// A pointer drawn in the style of the game's menu cursor: a hollow cyan
// arrowhead with a notched base and a dark rim, so it reads on bright and
// dark scenes alike. Pure: no Windows calls, so it can be rendered and
// checked outside the game.
struct Image
{
    int                        size = 0;  // square, size x size pixels
    int                        hotX = 0;  // the arrow's tip
    int                        hotY = 0;
    std::vector<std::uint32_t> pixels;    // 0xAARRGGBB, straight alpha, top row first
};

Image Render(int aSize);
} // namespace freecursor::PointerArt
