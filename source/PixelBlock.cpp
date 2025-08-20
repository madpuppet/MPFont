#include "PixelBlock.h"
#include "math.h"
#include "types.h"
#include "SDL3/SDL.h"

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

#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>


#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

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
            {
                g_posChecks.emplace_back(xo, yo, dist);
            }
        }
    }

    std::sort(g_posChecks.begin(), g_posChecks.end(), [](const PosCheck& a, const PosCheck& b)->bool { return a.dist < b.dist; });
}

void PixelBlock::ScaleCropped(const PixelBlock& source)
{
    if (source.crop_w == 0 || source.crop_h == 0)
        return;

    // scale source crop_x..crop_x+crop_w, crop_y..crop_y+crop_w  =>  w,h
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            int x1 = x * source.crop_w / w + source.crop_x;
            int x2 = (x + 1) * source.crop_w / w + source.crop_x;
            int y1 = y * source.crop_h / h + source.crop_y;
            int y2 = (y + 1) * source.crop_h / h + source.crop_y;
            u32 accumA = 0;
            u32 accumR = 0;
            u32 accumG = 0;
            u32 accumB = 0;
            for (int yy = y1; yy < y2; yy++)
            {
                for (int xx = x1; xx < x2; xx++)
                {
                    u32 val = source.pixels[yy * source.pitch / 4 + xx];
                    accumA += val >> 24;
                    accumR += (val >> 16) & 0xff;
                    accumG += (val >> 8) & 0xff;
                    accumB += val & 0xff;
                }
            }
            int area = (x2 - x1) * (y2 - y1);
            accumA /= area;
            accumR /= area;
            accumG /= area;
            accumB /= area;
            pixels[y * pitch / 4 + x] = (accumA << 24) | (accumR << 16) | (accumG << 8) | (accumB);
        }
    }
}

void PixelBlock::Scale(const PixelBlock& source)
{
    if (source.w == 0 || source.h == 0)
        return;

    // scale source crop_x..crop_x+crop_w, crop_y..crop_y+crop_w  =>  w,h
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            int x1 = x * source.w / w;
            int x2 = (x + 1) * source.w / w;
            int y1 = y * source.h / h;
            int y2 = (y + 1) * source.h / h;
            u32 accumA = 0;
            u32 accumR = 0;
            u32 accumG = 0;
            u32 accumB = 0;
            for (int yy = y1; yy < y2; yy++)
            {
                for (int xx = x1; xx < x2; xx++)
                {
                    u32 val = source.pixels[yy * source.pitch / 4 + xx];
                    accumA += val >> 24;
                    accumR += (val >> 16) & 0xff;
                    accumG += (val >> 8) & 0xff;
                    accumB += val & 0xff;
                }
            }
            int area = (x2 - x1) * (y2 - y1);
            accumA /= area;
            accumR /= area;
            accumG /= area;
            accumB /= area;
            pixels[y * pitch / 4 + x] = (accumA << 24) | (accumR << 16) | (accumG << 8) | (accumB);
        }
    }
}

void PixelBlock::CopyCropped(const PixelBlock& source, int x, int y)
{
    for (int yy = 0; yy < source.crop_h; yy++)
    {
        int sy = source.crop_y + yy;
        int dy = y + yy;
        if (dy < 0 || dy >= h)
            continue;
        for (int xx = 0; xx < source.crop_w; xx++)
        {
            int sx = source.crop_x + xx;
            int dx = x + xx;
            if (dx < 0 || dx >= w)
                continue;
            pixels[dy * pitch / 4 + dx] = source.pixels[sy * source.pitch / 4 + sx];
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
    int miny = std::max(source.crop_y - range, 0);
    int maxy = std::min(source.crop_y + source.crop_h, h);
    int minx = std::max(source.crop_x - range, 0);
    int maxx = std::max(source.crop_x + source.crop_w, w);

    for (int yy = miny; yy < maxy; yy++)
    {
        int ysrc = yy;
        for (int xx = minx; xx < maxx; xx++)
        {
            int xsrc = xx;
            int distX, distY;
            int dist = sourceDF.FindDistance(xsrc, ysrc, range, distX, distY);
            u32 nx = xx * 255 / (w - 1);
            u32 ny = yy * 255 / (h - 1);
            if (dist > -128 && dist < 128)
            {
                u32 nd = dist + 128;
                u32 out = nd << 24 | nx << 16 | ny << 8 | 0xff;
                pixels[yy * pitch / 4 + xx] = out;
            }
            else if (dist <= -127)
            {
                pixels[yy * pitch / 4 + xx] = nx << 16 | ny << 8 | 0xff;
            }
            else
            {
                pixels[yy * pitch / 4 + xx] = 0xff000000 | nx << 16 | ny << 8 | 0xff;
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
            line[x] = val > 0 ? 'A'+val/30 : '.';
        }
        SDL_Log("%03d:%s",y,line);
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

int PixelBlockDistanceFinder::FindDistance(int cx, int cy, int range, int &distX, int &distY) const
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
            distX = check.xo * 127 / 32;
            distY = check.yo * 127 / 32;
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


