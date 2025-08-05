#pragma once

#include "types.h"
#include "Atlas.h"

class Shad;

class Project
{
public:
    Project(const std::string& path);
    ~Project();

    void AskForFont(SDL_Renderer* renderer);
    void SetFont(const std::string& path, SDL_Renderer* renderer);
    void GenerateFont(SDL_Renderer* renderer);
    void LoadFromShad(const Shad& shad);
    void Save();
    void SaveAs();
    bool Gui(SDL_Renderer* renderer);
    void GenerateSDF(SDL_Renderer* renderer);
    bool CloseRequested() { return !m_open; }
    void Export();

    // source char
    struct Char
    {
        u16 ch = 0;
        bool selected = false;
        SDL_Texture* texture = nullptr;
        SDL_Surface* surface = nullptr;
        int w = 0;
        int h = 0;
        int crop_x = 0;
        int crop_y = 0;
        int crop_w = 0;
        int crop_h = 0;

        int minx = 0;       // from glyph
        int maxx = 0;
        int miny = 0;
        int maxy = 0;
        int advance = 0;
    };

    // sdf char
    struct SDFChar
    {
        u16 ch = 0;
        SDL_Surface* surface = nullptr;
        int pitch = 0;  // pitch of surface
        int x = 0;      // x location on page
        int y = 0;      // y location on page
        int w = 0;      // width on page
        int h = 0;      // height on page
        int page = 0;   // page number

        int xoffset = 0;    // offset from draw pos to bottom left render pos
        int yoffset = 0;    // offset from draw pos to bottom left render pos
        int advance = 0;    // how much to advance x pos after drawing this char
    };

    const std::string& Name() { return m_name; }
    const std::string& Path() { return m_path; }
    const std::string& TTFName() { return m_ttf_name; }

private:
    Atlas m_atlas;
    std::string m_name;
    std::string m_path;
    std::string m_ttf_name;
    TTF_Font* m_ttf_font = nullptr;
    std::vector<Char> m_chars;
    std::vector<SDFChar> m_sdfChars;
    bool m_open = true;
    int m_page = 0;
    int m_sdfPage = 0;
    bool m_isSelecting = false;
    int m_startIdx = -1;
    bool m_isEnabling = false;
    bool m_isCharsFolded = true;

    float m_sdf_zoom_x = 0.0f;
    float m_sdf_zoom_y = 0.0f;
    int m_sdf_zoom = 500;

    bool m_foldChars = false;

    int m_sdfRange = 10;
    int m_fontSize = 16;
    int m_pageWidth = 512;
    int m_pageHeight = 512;
    int m_lineHeight = 16;
    int m_padding = 2;
};
