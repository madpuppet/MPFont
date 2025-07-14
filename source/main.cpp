// Dear ImGui: standalone example application for SDL2 + SDL_Renderer
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// Important to understand: SDL_Renderer is an _optional_ component of SDL2.
// For a multi-platform app consider using e.g. SDL+DirectX on Windows and SDL+OpenGL on Linux/OSX.

#include "main.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <stdio.h>
#include <SDL.h>
#include <mutex>
#include <thread>
#ifdef _WIN32
#include <windows.h>        // SetProcessDPIAware()
#endif
#include "tinyfiledialogs.h"
#include "Settings.h"

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

std::mutex g_projectMutex;
std::mutex g_mainThreadMutex;
std::vector<Project*> g_projects;
std::vector<Callback> g_mainThreadTasks;
int g_selectedProject = -1;

struct SelectState
{
    bool isSelecting = false;
    int startIdx = -1;
    bool isEnabling = false;
} g_selectState;


// new project
void NewProject()
{
    auto task = []()
        {
            char* name = tinyfd_inputBox("Name of Font Project", "Enter name for font project", "my_font");
            if (name)
            {
                auto project = new Project;
                project->name = name;
                project->ttf_name = "<select ttf/otf>";
                g_projectMutex.lock();
                g_projects.push_back(project);
                g_projectMutex.unlock();
            }
        };

    std::thread async(task);
    async.detach();
}

// load project
void LoadProject()
{
    auto task = []() 
    {
        const char* formats[] = { "*.vfnt" };
        char* filename = tinyfd_openFileDialog("Load Project", "", 1, formats, nullptr, false);
        if (filename)
        {
            auto project = new Project;
            project->name = filename;
            project->ttf_name = "unknown";
            g_projectMutex.lock();
            g_projects.push_back(project);
            g_projectMutex.unlock();
        }
    };

    std::thread async(task);
    async.detach();
}

void SaveProject()
{
    auto task = []()
        {
            const char* formats[] = { "*.vfnt" };
            char* filename = tinyfd_saveFileDialog("Load Project", "", 1, formats, nullptr);
            g_projectMutex.lock();
            // copy current project
            g_projectMutex.unlock();
            // save current project to disk
        };

    std::thread async(task);
    async.detach();
}

void SelectFont(Project* project, SDL_Renderer *renderer)
{
    auto task = [project, renderer]()
        {
            const char* formats[] = { "*.ttf" };
            char* filename = tinyfd_openFileDialog("Choose Font", "", 1, formats, nullptr, false);
            if (filename)
            {
                TTF_Font* font = TTF_OpenFont(filename, 32);
                if (font)
                {
                    if (project->ttf_font)
                    {
                        project->internalMutex.lock();
                        TTF_CloseFont(project->ttf_font);
                        project->ttf_font = nullptr;
                        for (auto item : project->chars)
                        {
                            SDL_DestroyTexture(item.texture);
                        }
                        project->chars.clear();
                        project->internalMutex.unlock();
                    }

                    std::vector<Project::Char> chars;
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
                            item.selected = true;

                            SDL_Color white = { 255, 255, 255, 255 };
                            text[0] = ch;
                            SDL_Surface* surface = TTF_RenderUNICODE_Blended(font, text, white);
                            item.texture = SDL_CreateTextureFromSurface(renderer, surface);
                            SDL_SetTextureBlendMode(item.texture, SDL_BLENDMODE_BLEND);
                            SDL_FreeSurface(surface);
                            SDL_QueryTexture(item.texture, NULL, NULL, &item.w, &item.h);
                            chars.push_back(item);
                        }
                    }

                    project->internalMutex.lock();
                    project->ttf_font = font;
                    project->ttf_name = filename;
                    project->chars = std::move(chars);
                    project->internalMutex.unlock();
                }
            }
        };

    // have to just do them synchronous atm coz renderer can't be used async...
    task();
//    std::thread async(task);
//    async.detach();
}

void AddMainTask(const Callback& func)
{
    g_mainThreadMutex.lock();
    g_mainThreadTasks.push_back(func);
    g_mainThreadMutex.unlock();
}

// Main code
int main(int, char**)
{
    bool showDemo = false;
    gSettings.Load();

    // Setup SDL
#ifdef _WIN32
    ::SetProcessDPIAware();
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    // Create window with SDL_Renderer graphics context
    float main_scale = ImGui_ImplSDL2_GetContentScaleForDisplay(0);
    int windowX = gSettings.GetInt("WindowX", SDL_WINDOWPOS_CENTERED);
    int windowY = gSettings.GetInt("WindowY", SDL_WINDOWPOS_CENTERED);
    int windowWidth = gSettings.GetInt("WindowWidth", (int)(1280 * main_scale));
    int windowHeight = gSettings.GetInt("WindowHeight", (int)(720 * main_scale));
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+SDL_Renderer example", windowX, windowY, windowWidth, windowHeight, window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr)
    {
        SDL_Log("Error creating SDL_Renderer!");
        return -1;
    }
    //SDL_RendererInfo info;
    //SDL_GetRendererInfo(renderer, &info);
    //SDL_Log("Current SDL_Renderer: %s", info.name);
    TTF_Init();

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)
    io.ConfigDpiScaleFonts = true;          // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
    io.ConfigDpiScaleViewports = true;      // [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //style.FontSizeBase = 20.0f;
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            switch (event.type)
            {
            case SDL_QUIT:
                done = true;
                break;

                case SDL_WINDOWEVENT:
                {
                    switch (event.window.event)
                    {
                    case SDL_WINDOWEVENT_MOVED:
                        {
                            gSettings.Set("WindowX", (int)event.window.data1);
                            gSettings.Set("WindowY", (int)event.window.data2);
                            gSettings.Save();
                        }
                        break;
                    case SDL_WINDOWEVENT_RESIZED:
                        {
                            gSettings.Set("WindowWidth", (int)event.window.data1);
                            gSettings.Set("WindowHeight", (int)event.window.data2);
                            gSettings.Save();
                    }
                        break;
                    case SDL_WINDOWEVENT_CLOSE:
                        if (event.window.windowID == SDL_GetWindowID(window))
                        {
                            done = true;
                        }
                        break;
                    }
                }
                break;
            }
        }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImFont* font = ImGui::GetFont();
        font->Scale = gSettings.GetFloat("FontScale", 1.0f);

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Menu"))
            {
                if (ImGui::MenuItem("New Project", "CTRL+N"))
                {
                    NewProject();
                }
                if (ImGui::MenuItem("Load Project", "CTRL+L"))
                {
                    LoadProject();
                }
                if (ImGui::MenuItem("Save Project", "CTRL+S"))
                {
                    SaveProject();
                }

                ImFont* font = ImGui::GetFont();
                if (ImGui::DragFloat("Font scale", &font->Scale, 0.005f, 0.3f, 2.0f, "%.1f"))
                {
                    gSettings.Set("FontScale", font->Scale);
                    gSettings.Save();
                }

                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;// ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;

        for (auto project : g_projects)
        {
            if (ImGui::Begin(project->name.c_str(), &project->open, flags))
            {
                ImGui::Text("FONT");
                ImGui::SameLine();
                if (ImGui::Button(project->ttf_name.c_str()))
                {
                    SelectFont(project, renderer);
                }
                if (project->ttf_font)
                {
                    float oldScale = font->Scale;
                    font->Scale = 0.25f;

                    const int size = 64;
                    const int columns = 32;
                    const int rows = (((int)project->chars.size() + (columns - 1)) / columns);
                    const int rows_per_page = min(rows, 16);
                    const int items_per_page = rows_per_page * columns;
                    const int pages = (rows + (rows_per_page-1)) / rows_per_page;

                    ImVec2 panel_size = ImVec2((float)columns*size, (float)rows_per_page*size);
                    ImGui::ColorButton("##panel", ImVec4(0.7f, 0.1f, 0.7f, 1.0f), ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, panel_size);
                    ImVec2 panel_pos = ImGui::GetItemRectMin();
                    ImVec2 panel_max = ImGui::GetItemRectMax();

                    if (g_selectState.isSelecting)
                    {
                        if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_MouseLeft))
                        {
                            if (io.MousePos.x >= panel_pos.x && io.MousePos.x < panel_max.x && io.MousePos.y >= panel_pos.y && io.MousePos.y < panel_max.y)
                            {
                                int mc = (int)(io.MousePos.x - panel_pos.x) / size;
                                int mr = (int)(io.MousePos.y - panel_pos.y) / size;
                                int idx = project->page * items_per_page + mr * columns + mc;
                                if (idx < (int)project->chars.size())
                                {
                                    project->chars[idx].selected = g_selectState.isEnabling;
                                }
                            }
                        }
                        else
                        {
                            g_selectState.isSelecting = false;
                        }
                    }
                    else
                    {
                        if (ImGui::IsKeyPressed(ImGuiKey::ImGuiKey_MouseLeft))
                        {
                            if (io.MousePos.x >= panel_pos.x && io.MousePos.x < panel_max.x && io.MousePos.y >= panel_pos.y && io.MousePos.y < panel_max.y)
                            {
                                int mc = (int)(io.MousePos.x - panel_pos.x) / size;
                                int mr = (int)(io.MousePos.y - panel_pos.y) / size;
                                int idx = project->page * items_per_page + mr * columns + mc;
                                if (idx < (int)project->chars.size())
                                {
                                    g_selectState.isSelecting = true;
                                    g_selectState.isEnabling = !project->chars[idx].selected;
                                    g_selectState.startIdx = idx;
                                    project->chars[idx].selected = g_selectState.isEnabling;
                                }
                            }
                        }
                    }

                    if (ImGui::Button("Prev Page"))
                    {
                        project->page = max(0, project->page - 1);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Next Page"))
                    {
                        project->page = min(pages - 1, project->page + 1);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Clear All"))
                    {
                        for (auto& item : project->chars)
                            item.selected = false;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Select All"))
                    {
                        for (auto& item : project->chars)
                            item.selected = true;
                    }

                    ImVec4 colOffBG(0.1f, 0.1f, 0.1f, 1.0f);
                    ImVec4 colOnBG(0.3f, 0.3f, 0.3f, 1.0f);
                    ImVec4 colOffFG(0.5f, 0.5f, 0.5f, 1.0f);
                    ImVec4 colOnFG(1.0f, 1.0f, 1.0f, 1.0f);

                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    int startIdx = project->page * items_per_page;
                    int endIdx = min(startIdx + items_per_page, (int)project->chars.size());
                    for (int idx = startIdx; idx < endIdx; idx++)
                    {
                        auto& item = project->chars[idx];
                        ImVec4& colBG = item.selected ? colOnBG : colOffBG;
                        ImVec4& colFG = item.selected ? colOnFG : colOffFG;

                        int col = idx % columns;
                        int row = (idx-startIdx) / columns;

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

#if 0
                    ImGui::BeginChild("scrolling", scrolling_child_size, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
                    int idx = 0;
                    ImVec4 offColBase(0.1f, 0.1f, 0.1f, 1.0f);
                    ImVec4 offColHovered(0.2f, 0.2f, 0.2f, 1.0f);
                    ImVec4 offColActive(0.3f, 0.3f, 0.3f, 1.0f);
                    ImVec4 onColBase(0.2f, 0.2f, 0.0f, 1.0f);
                    ImVec4 onColHovered(0.3f, 0.3f, 0.1f, 1.0f);
                    ImVec4 onColActive(0.3f, 0.3f, 0.3f, 1.0f);
                    for (auto& item : project->chars)
                    {
                        if (idx % columns != 0) ImGui::SameLine();
                        char out[16];
                        sprintf_s(out, " %04x", item.ch);

                        if (item.selected)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Button, onColBase);
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, onColHovered);
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, onColActive);
                        }
                        else
                        {
                            ImGui::PushStyleColor(ImGuiCol_Button, offColBase);
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, offColHovered);
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, offColActive);
                        }

                        auto oldPos = ImGui::GetCursorPos();

                        ImGui::PushID(item.ch);
                        if (ImGui::Button("##custom_button", ImVec2(size, size)))
                            item.selected = !item.selected;
                        ImGui::PopID();

                        ImVec2 button_pos = ImGui::GetItemRectMin();
                        ImGui::SetCursorScreenPos(button_pos);
                        ImGui::Text(out);

                        button_pos.x += 12.0f;
                        button_pos.y = ImGui::GetItemRectMax().y + 6.0f;
                        ImGui::SetCursorScreenPos(button_pos);

                        ImVec4 imageBGCol, imageFGCol;
                        if (item.selected)
                        {
                            imageFGCol = ImVec4(1, 1, 1, 1);
                        }
                        else
                        {
                            imageFGCol = ImVec4(0.5f, 0.5f, 0.5f, 1);
                        }

                        ImGui::ImageWithBg((ImTextureID)item.texture, ImVec2(item.w, item.h), ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), imageFGCol);
//                        ImGui::Image((ImTextureID)item.texture, ImVec2(item.w,item.h));

                        ImGui::SetCursorPos(oldPos);
                        ImGui::InvisibleButton("##custom_button", ImVec2(64, 64));

                        ImGui::PopStyleColor(3);
                        idx++;
                    }
                    ImGui::EndChild();
#endif

                    font->Scale = oldScale;
                }




            }
            ImGui::End();
        }


        if (show_demo_window)
        {
            ImGui::ShowDemoWindow(&show_demo_window);
        }

#if 0


        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }
#endif

        // Rendering
        ImGui::Render();
        SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
