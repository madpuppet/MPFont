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
        ImGui::DragInt("Range", &m_sdfRange, 0.01f, 1, 10);

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
                auto task = [this, renderer]() { this->GenerateSDF(renderer); };
                QueueTask(task);
            }
            ImGui::SameLine();
            if (ImGui::MenuItem("Save Project"))
            {
                Save();
                SaveSettings();
            }

            const int sdf_rows = (((int)m_sdfChars.size() + (columns - 1)) / columns);
            const int sdf_rows_per_page = std::min(sdf_rows, 16);
            const int sdf_items_per_page = sdf_rows_per_page * columns;
            const int sdf_pages = sdf_items_per_page ? (sdf_rows + (sdf_rows_per_page - 1)) / sdf_rows_per_page : 0;
            ImVec2 sdf_panel_pos, sdf_panel_max;

            if (sdf_pages)
            {
                ImVec2 sdf_panel_size = ImVec2((float)columns * size, (float)sdf_rows_per_page * size);
                ImGui::ColorButton("##sdf", ImVec4(0.7f, 0.1f, 0.7f, 1.0f), ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, sdf_panel_size);
                sdf_panel_pos = ImGui::GetItemRectMin();
                sdf_panel_max = ImGui::GetItemRectMax();

                if (ImGui::Button("Prev Page"))
                {
                    m_sdfPage = std::max(0, m_sdfPage - 1);
                }
                ImGui::SameLine();
                if (ImGui::Button("Next Page"))
                {
                    m_sdfPage = std::min(sdf_pages - 1, m_sdfPage + 1);
                }
                ImGui::SameLine();
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

            if (sdf_pages)
            {
                int sdf_startIdx = m_sdfPage * sdf_items_per_page;
                int sdf_endIdx = std::min(sdf_startIdx + sdf_items_per_page, (int)m_sdfChars.size());
                for (int idx = sdf_startIdx; idx < sdf_endIdx; idx++)
                {
                    auto& item = m_sdfChars[idx];
                    ImVec4& colBG = colOnBG;
                    ImVec4& colFG = colOnFG;

                    int col = idx % columns;
                    int row = (idx - startIdx) / columns;

                    ImVec2 posMin;
                    ImVec2 posMax;
                    posMin.x = sdf_panel_pos.x + col * size + 2;
                    posMin.y = sdf_panel_pos.y + row * size + 2;
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

                    int useWidth = item.w / 2;
                    int useHeight = item.h / 2;

                    ImVec2 imgMin;
                    imgMin.x = centre.x - useWidth / 2;
                    imgMin.y = centre.y - useHeight / 2 + 4;
                    ImVec2 imgMax;
                    imgMax.x = imgMin.x + useWidth;
                    imgMax.y = imgMin.y + useHeight;
                    draw_list->AddImage(item.texture, imgMin, imgMax);
                }
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
                item.surface = TTF_RenderGlyph_Blended(font, ch, white);
                if (item.surface)
                {
                    item.texture = SDL_CreateTextureFromSurface(renderer, item.surface);
                    SDL_SetTextureBlendMode(item.texture, SDL_BLENDMODE_BLEND);
                    SDL_QueryTexture(item.texture, NULL, NULL, &item.w, &item.h);
                }
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

#if 1
u32 roundUp(u32 x) {
    if (x == 0) return 1;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}
#else
u32 roundUp(u32 x) {
    return (x + 31) & ~31;
}
#endif


void Project::GenerateSDF(SDL_Renderer *renderer)
{
    // delete old SDF textures
    for (auto& sdf : m_sdfChars)
    {
        if (sdf.texture)
            SDL_DestroyTexture(sdf.texture);
        if (sdf.surface)
            SDL_FreeSurface(sdf.surface);
    }
    m_sdfChars.clear();

    // generate SDF for each character
    if (m_ttf_font)
    {
        for (auto& item : m_chars)
        {
            if (item.selected && item.surface)
            {
                SDL_Color white = { 255, 255, 255, 255 };
                if (!SDL_LockSurface(item.surface))
                {
                    SDL_assert(item.surface->format->format == SDL_PIXELFORMAT_ARGB8888);

                    PixelBlock pb_source, pb_padded, pb_sdf;

                    // find crop rect
                    pb_source.pixels = (u32*)item.surface->pixels;
                    pb_source.pitch = item.surface->pitch;
                    pb_source.w = item.surface->w;
                    pb_source.h = item.surface->h;
                    pb_source.CalcCropRect();

                    pb_padded.w = pb_source.crop_w + m_sdfRange * 2;
                    pb_padded.h = pb_source.crop_h + m_sdfRange * 2;

                    SDFChar sdf;
                    sdf.ch = item.ch;
                    sdf.w = pb_padded.w;
                    sdf.h = pb_padded.h;
                    sdf.xoffset = pb_source.crop_x - m_sdfRange;
                    sdf.yoffset = pb_source.crop_y - m_sdfRange;
                    sdf.surface = SDL_CreateRGBSurfaceWithFormat(0, pb_padded.w, pb_padded.h, 1, SDL_PIXELFORMAT_ARGB8888);
                    sdf.pitch = sdf.surface->pitch;

                    SDL_LockSurface(sdf.surface);
                    int size = pb_padded.w * pb_padded.h;
                    pb_padded.pixels = (u32*)sdf.surface->pixels;
                    pb_padded.pitch = sdf.surface->pitch;
                    pb_padded.GenerateSDF(pb_source, m_sdfRange);
                    SDL_UnlockSurface(sdf.surface);
                    sdf.texture = SDL_CreateTextureFromSurface(renderer, sdf.surface);
                    SDL_SetTextureBlendMode(sdf.texture, SDL_BLENDMODE_BLEND);
                    SDL_LockSurface(sdf.surface);

                    m_sdfChars.push_back(sdf);
                }
            }
        }
    }
}


void PixelBlock::CalcCropRect()
{
    int xmin = 0;
    int xmax = w - 1;
    int ymin = 0;
    int ymax = h - 1;
    u32* p = pixels;
    for (int yy = 0; yy < h; yy++)
    {
        for (int xx = 0; xx < w; xx++)
        {
            if (*p++ != 0)
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
    }
    crop_x = xmin;
    crop_y = ymin;
    crop_w = xmax - xmin + 1;
    crop_h = ymax - ymin + 1;
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
                pixels[yy * pitch/4 + xx] = out;
            }
            else
            {
                pixels[yy + pitch/4 + xx] = 0;
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
        pixOn = (pixels[cy * pitch/4 + cx] & 0xff000000) != 0;

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
                localOn = (pixels[yd * pitch/4 + xd] & 0xff000000) != 0;
            }

            if (pixOn != localOn)
                dist = std::min(distXY, dist);
        }
    }

    return pixOn ? dist : -dist;
}

