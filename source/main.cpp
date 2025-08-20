// Dear ImGui: standalone example application for SDL3 + SDL_Renderer
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// Important to understand: SDL_Renderer is an _optional_ component of SDL3.
// For a multi-platform app consider using e.g. SDL+DirectX on Windows and SDL+OpenGL on Linux/OSX.

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <stdio.h>
#include <SDL3/SDL.h>

#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

#include <mutex>
#include <thread>
#ifdef _WIN32
#include <windows.h>        // SetProcessDPIAware()
#endif
#include "tinyfiledialogs.h"
#include "Settings.h"
#include "SHAD.h"
#include "WorkerFarm.h"
#include <filesystem>
#include <fstream>
#include <iostream>


std::vector<Project*> g_projects;
int g_selectedProject = -1;

struct SelectState
{
    Project* project = nullptr;
    bool isSelecting = false;
    int startIdx = -1;
    bool isEnabling = false;
} g_selectState;

std::vector<GenericTask> g_tasks;
std::mutex g_tasks_access;
void QueueMainThreadTask(const GenericTask& func)
{
    g_tasks_access.lock();
    g_tasks.push_back(func);
    g_tasks_access.unlock();
}

WorkerFarm gWorkers;
void QueueAsyncTaskLP(const GenericTask& func)
{
    gWorkers.QueueLowPriorityTask(func);
}
void QueueAsyncTaskHP(const GenericTask& func)
{
    gWorkers.QueueHighPriorityTask(func);
}
int GetAsyncTasksRemaining()
{
    return gWorkers.TasksRemaining();
}


void WaitForAsyncTasks()
{
    gWorkers.WaitForTasks();
}
void AbortAsyncTasks()
{
    gWorkers.Abort();
    gWorkers.WaitForTasks();
}

// new project
void NewProject(SDL_Renderer* renderer)
{
    const char* formats[] = { "*.ttf" };
    char* filename = tinyfd_openFileDialog("Load Project from Font", "", 1, formats, nullptr, false);
    if (filename)
    {
        std::filesystem::path path = filename;
        std::string projectPath = path.stem().string() + ".mpfnt";
        auto project = new Project(projectPath);
        g_projects.push_back(project);
        project->SetFont(filename, renderer);
    }
}

// load project
void LoadProject(const std::string& name, SDL_Renderer* renderer)
{
    // check project isn't already loaded...
    std::filesystem::path path = name;
    std::string nameNoExt = path.stem().filename().string();
    for (auto proj : g_projects)
        if (proj->Name() == nameNoExt)
            return;

    std::ifstream file(name, std::ios::binary | std::ios::ate);
    if (file.is_open())
    {
        int size = (int)file.tellg();
        file.seekg(0);
        char* mem = new char[size];

        file.read(mem, size);
        if (!(file.flags() & std::ifstream::failbit))
        {
            Shad shad;
            shad.Parse(mem, size);
            auto project = new Project(name);
            project->LoadFromShad(shad);
            project->GenerateFont(renderer);
            g_projects.push_back(project);
        }
    }
}

void LoadProject(SDL_Renderer* renderer)
{
    const char* formats[] = { "*.mpfnt" };
    char* filename = tinyfd_openFileDialog("Load Project", "", 1, formats, nullptr, false);
    if (filename)
    {
        LoadProject(filename, renderer);
    }
}

void SaveSettings()
{
    std::vector<std::string> projectList;
    for (auto project : g_projects)
        projectList.push_back(project->Path());
    gSettings.SetProjects(projectList);
    gSettings.Save();
}



// Main code
int main(int, char**)
{
    // Setup SDL
    // [If using SDL_MAIN_USE_CALLBACKS: all code below until the main loop starts would likely be your SDL_AppInit() function]
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return -1;
    }

    gSettings.Load();
    InitPosCheckArray();


    // Create window with SDL_Renderer graphics context
    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    int windowX = gSettings.GetInt("WindowX", SDL_WINDOWPOS_CENTERED);
    int windowY = gSettings.GetInt("WindowY", SDL_WINDOWPOS_CENTERED);
    int windowWidth = gSettings.GetInt("WindowWidth", (int)(1280 * main_scale));
    int windowHeight = gSettings.GetInt("WindowHeight", (int)(720 * main_scale));
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* window = SDL_CreateWindow("Vahl Font Editor V1.0", windowWidth, windowHeight, window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }
    SDL_SetWindowPosition(window, windowX, windowY);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    SDL_SetRenderVSync(renderer, 1);
    if (renderer == nullptr)
    {
        SDL_Log("Error: SDL_CreateRenderer(): %s\n", SDL_GetError());
        return -1;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    TTF_Init();

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    //style.FontSizeBase = 20.0f;
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);

    // Our state
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    auto projects = gSettings.GetProjects();
    for (auto& proj : projects)
        LoadProject(proj, renderer);

    // Main loop
    bool done = false;
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!done)
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        // [If using SDL_MAIN_USE_CALLBACKS: call ImGui_ImplSDL3_ProcessEvent() from your SDL_AppEvent() function]
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            switch (event.type)
            {
                case SDL_EVENT_QUIT:
                    done = true;
                    break;
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    if (event.window.windowID == SDL_GetWindowID(window))
                        done = true;
                    break;

                case SDL_EVENT_WINDOW_MOVED:
                    gSettings.Set("WindowX", (int)event.window.data1);
                    gSettings.Set("WindowY", (int)event.window.data2);
                    SaveSettings();
                    break;

                case SDL_EVENT_WINDOW_RESIZED:
                    gSettings.Set("WindowWidth", (int)event.window.data1);
                    gSettings.Set("WindowHeight", (int)event.window.data2);
                    SaveSettings();
                    break;
            }
        }

        // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppIterate() function]
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        // close any finished projects
        std::vector<Project*> closeProjects;
        for (auto project : g_projects)
        {
            if (project->CloseRequested())
                closeProjects.push_back(project);
        }
        if (closeProjects.size() > 0)
        {
            for (auto project : closeProjects)
            {
                g_projects.erase(std::remove(g_projects.begin(), g_projects.end(), project), g_projects.end());
                delete project;
            }
            SaveSettings();
        }

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // setup font scaling for this frame
        ImFont* font = ImGui::GetFont();
        font->Scale = gSettings.GetFloat("FontScale", 1.0f);

        // present UI
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Menu"))
            {
                if (ImGui::MenuItem("New Project", "CTRL+N"))
                {
                    QueueMainThreadTask([renderer]() { NewProject(renderer); });
                }
                if (ImGui::MenuItem("Load Project", "CTRL+L"))
                {
                    QueueMainThreadTask([renderer]() { LoadProject(renderer); SaveSettings(); });
                }
                ImFont* font = ImGui::GetFont();
                if (ImGui::DragFloat("Font scale", &font->Scale, 0.005f, 0.3f, 2.0f, "%.1f"))
                {
                    gSettings.Set("FontScale", font->Scale);
                    SaveSettings();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        for (auto project : g_projects)
            project->Gui(renderer);

        // Rendering
        ImGui::Render();
        SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColorFloat(renderer, clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);

        // next frame tasks
        g_tasks_access.lock();
        std::vector<GenericTask> tasks = std::move(g_tasks);
        g_tasks_access.unlock();

        for (auto& task : tasks)
        {
            task();
        }
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppQuit() function]
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

