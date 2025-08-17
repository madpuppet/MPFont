#pragma once

#include "SDL.h"
#include "SDL_Surface.h"
#include "PixelBlock.h"

#define SDFRange 32

// source char
struct FontChar
{
    u16 ch = 0;
    bool selected = false;
    bool preview = false;
    bool generated = false;

    SDL_Texture* texture = nullptr;             // preview texture  (32)
    SDL_Surface* surface = nullptr;             // small surface  (32)

    // glyph settings from 512 sized glyph
    int large_minx = 0;
    int large_maxx = 0;
    int large_miny = 0;
    int large_maxy = 0;
    int large_advance = 0;

    // Final SDF block needed by Atlas
    PixelBlock pb_scaledSDF;

    // final render data
    int scaledSize = 0;
    int x = 0;      // x location on page
    int y = 0;      // y location on page
    int w = 0;      // width on page
    int h = 0;      // height on page
    int page = 0;   // page number
    int xoffset = 0;    // offset from draw pos to bottom left render pos
    int yoffset = 0;    // offset from draw pos to bottom left render pos
    int advance = 0;    // how much to advance x pos after drawing this char
};
