#pragma once

#include "types.h"

void InitPosCheckArray();

struct PixelBlock;
struct PixelBlockDistanceFinder
{
    ~PixelBlockDistanceFinder()
    {
        delete[] pixelMaskFullRez;
    }

    // high rez and low rez pixel mask - 1 bit per pixel on/off
    u64* pixelMaskFullRez = nullptr;

    int w = 0;
    int h = 0;
    int fullPitch = 0;

    void Generate(const PixelBlock& source);
    int FindDistance(int cx, int cy, int range) const;
    void Dump() const;
};

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
    void GenerateSDF(const PixelBlock& source, const PixelBlockDistanceFinder& sourceDF, int range);
    void BicubicScale(const PixelBlock& source);
    void Dump();
};

