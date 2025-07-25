#include "main.h"
#include <map>
#include "tinyfiledialogs.h"
#include "SHAD.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

void Project::LoadFromShad(const Shad& shad)
{
    auto roots = shad.GetRoots();
    m_ttf_name = "";
    for (auto r : roots)
    {
        m_name = r->GetString();
        for (auto c : r->children)
        {
            if (c->field == "font")
                m_ttf_name = c->GetString();
            else if (c->field == "chars")
            {
                for (auto ch : c->children)
                {
                    Project::Char new_ch;
                    new_ch.ch = ch->GetI32();
                    for (auto subch : ch->children)
                    {
                        if (subch->field == "selected")
                            new_ch.selected = subch->GetBool();
                    }
                    m_chars.push_back(new_ch);
                }
            }
        }
    }
}

Project::~Project()
{
    if (m_ttf_font)
        TTF_CloseFont(m_ttf_font);
    for (auto& ch : m_chars)
    {
        if (ch.texture)
            SDL_DestroyTexture(ch.texture);
    }
}

bool Project::Gui(SDL_Renderer* renderer)
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking;
    ImFont* font = ImGui::GetFont();
    ImGuiIO& io = ImGui::GetIO();

    if (ImGui::Begin(m_name.c_str(), &m_open, flags))
    {
        ImGui::Text("FONT");
        ImGui::SameLine();
        if (ImGui::Button(m_ttf_name.c_str()))
        {
            AskForFont(renderer);
            SaveSettings();
        }
        if (m_ttf_font)
        {
            float oldScale = font->Scale;
            font->Scale = 0.25f;

            const int size = 64;
            const int columns = 32;
            const int rows = (((int)m_chars.size() + (columns - 1)) / columns);
            const int rows_per_page = std::min(rows, 16);
            const int items_per_page = rows_per_page * columns;
            const int pages = (rows + (rows_per_page - 1)) / rows_per_page;

            ImVec2 panel_size = ImVec2((float)columns * size, (float)rows_per_page * size);
            ImGui::ColorButton("##panel", ImVec4(0.7f, 0.1f, 0.7f, 1.0f), ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, panel_size);
            ImVec2 panel_pos = ImGui::GetItemRectMin();
            ImVec2 panel_max = ImGui::GetItemRectMax();

            if (m_isSelecting)
            {
                if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_MouseLeft))
                {
                    if (io.MousePos.x >= panel_pos.x && io.MousePos.x < panel_max.x && io.MousePos.y >= panel_pos.y && io.MousePos.y < panel_max.y)
                    {
                        int mc = (int)(io.MousePos.x - panel_pos.x) / size;
                        int mr = (int)(io.MousePos.y - panel_pos.y) / size;
                        int idx = m_page * items_per_page + mr * columns + mc;
                        if (idx < (int)m_chars.size())
                        {
                            m_chars[idx].selected = m_isEnabling;
                        }
                    }
                }
                else
                {
                    m_isSelecting = false;
                }
            }
            else
            {
                if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey::ImGuiKey_MouseLeft, false))
                {
                    if (io.MousePos.x >= panel_pos.x && io.MousePos.x < panel_max.x && io.MousePos.y >= panel_pos.y && io.MousePos.y < panel_max.y)
                    {
                        int mc = (int)(io.MousePos.x - panel_pos.x) / size;
                        int mr = (int)(io.MousePos.y - panel_pos.y) / size;
                        int idx = m_page * items_per_page + mr * columns + mc;
                        if (idx < (int)m_chars.size())
                        {
                            m_isSelecting = true;
                            m_isEnabling = !m_chars[idx].selected;
                            m_startIdx = idx;
                            m_chars[idx].selected = m_isEnabling;
                        }
                    }
                }
            }

            if (ImGui::Button("Prev Page"))
            {
                m_page = std::max(0, m_page - 1);
            }
            ImGui::SameLine();
            if (ImGui::Button("Next Page"))
            {
                m_page = std::min(pages - 1, m_page + 1);
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear All"))
            {
                for (auto& item : m_chars)
                    item.selected = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Select All"))
            {
                for (auto& item : m_chars)
                    item.selected = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Generate SDF"))
            {
                GenerateSDF();
            }
            ImGui::SameLine();
            if (ImGui::MenuItem("Save Project"))
            {
                Save();
                SaveSettings();
            }

            ImVec4 colOffBG(0.1f, 0.1f, 0.1f, 1.0f);
            ImVec4 colOnBG(0.3f, 0.3f, 0.3f, 1.0f);
            ImVec4 colOffFG(0.5f, 0.5f, 0.5f, 1.0f);
            ImVec4 colOnFG(1.0f, 1.0f, 1.0f, 1.0f);

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            int startIdx = m_page * items_per_page;
            int endIdx = std::min(startIdx + items_per_page, (int)m_chars.size());
            for (int idx = startIdx; idx < endIdx; idx++)
            {
                auto& item = m_chars[idx];
                ImVec4& colBG = item.selected ? colOnBG : colOffBG;
                ImVec4& colFG = item.selected ? colOnFG : colOffFG;

                int col = idx % columns;
                int row = (idx - startIdx) / columns;

                ImVec2 posMin;
                ImVec2 posMax;
                posMin.x = panel_pos.x + col * size + 2;
                posMin.y = panel_pos.y + row * size + 2;
                posMax.x = posMin.x + size - 4;
                posMax.y = posMin.y + size - 4;

                ImU32 colBG32 = ImGui::GetColorU32(colBG);
                ImU32 colFG32 = ImGui::GetColorU32(colFG);
                draw_list->AddRectFilled(posMin, posMax, colBG32);

                char out[16];
                sprintf_s(out, " %04x", item.ch);
                draw_list->AddText(font, 16, posMin, colFG32, out);

                ImVec2 centre;
                centre.x = (posMin.x + posMax.x) / 2;
                centre.y = (posMin.y + posMax.y) / 2 + 4;

                ImVec2 imgPos;
                imgPos.x = centre.x - item.w / 2;
                imgPos.y = centre.y - item.h / 2 + 4;
                ImGui::SetCursorScreenPos(imgPos);
                ImGui::ImageWithBg((ImTextureID)item.texture, ImVec2((float)item.w, (float)item.h), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), colFG);
            }
            font->Scale = oldScale;
        }
    }
    ImGui::End();
    return m_open;
}

void Project::Save()
{
    const char* formats[] = { "*.vfnt" };
    char* filename = tinyfd_saveFileDialog("Load Project", m_name.c_str(), 1, formats, nullptr);
    std::ofstream out_file(filename);
    if (out_file.is_open())
    {
        m_name = filename;

        Shad shad;
        ShadNode* root = new ShadNode;
        root->field = "mpfont";
        root->values.push_back(m_name);
        shad.AddRoot(root);
        root->AddChild("font", std::format("\"{}\"", m_ttf_name));
        auto charsNode = root->AddChild("chars");
        for (auto& ch : m_chars)
        {
            auto charNode = charsNode->AddChild("char", ch.ch);
            charNode->AddChild("selected", ch.selected);
        }

        u32 size;
        char* mem;
        shad.Write(mem, size);

        out_file.write(mem, size);
        out_file.close();
    }
}

void Project::GenerateFont(SDL_Renderer* renderer)
{
    if (m_ttf_name.empty())
        return;

    std::map<int, bool> selected;
    for (auto ch : m_chars)
        selected[ch.ch] = ch.selected;

    TTF_Font* font = TTF_OpenFont(m_ttf_name.c_str(), 32);
    if (font)
    {
        if (m_ttf_font)
        {
            TTF_CloseFont(m_ttf_font);
            m_ttf_font = nullptr;
            for (auto item : m_chars)
            {
                SDL_DestroyTexture(item.texture);
            }
        }
        m_chars.clear();

        SDL_Color white = { 255, 255, 255, 255 };
        u16 text[2];
        text[1] = 0;
        for (u16 ch = 1; ch < 0xffff; ch++)
        {
            // create textures for each glyph in the font
            if (TTF_GlyphIsProvided(font, ch))
            {
                Project::Char item;
                item.ch = ch;
                auto it = selected.find(ch);
                item.selected = it != selected.end() ? it->second : true;

                SDL_Color white = { 255, 255, 255, 255 };
                text[0] = ch;
                SDL_Surface* surface = TTF_RenderUNICODE_Blended(font, text, white);
                item.texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_SetTextureBlendMode(item.texture, SDL_BLENDMODE_BLEND);
                SDL_FreeSurface(surface);
                SDL_QueryTexture(item.texture, NULL, NULL, &item.w, &item.h);
                m_chars.push_back(item);
            }
        }
        m_ttf_font = font;
    }
}

void Project::AskForFont(SDL_Renderer* renderer)
{
    const char* formats[] = { "*.ttf" };
    auto result = tinyfd_openFileDialog("Choose Font", "", 1, formats, nullptr, false);
    if (result)
    {
        m_ttf_name = result;
        GenerateFont(renderer);
    }
}

void Project::GenerateSDF()
{
}
