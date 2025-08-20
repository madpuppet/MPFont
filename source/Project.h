#pragma once

#include "types.h"
#include "Atlas.h"
#include "FontChar.h"

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
    void GenerateCharSDF(FontChar& item);
    bool CloseRequested() { return !m_open; }
    void Export();
    void SetRenderer(SDL_Renderer* renderer);
    void GenerateCharItem(FontChar& item, SDL_Renderer* renderer);

    const std::string& Name() { return m_name; }
    const std::string& Path() { return m_path; }
    const std::string& TTFName() { return m_ttf_name; }

private:
    char m_sampleBuffer[64]{ 0 };

    Atlas m_atlas;
    std::string m_name;
    std::string m_path;
    std::string m_ttf_name;

    TTF_Font* m_ttf_font_small = nullptr;       // 32 point font for preview
    TTF_Font* m_ttf_font_large = nullptr;       // 512 point font for SDF generation

    std::vector<FontChar> m_chars;
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
    bool m_applySDF = false;

    int m_fontSize = 16;
    int m_pageWidth = 512;
    int m_pageHeight = 512;
    int m_linePadding = 2;
    int m_padding = 2;

    std::mutex m_ttf_access;
    bool m_generatingSDF = false;
    bool m_finishedGeneratingSDF = false;
    std::thread* m_generateSDFTask = nullptr;
};
