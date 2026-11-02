#include "Reader.h"
#include "cycode/Paths.h"
#include <SDL3/SDL_opengl.h>
#include <cyui/CyUI.h>
#include <fstream>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif
namespace {
std::vector<std::string> arguments(int argc,char **argv) {
    std::vector<std::string> result;
#ifdef _WIN32
    int count=0; auto values=CommandLineToArgvW(GetCommandLineW(),&count);
    for (int i=0; i<count; ++i) {
        const auto length=static_cast<int>(wcslen(values[i]));
        const auto bytes=WideCharToMultiByte(CP_UTF8,0,values[i],length,nullptr,0,nullptr,nullptr);
        std::string text(bytes,0); WideCharToMultiByte(CP_UTF8,0,values[i],length,text.data(),bytes,nullptr,nullptr); result.push_back(std::move(text));
    }
    LocalFree(values);
#else
    for (int i=0; i<argc; ++i) result.emplace_back(argv[i]);
#endif
    return result;
}
void saveCapture(const std::filesystem::path &path,int width,int height) {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    auto surface=SDL_CreateSurface(width,height,SDL_PIXELFORMAT_RGBA32);
    if (!surface) throw std::runtime_error(SDL_GetError());
    glPixelStorei(GL_PACK_ALIGNMENT,1); glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,surface->pixels);
    auto pixels=static_cast<unsigned char *>(surface->pixels);
    for (int y=0; y<height/2; ++y) std::swap_ranges(pixels+y*surface->pitch,pixels+(y+1)*surface->pitch,pixels+(height-1-y)*surface->pitch);
    const auto saved=SDL_SaveBMP(surface,cycode::pathUtf8(path).c_str()); SDL_DestroySurface(surface);
    if (!saved) throw std::runtime_error(SDL_GetError());
}
}
int main(int argc,char **argv) {
    using namespace cycode;
    SDL_Window *window=nullptr; SDL_GLContext context=nullptr;
    bool hidden=false, initialized=false; int frameLimit=0;
    std::filesystem::path installation,capture,report;
    reader::Json script=reader::Json::array();
    try {
        reader::Options options;
        const auto args=arguments(argc,argv);
        for (size_t i=1; i<args.size(); ++i) {
            auto next=[&]() -> std::string { if (i+1>=args.size()) throw std::runtime_error("Missing command line value"); return args[++i]; };
            if (args[i]=="--theme") options.theme=next();
            else if (args[i]=="--language") options.language=next();
            else if (args[i]=="--open") options.files.push_back(utf8Path(next()));
            else if (args[i]=="--test-frames") { frameLimit=std::stoi(next()); if (frameLimit<=0 || frameLimit>10000) throw std::runtime_error("Invalid test frame limit"); }
            else if (args[i]=="--test-hidden") hidden=true;
            else if (args[i]=="--test-script") script=reader::Json::parse(readTextFileBytes(utf8Path(next())));
            else if (args[i]=="--test-report") report=utf8Path(next());
            else if (args[i]=="--capture") capture=utf8Path(next());
            else if (!args[i].starts_with('-')) options.files.push_back(utf8Path(args[i]));
            else throw std::runtime_error("Unknown CyCodeReader option");
        }
        options.automated=frameLimit>0;
        if ((hidden || !script.empty() || !report.empty() || !capture.empty()) && !options.automated) throw std::runtime_error("Reader capture/test options require --test-frames");
        if (!script.is_array()) throw std::runtime_error("Reader test script must be an array");
        if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
        auto base=utf8Path(SDL_GetBasePath());
        for (int depth=0; depth<6; ++depth) { if (std::filesystem::exists(base/"locales/ui.json")) { installation=base; break; } base=base.parent_path(); }
        if (installation.empty()) throw std::runtime_error("Cannot locate locales/ui.json beside CyCodeReader");
        options.installation=installation;
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3); SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
#ifdef __APPLE__
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
        const char *glsl="#version 150";
#else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3); const char *glsl="#version 330 core";
#endif
        window=SDL_CreateWindow("CyCodeReader",1280,820,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY|(hidden ? SDL_WINDOW_HIDDEN : 0));
        if (!window) throw std::runtime_error(SDL_GetError());
        context=SDL_GL_CreateContext(window); if (!context) throw std::runtime_error(SDL_GetError());
        SDL_GL_MakeCurrent(window,context); SDL_GL_SetSwapInterval(options.automated ? 0 : 1);
        IMGUI_CHECKVERSION(); ImGui::CreateContext();
        auto &io=ImGui::GetIO(); io.IniFilename=nullptr;
        io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_DockingEnable|ImGuiConfigFlags_ViewportsEnable;
        io.ConfigViewportsNoAutoMerge=true; io.ConfigViewportsNoTaskBarIcon=false; io.ConfigViewportsNoDefaultParent=true;
        io.ConfigDpiScaleFonts=true; io.ConfigDpiScaleViewports=true;
        ImGui_ImplSDL3_InitForOpenGL(window,context);
        if (!ImGui_ImplOpenGL3_Init(glsl)) throw std::runtime_error("OpenGL renderer initialization failed");
        initialized=true;
        // SDL's backend creates platform windows hidden. Suppress its only show
        // callback during tests, retaining real native multi-window creation.
        if (hidden) { ImGui::GetPlatformIO().Platform_ShowWindow=[](ImGuiViewport *) {}; ImGui::GetPlatformIO().Platform_SetWindowFocus=[](ImGuiViewport *) {}; }
        {
            reader::Reader app(std::move(options),window);
            for (int frame=0; !app.shouldQuit(); ++frame) {
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    if (frameLimit && (event.type==SDL_EVENT_KEY_DOWN || event.type==SDL_EVENT_KEY_UP || event.type==SDL_EVENT_TEXT_INPUT)) continue;
                    ImGui_ImplSDL3_ProcessEvent(&event);
                    if (event.type==SDL_EVENT_QUIT || (event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID==SDL_GetWindowID(window))) app.requestQuit();
                    if (event.type==SDL_EVENT_DROP_FILE && event.drop.data) app.open(utf8Path(event.drop.data));
                }
                ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL3_NewFrame();
                for (const auto &action : script) if (action.at("frame")==frame) {
                    if (action.at("command")=="keys") for (const auto &value : action.at("keys")) {
                        const auto key=value.get<std::string>(); const auto down=action.value("down",true);
                        if (key=="Ctrl") { io.AddKeyEvent(ImGuiMod_Ctrl,down); io.AddKeyEvent(ImGuiKey_LeftCtrl,down); }
                        else if (key=="Shift") { io.AddKeyEvent(ImGuiMod_Shift,down); io.AddKeyEvent(ImGuiKey_LeftShift,down); }
                        else if (key=="Escape") io.AddKeyEvent(ImGuiKey_Escape,down);
                        else if (key=="Enter") io.AddKeyEvent(ImGuiKey_Enter,down);
                        else if (key.size()==1 && key[0]>='A' && key[0]<='Z') io.AddKeyEvent(static_cast<ImGuiKey>(ImGuiKey_A+key[0]-'A'),down);
                    }
                    else if (action.at("command")=="text") io.AddInputCharactersUTF8(action.at("text").get<std::string>().c_str());
                }
                ImGui::NewFrame();
                for (const auto &action : script) if (action.at("frame")==frame && action.at("command")!="keys" && action.at("command")!="text") app.testAction(action);
                app.frame(); ImGui::Render(); SDL_GL_MakeCurrent(window,context);
                int width=0,height=0; SDL_GetWindowSizeInPixels(window,&width,&height); glViewport(0,0,width,height);
                const auto ground=cyui::vec(cyui::palette().bg0); glClearColor(ground.x,ground.y,ground.z,1); glClear(GL_COLOR_BUFFER_BIT);
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                ImGui::UpdatePlatformWindows(); ImGui::RenderPlatformWindowsDefault(); SDL_GL_MakeCurrent(window,context);
                if (frameLimit && frame+1>=frameLimit) {
                    auto state=app.state(); int count=0; bool allHidden=true;
                    auto windows=SDL_GetWindows(&count); for (int i=0; i<count; ++i) allHidden&=bool(SDL_GetWindowFlags(windows[i])&SDL_WINDOW_HIDDEN); SDL_free(windows);
                    state["nativeWindows"]=count; state["allWindowsHidden"]=allHidden;
                    if (!report.empty()) { if (!report.parent_path().empty()) std::filesystem::create_directories(report.parent_path()); std::ofstream out(report); out<<state.dump(2); if (!out) throw std::runtime_error("Cannot write Reader test report"); }
                    if (!capture.empty()) saveCapture(capture,width,height);
                    break;
                }
                SDL_GL_SwapWindow(window); SDL_Delay(frameLimit ? 4 : 20);
            }
        }
        ImGui::DestroyPlatformWindows(); ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL3_Shutdown(); ImGui::DestroyContext();
        SDL_GL_DestroyContext(context); SDL_DestroyWindow(window); SDL_Quit(); return 0;
    } catch (const std::exception &exception) {
        const auto path=(installation.empty() ? std::filesystem::current_path() : installation)/"data/reports/reader-startup-error.txt";
        std::error_code ec; std::filesystem::create_directories(path.parent_path(),ec); std::ofstream(path)<<exception.what();
        if (!frameLimit && !hidden) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"CyCodeReader",exception.what(),window);
        if (initialized) { ImGui::DestroyPlatformWindows(); ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL3_Shutdown(); ImGui::DestroyContext(); }
        if (context) SDL_GL_DestroyContext(context); if (window) SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }
}
