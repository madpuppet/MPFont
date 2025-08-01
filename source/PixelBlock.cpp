#include "PixelBlock.h"
#include "math.h"
#include "types.h"
#include "SDL.h"

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

    int dist = 128;
    for (int yo = -range; yo <= range; yo++)
    {
        int yd = cy + yo;
        for (int xo = -range; xo <= range; xo++)
        {
            int xd = cx + xo;

            int distX = abs(cx - xd);
            int distY = abs(cy - yd);
            int distXY = distX * distX + distY * distY;
            if (distXY > 0)
                distXY = (int)(sqrtf((float)distXY) * 127.0f / (float)range);

            bool localOn = false;
            if (yd >= crop_y && yd < crop_y2 && xd >= crop_x && xd < crop_x2)
            {
                localOn = (pixels[yd * pitch / 4 + xd] & 0xff000000) != 0;
            }

            if (pixOn != localOn)
                dist = std::min(distXY, dist);
        }
    }

    return pixOn ? dist : -dist;
}
