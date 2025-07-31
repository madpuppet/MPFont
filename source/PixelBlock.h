#pragma once

#include "types.h"

struct PixelBlock
{
    u32* pixels = nullptr;
    int w = 0;
    int h = 0;
    int pitch = 0;
    int crop_x = 0;
    int crop_y = 0;
    int crop_w = 0;
    int crop_h = 0;

    void CalcCropRect();
    void GenerateSDF(const PixelBlock& source, int range);
    int FindDistance(int xx, int yy, int range) const;
};
