#pragma once

#include "types.h"

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
    void GenerateSDF();

    struct Char
    {
        u16 ch = 0;
        bool selected = false;
        SDL_Texture* texture = nullptr;
        int w = 0;
        int h = 0;
    };

    const std::string& Name() { return m_name; }
    const std::string& TTFName() { return m_ttf_name; }

private:
    std::string m_name;
    std::string m_ttf_name;
    TTF_Font* m_ttf_font = nullptr;
    std::vector<Char> m_chars;
    bool m_open = true;
    int m_page = 0;
    bool m_isSelecting = false;
    int m_startIdx = -1;
    bool m_isEnabling = false;
};
