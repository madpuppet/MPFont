#include "PixelBlock.h"
#include "math.h"
#include "types.h"
#include "SDL.h"

#include <cstdint>
#include <cmath>
#include <memory>
#include <algorithm>

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <memory>
#include <string>
#include <format>

struct PosCheck
{
    int xo, yo;
    float dist;
};

std::vector<PosCheck> g_posChecks;

void InitPosCheckArray()
{
    for (int yo = -32; yo <= 32; yo++)
    {
        for (int xo = -32; xo <= 32; xo++)
        {
            if (xo == 0 && yo == 0)
                continue;

            float dist = (int)sqrtf((float)(xo * xo + yo * yo)) / 32.0f * 127.0f;
            if (dist < 128.0f)
                g_posChecks.emplace_back(xo, yo, dist);
        }
    }

    std::sort(g_posChecks.begin(), g_posChecks.end(), [](const PosCheck& a, const PosCheck& b)->bool { return a.dist < b.dist; });
}


// Mitchell-Netravali cubic filter
float cubicFilter(float x) {
    const float B = 1.0f / 3.0f;
    const float C = 1.0f / 3.0f;
    x = std::abs(x);

    if (x < 1.0f) {
        return ((12 - 9 * B - 6 * C) * x * x * x +
            (-18 + 12 * B + 6 * C) * x * x +
            (6 - 2 * B)) / 6.0f;
    }
    else if (x < 2.0f) {
        return ((-B - 6 * C) * x * x * x +
            (6 * B + 30 * C) * x * x +
            (-12 * B - 48 * C) * x +
            (8 * B + 24 * C)) / 6.0f;
    }
    else {
        return 0.0f;
    }
}

// Clamp float to byte
inline uint8_t clampByte(float x) {
    return static_cast<uint8_t>(std::clamp(x, 0.0f, 255.0f));
}

// Get channel from ARGB
inline uint8_t getChannel(uint32_t pixel, int channel) {
    return (pixel >> (24 - channel * 8)) & 0xFF; // 0=A, 1=R, 2=G, 3=B
}

void PixelBlock::BicubicScale(const PixelBlock &source)
{
    int sourcePitch = source.pitch / 4;
    int destPitch = pitch / 4;

    float scaleX = static_cast<float>(source.w) / static_cast<float>(w);
    float scaleY = static_cast<float>(source.h) / static_cast<float>(h);

    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            float srcX = (dx + 0.5f) * scaleX - 0.5f;
            float srcY = (dy + 0.5f) * scaleY - 0.5f;

            int xInt = static_cast<int>(std::floor(srcX));
            int yInt = static_cast<int>(std::floor(srcY));
            float dxF = srcX - xInt;
            float dyF = srcY - yInt;

            float weightsX[4], weightsY[4];
            for (int i = 0; i < 4; ++i) {
                weightsX[i] = cubicFilter(i - 1 - dxF);
                weightsY[i] = cubicFilter(i - 1 - dyF);
            }

            float sum[4] = {}; // A, R, G, B

            for (int m = 0; m < 4; ++m) {
                int sy = std::clamp(yInt + m - 1, 0, source.h - 1);
                for (int n = 0; n < 4; ++n) {
                    int sx = std::clamp(xInt + n - 1, 0, source.w - 1);
                    uint32_t pixel = source.pixels[sy * sourcePitch + sx];

                    float w = weightsX[n] * weightsY[m];
                    for (int c = 0; c < 4; ++c) {
                        sum[c] += getChannel(pixel, c) * w;
                    }
                }
            }

            uint32_t A = clampByte(sum[0]);
            uint32_t R = clampByte(sum[1]);
            uint32_t G = clampByte(sum[2]);
            uint32_t B = clampByte(sum[3]);

            pixels[dy * destPitch + dx] = (A << 24) | (R << 16) | (G << 8) | B;
        }
    }
}


void PixelBlock::CalcCropRect()
{
    int xmin = w - 1;
    int xmax = 0;
    int ymin = h - 1;
    int ymax = 0;
    int wpitch = pitch / 4;
    u32* base_p = pixels;
    for (int yy = 0; yy < h; yy++)
    {
        u32* p = base_p;
        for (int xx = 0; xx < w; xx++)
        {
            if (((*p++) & 0xff000000) != 0)
            {
                if (xx < xmin)
                    xmin = xx;
                if (xx > xmax)
                    xmax = xx;
                if (yy < ymin)
                    ymin = yy;
                if (yy > ymax)
                    ymax = yy;
            }
        }
        base_p += wpitch;
    }
    crop_x = xmin;
    crop_y = ymin;
    crop_w = xmax - xmin + 1;
    crop_h = ymax - ymin + 1;

    if (crop_w <= 0 || crop_h <= 0)
    {
        crop_x = 0;
        crop_y = 0;
        crop_w = 0;
        crop_h = 0;
    }
}

void PixelBlock::GenerateSDF(const PixelBlock& source, const PixelBlockDistanceFinder &sourceDF, int range)
{
    for (int yy = 0; yy < h; yy++)
    {
        int ysrc = source.crop_y - range + yy;
        for (int xx = 0; xx < w; xx++)
        {
            int xsrc = source.crop_x - range + xx;
            int dist = sourceDF.FindDistance(xsrc, ysrc, range);
            if (dist > -128 && dist < 128)
            {
                u32 nd = dist + 128;
                u32 out = nd << 24 | 0xffffff;
                pixels[yy * pitch / 4 + xx] = out;
            }
            else if (dist <= -127)
            {
                pixels[yy * pitch / 4 + xx] = 0x00ffffff;
            }
            else
            {
                pixels[yy * pitch / 4 + xx] = 0xffffffff;
            }
        }
    }
}

void PixelBlock::Dump()
{
    char* line = new char[w+1];
    line[w] = 0;
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            u8 val = (pixels[y * (pitch / 4) + x] & 0xff000000) >> 24;
            if (x >= crop_x && x < crop_x+crop_w && y >= crop_y && y < crop_y+crop_h)
                line[x] = val ? 'a'+(val/16) : '.';
            else
                line[x] = val ? 'A'+(val/16) : '*';
        }
        SDL_Log(line);
    }
}

void PixelBlockDistanceFinder::Generate(const PixelBlock& source)
{
    fullPitch = (source.w+63) / 64;

    pixelMaskFullRez = new u64[fullPitch * source.h];
    memset(pixelMaskFullRez, 0, fullPitch * source.h * 8);

    w = source.w;
    h = source.h;

    for (int y = 0; y < source.h; y++)
    {
        for (int x = 0; x < source.w; x++)
        {
            u8 val = source.pixels[y * source.pitch / 4 + x] >> 24;
            if (val >= 0x80)
            {
                pixelMaskFullRez[y * fullPitch + x / 64] |= (u64)1 << (x & 63);
            }
        }
    }
}

int PixelBlockDistanceFinder::FindDistance(int cx, int cy, int range) const
{
    bool pixOn = false;
    if (cx >= 0 && cx < w && cy >= 0 && cy < h)
    {
        int bit = cx & 63;
        int x = cx / 64;
        pixOn = pixelMaskFullRez[cy * fullPitch + x] & ((u64)1 << bit) ? true : false;
    }

    float fcx = (float)cx;
    float fcy = (float)cy;

    // find closest pixel OFF
    for (auto &check : g_posChecks)
    {
        int xo = cx + check.xo;
        int yo = cy + check.yo;
        if (xo < 0 || xo >= w || yo < 0 || yo >= h)
            continue;

        int bit = xo & 63;
        int x = xo / 64;
        bool localOn = pixelMaskFullRez[yo * fullPitch + x] & ((u64)1 << bit) ? true : false;
        if (pixOn != localOn)
        {
            return pixOn ? (int)(check.dist - 1.0f / 32.0f * 127.0f) : (int)-check.dist;
        }
    }

    return pixOn ? 128 : -128;
}

void PixelBlockDistanceFinder::Dump() const
{
    char* line = new char[w+1];
    line[w] = 0;

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            line[x] = pixelMaskFullRez[y * fullPitch + x / 64] & ((u64)1 << (x & 63)) ? 'X' : '.';
        }
        SDL_Log("%s", line);
    }
}


