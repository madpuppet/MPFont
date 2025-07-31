#pragma once

#include "types.h"
#include "Atlas.h"

class Shad;

class Project
{
public:
    Project(const std::string& name) : m_name(name) {}
    ~Project();

    void AskForFont(SDL_Renderer* renderer);
    void GenerateFont(SDL_Renderer* renderer);
    void LoadFromShad(const Shad& shad);
    void Save();
    bool Gui(SDL_Renderer* renderer);
    void GenerateSDF(SDL_Renderer* renderer);
    bool CloseRequested() { return !m_open; }

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
    };

    struct SDFChar
    {
        u16 ch = 0;
        SDL_Surface* surface = nullptr;
        int w = 0;
        int h = 0;
        int pitch = 0;
        int xoffset = 0;
        int yoffset = 0;
        int crop_x = 0;
        int crop_y = 0;
        int crop_w = 0;
        int crop_h = 0;
    };

    const std::string& Name() { return m_name; }
    const std::string& TTFName() { return m_ttf_name; }

private:
    Atlas m_atlas;
    std::string m_name;
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

    int m_sdfRange = 10;
    int m_fontSize = 16;
    int m_pageWidth = 512;
    int m_pageHeight = 512;
};
