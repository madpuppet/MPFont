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
            if (((*p++) & 0xff) != 0)
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

void PixelBlock::GenerateSDF(const PixelBlock& source, int range)
{
    for (int yy = 0; yy < h; yy++)
    {
        int ysrc = crop_y - range + yy;
        for (int xx = 0; xx < w; xx++)
        {
            int xsrc = source.crop_x - range + xx;
            int dist = source.FindDistance(xsrc, ysrc, range);
            if (dist > -128 && dist < 128)
            {
                u8 nd = (u8)(dist + 128);
                u32 out = 0xff000000 | nd | (nd << 8) | (nd << 16);
                pixels[yy * pitch / 4 + xx] = out;
            }
            else if (dist <= -127)
            {
                pixels[yy * pitch / 4 + xx] = 0xff000000;
            }
            else
            {
                pixels[yy * pitch / 4 + xx] = 0xffffffff;
            }
        }
    }
}

int PixelBlock::FindDistance(int cx, int cy, int range) const
{
    int crop_x2 = crop_x + crop_w;
    int crop_y2 = crop_y + crop_h;

    bool pixOn = false;
    if (cx >= crop_x && cx < crop_x2 && cy >= crop_y && cy < crop_y2)
        pixOn = (pixels[cy * pitch / 4 + cx] & 0xff000000) != 0;

    float fcx = (float)cx;
    float fcy = (float)cy;

    int dist = 128;
    for (int yo = -range; yo <= range; yo++)
    {
        int yd = cy + yo;
        for (int xo = -range; xo <= range; xo++)
        {
            int xd = cx + xo;
            float fx = (float)xd;
            float fy = (float)yd;
            float dfx = fx - fcx;
            float dfy = fy - fcy;
            float distXY_pixels = sqrtf(dfx * dfx + dfy * dfy);
            if (pixOn)
                distXY_pixels -= 1.0f;

            float distXY = range ? distXY_pixels / (float)range * 127.0f : 127.0f;

            bool localOn = false;
            if (yd >= crop_y && yd < crop_y2 && xd >= crop_x && xd < crop_x2)
            {
                localOn = (pixels[yd * pitch / 4 + xd] & 0xff000000) != 0;
            }

            if (pixOn != localOn)
                dist = std::min((int)distXY, dist);
        }
    }

    return pixOn ? dist : -dist;
}
