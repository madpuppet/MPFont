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
#include <set>
#include "FontChar.h"
#include "SDL_ttf.h"

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
            else if (c->field == "chars")
            {
                for (auto ch : c->children)
                {
                    FontChar new_ch;
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
    if (m_ttf_font_small)
    {
        TTF_CloseFont(m_ttf_font_small);
        TTF_CloseFont(m_ttf_font_large);
    }
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
        if (ImGui::SliderInt("Font Size", &m_fontSize, 8, 64))
        {
        }
        ImGui::SameLine(0, 100);
        if (ImGui::SliderInt("Line Height", &m_lineHeight, 8, 70))
        {
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

        if (m_ttf_font_small)
        {
            std::vector<u32> statusColors;
            statusColors.push_back(ImGui::GetColorU32(ImVec4(1.0f, 0, 0, 1)));         // Empty
            statusColors.push_back(ImGui::GetColorU32(ImVec4(0.0f, 0.5f, 0, 1)));      // GeneratedSource
            statusColors.push_back(ImGui::GetColorU32(ImVec4(0.0f, 0.5f, 1.0f, 1)));   // GeneratingLargeSDF
            statusColors.push_back(ImGui::GetColorU32(ImVec4(1.0f, 0.5f, 1.0f, 1)));   // GeneratedLargeSDF
            statusColors.push_back(ImGui::GetColorU32(ImVec4(1.0f, 0.0f, 1.0f, 1)));   // GeneratingScaledSDF
            statusColors.push_back(ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 0.0f, 1)));   // GeneratedScaledSDF

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

                    if (item.sourceState == FontChar::Empty)
                    {
                        GenerateCharItem(item, renderer);
                    }
                    else if (item.texture)
                    {
                        if (item.selected && item.sourceState == FontChar::GeneratedSource)
                            GenerateCharItem(item, renderer);

                        float aspectRatio = (float)item.surface->w / (float)item.surface->h;
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

                    ImVec4 rectCol(1.0f, 0.0f, 0.0f, 1.0f);
                    if (item.sourceState == FontChar::GeneratedScaledSDF)
                    {
                        rectCol = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
                    }

                    ImVec2 statusMin;
                    statusMin.x = posMax.x - 8.0f;
                    statusMin.y = posMax.y - 8.0f;
                    ImVec2 statusMax;
                    statusMax.x = posMax.x - 2.0f;
                    statusMax.y = posMax.y - 2.0f;
                    draw_list->AddRectFilled(statusMin, statusMax, statusColors[(int)item.sourceState]);
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

            ImGui::InputText("SAMPLE", m_sampleBuffer, 64);
            ImGui::ColorButton("##sample", ImVec4(0.1f, 0.2f, 0.3f, 1.0f), ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(2000.0f,96.0f));
            ImVec2 sample_min = ImGui::GetItemRectMin();
            ImVec2 sample_max = ImGui::GetItemRectMax();
            for (int chidx = 0; m_sampleBuffer[chidx]; chidx++)
            {
                char ch = m_sampleBuffer[chidx];
                auto it = std::find_if(m_chars.begin(), m_chars.end(), [ch](FontChar &fc)->bool {return fc.ch == (u16)ch; });
                if (it != m_chars.end())
                {
                    float scale = 16.0f / 96.0f;
                    if (it->sourceState == FontChar::Empty)
                    {
                        GenerateCharItem(*it, renderer);
                    }
                    else
                    {
                        ImDrawList* draw_list = ImGui::GetWindowDrawList();
                        ImVec2 imgMin(sample_min.x, sample_min.y);
                        ImVec2 imgMax(sample_min.x + it->largeSurface->w * scale, sample_min.y + it->largeSurface->h * scale);
                        draw_list->AddImage(it->texture, imgMin, imgMax, ImVec2(0, 0), ImVec2(1, 1), 0xffffffff);
                    }
                    sample_min.x += it->large_advance * scale;
                    sample_max.x += it->large_advance * scale;
                }
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
            auto charsNode = root->AddChild("chars", std::format("{}", m_chars.size()));
            for (auto &item : m_chars)
            {
                if (item.selected)
                {
                    auto charNode = charsNode->AddChild("char", std::format("{}", item.ch));
                    charNode->AddChild("pos", std::format("{},{}", item.x, item.y));
                    charNode->AddChild("size", std::format("{},{}", item.w, item.h));
                    charNode->AddChild("page", std::format("{}", item.page));
                    charNode->AddChild("offset", std::format("{},{}", item.xoffset, item.yoffset));
                    charNode->AddChild("advance", std::format("{}", item.advance));
                }
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

    AbortAsyncTasks();

    std::set<int> selected;
    for (auto &item : m_chars)
    {
        if (item.selected)
            selected.insert(item.ch);
    }

    TTF_Font* font_small = TTF_OpenFont(m_ttf_name.c_str(), 32);
    TTF_Font* font_large = TTF_OpenFont(m_ttf_name.c_str(), 512);
    if (font_small && font_large)
    {
        if (m_ttf_font_small)
        {
            TTF_CloseFont(m_ttf_font_small);
            TTF_CloseFont(m_ttf_font_large);
            m_ttf_font_small = nullptr;
            m_ttf_font_large = nullptr;

            for (auto item : m_chars)
            {
                SDL_DestroyTexture(item.texture);
            }
        }
        m_chars.clear();

        // just create all placeholders
        for (u16 ch = 1; ch < 0xffff; ch++)
        {
            if (TTF_GlyphIsProvided(font_small, ch))
            {
                FontChar item;
                item.ch = ch;
                item.selected = selected.contains(ch);
                m_chars.push_back(item);
            }
        }

        m_ttf_font_small = font_small;
        m_ttf_font_large = font_large;
    }
}

void Project::GenerateCharItem(FontChar &item, SDL_Renderer* renderer)
{
    auto func = [renderer, &item, fontSize = m_fontSize]()
    {
        // generate the SDF (512 size with 32 range SDF)
        // this only needs to be generated once by character
        // if the font size changes, we just scale this to a different size
        if (item.sourceState == FontChar::GeneratingLargeSDF)
        {
            item.pb_source.pixels = (u32*)item.largeSurface->pixels;
            item.pb_source.w = item.largeSurface->w;
            item.pb_source.h = item.largeSurface->h;
            item.pb_source.pitch = item.largeSurface->pitch;
            item.pb_source.CalcCropRect();
            item.pbdf_source.Generate(item.pb_source);
            item.pb_SDF.w = item.pb_source.crop_w + SDFRange*2;
            item.pb_SDF.h = item.pb_source.crop_h + SDFRange*2;
            item.pb_SDF.pitch = item.pb_SDF.w * 4;
            item.pb_SDF.pixels = new u32[item.pb_SDF.w * item.pb_SDF.h];

            item.pb_SDF.GenerateSDF(item.pb_source, item.pbdf_source, SDFRange);
            item.pb_SDF.CalcCropRect();
            item.sourceState = FontChar::GeneratedLargeSDF;
        }

        // generate scaled SDF if we haven't generated it for the current font size
        SDL_assert(item.sourceState == FontChar::GeneratedLargeSDF || item.sourceState == FontChar::GeneratedScaledSDF);

        if (item.sourceState != FontChar::GeneratingScaledSDF && (item.sourceState != FontChar::GeneratedScaledSDF || item.scaledSize != fontSize))
        {
            // scale the large SDF to fontsize
            float scalar = (float)fontSize / 512.0f;
            item.pb_scaledSDF.w = (int)(item.pb_SDF.w * scalar);
            item.pb_scaledSDF.h = (int)(item.pb_SDF.h * scalar);
            item.pb_scaledSDF.pixels = new u32[item.pb_scaledSDF.w * item.pb_scaledSDF.h];
            item.pb_scaledSDF.pitch = item.pb_scaledSDF.w * 4;
            item.pb_scaledSDF.BicubicScale(item.pb_SDF);
            item.pb_scaledSDF.CalcCropRect();
            item.scaledSize = fontSize;
            item.sourceState = FontChar::GeneratedScaledSDF;
        }
    };

    if (item.sourceState == FontChar::Empty)
    {
        SDL_Color white = { 255, 255, 255, 255 };
        item.surface = TTF_RenderGlyph_Blended(m_ttf_font_small, item.ch, white);
        if (item.surface)
        {
            item.texture = SDL_CreateTextureFromSurface(renderer, item.surface);
            item.largeSurface = TTF_RenderGlyph_Blended(m_ttf_font_large, item.ch, white);
            SDL_SetTextureBlendMode(item.texture, SDL_BLENDMODE_BLEND);
            SDL_LockSurface(item.surface);
            SDL_LockSurface(item.largeSurface);
            TTF_GlyphMetrics(m_ttf_font_large, item.ch, &item.large_minx, &item.large_maxx, &item.large_miny, &item.large_maxy, &item.large_advance);
        }

        // we'll be generating the large SDF next on thread
        item.sourceState = FontChar::GeneratedSource;
    }

    // crop the preview
    if (item.surface && item.selected)
    {
        if (item.sourceState == FontChar::GeneratedSource)
            item.sourceState = FontChar::GeneratingLargeSDF;
        QueueAsyncTaskHP(func);
    }
}

void Project::SetFont(const std::string& path, SDL_Renderer* renderer)
{
    AbortAsyncTasks();

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
    if (!m_ttf_font_large)
        return;

    // now wait for all tasks to finish
    WaitForAsyncTasks();

    // first make sure every selected character has a highrez font and an SDF
    for (auto& item : m_chars)
    {
        if (item.selected)
        {
            GenerateCharItem(item, renderer);
        }
    }

    // now wait for all tasks to finish
    WaitForAsyncTasks();

    // clears the atlas ready to build it again
    m_atlas.StartLayout(m_pageWidth, m_pageHeight, m_padding);

    for (auto& item : m_chars)
    {
        if (item.selected)
        {
            m_atlas.AddBlock(&item);
        }
    }

    m_atlas.FinishLayout();
}

