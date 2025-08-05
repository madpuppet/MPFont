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
#include <stdio.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

Project::Project(const std::string& path)
{
    m_path = path;

    std::filesystem::path fpath = path;
    m_name = fpath.stem().filename().string();
}

void Project::LoadFromShad(const Shad& shad)
{
    auto roots = shad.GetRoots();
    for (auto r : roots)
    {
        for (auto c : r->children)
        {
            if (c->field == "font")
                m_ttf_name = c->GetString();
            else if (c->field == "charsFolded")
                m_isCharsFolded = c->GetBool();
            else if (c->field == "fontSize")
                m_fontSize = c->GetI32();
            else if (c->field == "lineHeight")
                m_lineHeight = c->GetI32();
            else if (c->field == "pagewidth")
                m_pageWidth = c->GetI32();
            else if (c->field == "pageheight")
                m_pageHeight = c->GetI32();
            else if (c->field == "padding")
                m_padding = c->GetI32();
            else if (c->field == "sdfRange")
                m_sdfRange = c->GetI32();
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
    m_atlas.SetRenderer(renderer);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar;
    ImFont* font = ImGui::GetFont();
    ImGuiIO& io = ImGui::GetIO();

    if (ImGui::Begin(m_name.c_str(), &m_open, flags))
    {
        char name_buffer[64];
        strncpy_s(name_buffer, m_name.c_str(), std::min((int)m_name.size(), 63));
        name_buffer[63] = 0;
        if (ImGui::InputText(m_name.c_str(), name_buffer, 64, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            m_name = name_buffer;
            std::filesystem::path path = m_path;
            std::filesystem::path newFilename = m_name + ".mpfnt";
            m_path = path.replace_filename(newFilename).string();
            SaveSettings();
        }
        ImGui::PushID("font name");
        if (ImGui::Button(m_ttf_name.c_str()))
        {
            AskForFont(renderer);
            Save();
        }
        ImGui::SameLine();
        ImGui::Text("FONT");

        ImGui::PopID();
        ImGui::SameLine(0,100);
        if (ImGui::Button("Save Project"))
        {
            SaveAs();
        }

        ImGui::PushItemWidth(300.0f);
        if (ImGui::SliderInt("SDF Range", &m_sdfRange, 0, 32))
        {
        }
        ImGui::SameLine(0,100);
        if (ImGui::SliderInt("Font Size", &m_fontSize, 8, 512))
        {
        }
        ImGui::SameLine(0, 100);
        if (ImGui::SliderInt("Line Height", &m_lineHeight, 8, 70))
        {
        }
        ImGui::SameLine(0, 100);
        if (ImGui::Button("GenerateFont"))
        {
            GenerateFont(renderer);
        }
        ImGui::SameLine(0, 100);
        if (ImGui::Button("GenerateSDF"))
        {
            GenerateSDF(renderer);
        }

        if (ImGui::InputInt("Page Width", &m_pageWidth, 64))
        {
        }
        ImGui::SameLine(0, 100);
        if (ImGui::InputInt("Page Height", &m_pageHeight, 64))
        {
        }
        ImGui::SameLine(0, 100);
        if (ImGui::InputInt("Padding", &m_padding, 32))
        {
        }

        if (m_atlas.Pages().size() > 0)
        {
            ImGui::SameLine(0, 100);
            if (ImGui::Button("Export"))
            {
                Export();
                SaveSettings();
            }
        }

        ImGui::PopItemWidth();

        if (m_ttf_font)
        {
            float oldScale = font->Scale;
            font->Scale = 0.25f;

            const float window_width = ImGui::GetWindowWidth();

            const int size = 64;
            const int columns = std::max((int)(window_width / size - 1), 8);
            const int rows = (((int)m_chars.size() + (columns - 1)) / columns);
            const int rows_per_page = std::min(rows, 16);
            const int items_per_page = rows_per_page * columns;
            const int pages = (rows + (rows_per_page - 1)) / rows_per_page;

            if (ImGui::CollapsingHeader("Selected Chars", m_isCharsFolded ? 0 : ImGuiTreeNodeFlags_DefaultOpen))
            {
                m_isCharsFolded = false;

                ImVec2 panel_size = ImVec2((float)columns * size, (float)rows_per_page * size);
                ImGui::ColorButton("##chars", ImVec4(0.7f, 0.1f, 0.7f, 1.0f), ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, panel_size);
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

                    ImVec2 areaCentre((posMin.x + posMax.x) / 2, (posMin.y + 15 + posMax.y) / 2);
                    float areaHalfSize = (size - 30) / 2;
                    ImVec2 areaMin(areaCentre.x - areaHalfSize, areaCentre.y - areaHalfSize);
                    ImVec2 areaMax(areaCentre.x + areaHalfSize, areaCentre.y + areaHalfSize);

                    ImVec2 centre;
                    centre.x = (areaMin.x + areaMax.x) / 2;
                    centre.y = (areaMin.y + areaMax.y) / 2 + 4;

                    if (item.h > 0)
                    {
                        float aspectRatio = (float)item.w / (float)item.h;
                        int useWidth, useHeight;
                        if (aspectRatio > 1.0f)
                        {
                            useWidth = (int)(areaMax.x - areaMin.x);
                            useHeight = (int)((areaMax.y - areaMin.y) / aspectRatio);
                        }
                        else
                        {
                            useHeight = (int)(areaMax.y - areaMin.y);
                            useWidth = (int)((areaMax.x - areaMin.x) * aspectRatio);
                        }

                        ImVec2 imgMin;
                        imgMin.x = centre.x - useWidth / 2;
                        imgMin.y = centre.y - useHeight / 2;
                        ImVec2 imgMax;
                        imgMax.x = imgMin.x + useWidth;
                        imgMax.y = imgMin.y + useHeight;
                        draw_list->AddImage(item.texture, imgMin, imgMax, ImVec2(0, 0), ImVec2(1, 1), colFG32);
                    }
                }
                if (ImGui::Button("Prev Chars"))
                {
                    m_page = std::max(0, m_page - 1);
                }
                ImGui::SameLine();
                if (ImGui::Button("Next Chars"))
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
            }
            else
            {
                m_isCharsFolded = true;
            }

            if (m_atlas.Pages().size() > 0)
            {
                const float window_width = ImGui::GetWindowWidth();
                m_sdfPage = std::min((int)m_atlas.Pages().size() - 1, m_sdfPage);
                auto& page = m_atlas.Pages()[m_sdfPage];
                ImGui::Image(page.m_texture, ImVec2((float)m_pageWidth, (float)m_pageHeight));
                ImVec2 sdf_pos = ImGui::GetItemRectMin();
                ImVec2 sdf_max = ImGui::GetItemRectMax();

                if (ImGui::IsWindowFocused() && ImGui::IsKeyDown(ImGuiKey::ImGuiKey_MouseRight))
                {
                    float xnorm = ((float)io.MousePos.x - sdf_pos.x) / (float)(sdf_max.x - sdf_pos.x);
                    float ynorm = ((float)io.MousePos.y - sdf_pos.y) / (float)(sdf_max.y - sdf_pos.y);
                    if (xnorm >= 0 && xnorm <= 1.0f && ynorm >= 0 && ynorm <= 1.0f)
                    {
                        m_sdf_zoom_x = xnorm;
                        m_sdf_zoom_y = ynorm;
                    }
                }

                float d = 25.0f / (float)m_sdf_zoom;
                float u1 = m_sdf_zoom_x - d;
                float u2 = m_sdf_zoom_x + d;
                float v1 = m_sdf_zoom_y - d;
                float v2 = m_sdf_zoom_y + d;
                if (u1 < 0)
                {
                    u2 -= u1;
                    u1 = 0.0f;
                }
                else if (u2 > 1.0f)
                {
                    u1 -= (u2 - 1.0f);
                    u2 = 1.0f;
                }
                if (v1 < 0)
                {
                    v2 -= v1;
                    v1 = 0.0f;
                }
                else if (v2 > 1.0f)
                {
                    v1 -= (v2 - 1.0f);
                    v2 = 1.0f;
                }
                ImGui::SameLine();
                ImGui::Image(page.m_texture, ImVec2(512.0f, 512.0f), ImVec2(u1, v1), ImVec2(u2, v2));
            }
            if (m_atlas.Pages().size() > 1)
            {
                if (ImGui::Button("Prev Page"))
                {
                    if (m_atlas.Pages().size() > 1)
                    {
                        m_sdfPage = std::max(m_sdfPage - 1, 0);
                    }
                    SaveSettings();
                }
                ImGui::SameLine();
                if (ImGui::Button("Next Page"))
                {
                    if (m_atlas.Pages().size() > 1)
                    {
                        m_sdfPage = std::min(m_sdfPage + 1, (int)m_atlas.Pages().size() - 1);
                    }
                    SaveSettings();
                }
                ImGui::SameLine();
            }
            ImGui::PushItemWidth(300.0f);
            if (ImGui::SliderInt("Zoom", &m_sdf_zoom, 200, 10000))
            {
                SaveSettings();
            }
            ImGui::PopItemWidth();

            font->Scale = oldScale;
        }
    }
    ImGui::End();
    return m_open;
}

// Custom write function to append data to a vector
static void write_to_memory(void* context, void* data, int size) {
    std::vector<uint8_t>* buffer = static_cast<std::vector<uint8_t>*>(context);
    buffer->insert(buffer->end(), static_cast<uint8_t*>(data), static_cast<uint8_t*>(data) + size);
}

void Project::Export()
{
    const char* formats[] = { "*.fnt" };
    std::filesystem::path filepath = m_name;
    std::string export_path = (filepath.parent_path() / filepath.stem()).string() + ".fnt";
    char* filename = tinyfd_saveFileDialog("Export", export_path.c_str(), 1, formats, nullptr);
    if (filename)
    {
        std::ofstream out_file(filename);
        if (out_file.is_open())
        {
            Shad shad;
            ShadNode* root = new ShadNode;
            root->field = "font";
            root->values.push_back(filepath.filename().string());
            shad.AddRoot(root);

            root->AddChild("pages", std::format("{}", m_atlas.Pages().size()));
            root->AddChild("pageWidth", std::format("{}", m_pageWidth));
            root->AddChild("pageHeight", std::format("{}", m_pageHeight));
            root->AddChild("fontSize", std::format("{}", m_fontSize));
            root->AddChild("lineHeight", std::format("{}", m_lineHeight));
            auto charsNode = root->AddChild("chars", std::format("{}", m_sdfChars.size()));
            for (auto item : m_sdfChars)
            {
                auto charNode = charsNode->AddChild("char", std::format("{}", item.ch));
                charNode->AddChild("pos", std::format("{},{}", item.x, item.y));
                charNode->AddChild("size", std::format("{},{}", item.w, item.h));
                charNode->AddChild("page", std::format("{}", item.page));
                charNode->AddChild("offset", std::format("{},{}", item.xoffset, item.yoffset));
                charNode->AddChild("advance", std::format("{}", item.advance));
            }

            u32 size;
            char* mem;
            shad.Write(mem, size);

            out_file.write(mem, size);
            out_file.close();
        }

        for (int p=0; p<m_atlas.Pages().size(); p++)
        {
            auto& page = m_atlas.Pages()[p];
            u32* data = new u32[page.m_surface->w * page.m_surface->h];
            u32* src = (u32*)page.m_surface->pixels;
            u32* out = data;
            for (int y = 0; y < page.m_surface->h; y++)
            {
                for (int x = 0; x < page.m_surface->w; x++)
                {
                    u32 value = src[x];
                    *out++ = value;
                }
                src += page.m_surface->pitch/4;
            }

            std::vector<uint8_t> png_buffer;
            stbi_write_png_to_func(write_to_memory, &png_buffer, page.m_surface->w, page.m_surface->h, 4, data, page.m_surface->w*4);

            std::filesystem::path export_filepath = filename;
            std::string export_path = (export_filepath.parent_path() / export_filepath.stem()).string() + std::format("_page{}.png", p);
            std::ofstream out_file(export_path, std::ios::binary);
            if (out_file.is_open())
            {
                out_file.write((const char *)png_buffer.data(), png_buffer.size());
                out_file.close();
            }
        }
    }
}

void Project::Save()
{
    std::ofstream out_file(m_path);
    if (out_file.is_open())
    {
        std::filesystem::path path = m_path;
        m_name = path.filename().string();

        Shad shad;
        ShadNode* root = new ShadNode;
        root->field = "mpfnt";
        shad.AddRoot(root);
        root->AddChild("font", m_ttf_name);
        root->AddChild("charsFolded", std::format("{}", m_isCharsFolded));
        root->AddChild("fontSize", std::format("{}", m_fontSize));
        root->AddChild("pagewidth", std::format("{}", m_pageWidth));
        root->AddChild("pageheight", std::format("{}", m_pageHeight));
        root->AddChild("padding", std::format("{}", m_padding));
        root->AddChild("sdfRange", std::format("{}", m_sdfRange));
        root->AddChild("zoom", std::format("{}", m_sdf_zoom));

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
void Project::SaveAs()
{
    const char* formats[] = { "*.mpfnt" };
    char* filename = tinyfd_saveFileDialog("Save Project", m_path.c_str(), 1, formats, nullptr);
    if (filename)
    {
        m_path = filename;
        Save();
    }
}

void Project::GenerateFont(SDL_Renderer* renderer)
{
    if (m_ttf_name.empty())
        return;

    std::map<int, bool> selected;
    for (auto ch : m_chars)
        selected[ch.ch] = ch.selected;

    TTF_Font* font = TTF_OpenFont(m_ttf_name.c_str(), m_fontSize);
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
                extern DECLSPEC int SDLCALL TTF_GlyphMetrics(TTF_Font * font, Uint16 ch,
                    int* minx, int* maxx,
                    int* miny, int* maxy, int* advance);


                Project::Char item;
                item.ch = ch;
                auto it = selected.find(ch);
                item.selected = it != selected.end() ? it->second : true;
                TTF_GlyphMetrics(font, ch, &item.minx, &item.maxx, &item.miny, &item.maxy, &item.advance);

                SDL_Color white = { 255, 255, 255, 255 };
                item.surface = TTF_RenderGlyph_Blended(font, ch, white);
                if (item.surface)
                {
                    item.texture = SDL_CreateTextureFromSurface(renderer, item.surface);
                    SDL_assert(item.surface->format->format == SDL_PIXELFORMAT_ARGB8888);

                    SDL_SetTextureBlendMode(item.texture, SDL_BLENDMODE_BLEND);
                    SDL_QueryTexture(item.texture, NULL, NULL, &item.w, &item.h);
                    SDL_LockSurface(item.surface);

                    PixelBlock pb;
                    pb.pixels = (u32*)item.surface->pixels;
                    pb.w = item.surface->w;
                    pb.h = item.surface->h;
                    pb.pitch = item.surface->pitch;
                    pb.CalcCropRect();
                    item.crop_x = pb.crop_x;
                    item.crop_y = pb.crop_y;
                    item.crop_w = pb.crop_w;
                    item.crop_h = pb.crop_h;
                }
                m_chars.push_back(item);
            }
        }
        m_ttf_font = font;
    }
}

void Project::SetFont(const std::string& path, SDL_Renderer* renderer)
{
    m_ttf_name = path;
    GenerateFont(renderer);
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


void Project::GenerateSDF(SDL_Renderer* renderer)
{
    // generate SDF for each character
    if (m_ttf_font)
    {
        // clears the atlas ready to build it again
        m_atlas.StartLayout(m_pageWidth, m_pageHeight, m_padding);

        // delete old SDF textures
        for (auto& sdf : m_sdfChars)
        {
            if (sdf.surface)
                SDL_FreeSurface(sdf.surface);
        }
        m_sdfChars.clear();

        for (auto& item : m_chars)
        {
            if (item.selected && item.surface)
            {
                SDFChar sdf;
                sdf.ch = item.ch;
                sdf.w = item.crop_w + m_sdfRange * 2;
                sdf.h = item.crop_h + m_sdfRange * 2;
                sdf.xoffset = item.crop_x - m_sdfRange + item.minx;
                sdf.yoffset = item.crop_y - m_sdfRange + item.miny;
                sdf.advance = item.advance;

                sdf.surface = SDL_CreateRGBSurfaceWithFormat(0, sdf.w, sdf.h, 1, SDL_PIXELFORMAT_ARGB8888);
                sdf.pitch = sdf.surface->pitch;
                SDL_LockSurface(sdf.surface);
                m_sdfChars.push_back(sdf);

                auto work = [this, sdf, item]()
                    {
                        PixelBlock pb_source;
                        pb_source.pixels = (u32*)item.surface->pixels;
                        pb_source.w = item.surface->w;
                        pb_source.h = item.surface->h;
                        pb_source.pitch = item.surface->pitch;
                        pb_source.crop_x = item.crop_x;
                        pb_source.crop_y = item.crop_y;
                        pb_source.crop_w = item.crop_w;
                        pb_source.crop_h = item.crop_h;

                        PixelBlock pb_sdf;
                        pb_sdf.pixels = (u32*)sdf.surface->pixels;
                        pb_sdf.w = sdf.surface->w;
                        pb_sdf.h = sdf.surface->h;
                        pb_sdf.pitch = sdf.surface->pitch;
                        pb_sdf.GenerateSDF(pb_source, m_sdfRange);
                        pb_sdf.CalcCropRect();
                        m_atlas.AddBlock(item.ch, pb_sdf);
                    };

                QueueAsyncTask(work);
            }
        }

        WaitForAsyncTasks();

        std::vector<LayoutPos> posList;
        m_atlas.FinishLayout(posList);

        for (auto& p : posList)
        {
            int ch = p.ch;
            auto pred = [ch](const SDFChar& item)->bool { return ch == item.ch; };
            auto it = std::find_if(m_sdfChars.begin(), m_sdfChars.end(), pred);
            SDL_assert(it != m_sdfChars.end());
            it->x = p.x;
            it->y = p.y;
        }
    }
}

