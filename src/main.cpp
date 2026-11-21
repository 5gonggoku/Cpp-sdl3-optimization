#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <cfloat>
#include <cstdio>
#include <string>

#if defined(_DEBUG) && defined(_MSC_VER)
#include <crtdbg.h>
#endif

#include "bench/Bench.h"
#include "bench/CliOptions.h"
#include "data/DataManager.h"
#include "gameplay/World.h"
#include "render/Renderer.h"

namespace {

Vector2 ReadMoveDir(const bool* keys)
{
    Vector2 dir;
    if (keys[SDL_SCANCODE_D]) dir.x += 1.0f;
    if (keys[SDL_SCANCODE_A]) dir.x -= 1.0f;
    if (keys[SDL_SCANCODE_S]) dir.y += 1.0f;
    if (keys[SDL_SCANCODE_W]) dir.y -= 1.0f;
    return dir.LengthSq() > 0.0f ? dir.Normalized() : dir;
}

}

int main(int argc, char** argv)
{
#if defined(_DEBUG) && defined(_MSC_VER)

    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
#endif

    CliOptions options;
    if (!ParseCli(argc, argv, options)) return 1;
    options.dataDir = ResolveDataDir(options, argv[0]);
    if (options.bench || options.verify) return RunBench(options);

    DataManager data;
    std::string dataError;
    if (!data.Load(options.dataDir, dataError)) {
        std::fprintf(stderr, "데이터 로드 실패: %s\n", dataError.c_str());
        return 1;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init 실패: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("MyGame", static_cast<int>(World::kWidth),
                                           static_cast<int>(World::kHeight), 0);
    SDL_Renderer* sdlRenderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!window || !sdlRenderer) {
        SDL_Log("창/렌더러 생성 실패: %s", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(sdlRenderer, 0);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForSDLRenderer(window, sdlRenderer);
    ImGui_ImplSDLRenderer3_Init(sdlRenderer);

    World world(data, BuildWorldConfig(options));
    Renderer renderer(sdlRenderer);

    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    Uint64 prev = SDL_GetPerformanceCounter();

    constexpr double kFixedDt = 1.0 / 60.0;
    double accumulator = 0.0;
    bool running = true;

    constexpr int kFrameHistoryCount = 120;
    constexpr double kStatsSampleInterval = 0.5;
    float frameTimesMs[kFrameHistoryCount] = {};
    int frameTimeOffset = 0;

    double statsAccumulator = 0.0;
    double statsSumMs = 0.0;
    double statsMinMs = DBL_MAX;
    double statsMaxMs = 0.0;
    int statsFrameCount = 0;

    float displayFps = 0.0f;
    float displayAvgMs = 0.0f;
    float displayMinMs = 0.0f;
    float displayMaxMs = 0.0f;

    while (running) {
        const Uint64 now = SDL_GetPerformanceCounter();
        const double rawFrameTime = static_cast<double>(now - prev) / freq;
        prev = now;
        double frameTime = rawFrameTime;
        if (frameTime > 0.25) frameTime = 0.25;
        accumulator += frameTime;

        const double rawFrameTimeMs = rawFrameTime * 1000.0;
        statsAccumulator += rawFrameTime;
        statsSumMs += rawFrameTimeMs;
        statsMinMs = rawFrameTimeMs < statsMinMs ? rawFrameTimeMs : statsMinMs;
        statsMaxMs = rawFrameTimeMs > statsMaxMs ? rawFrameTimeMs : statsMaxMs;
        ++statsFrameCount;

        if (statsAccumulator >= kStatsSampleInterval && statsFrameCount > 0) {
            displayAvgMs = static_cast<float>(statsSumMs / statsFrameCount);
            displayMinMs = static_cast<float>(statsMinMs);
            displayMaxMs = static_cast<float>(statsMaxMs);
            displayFps = displayAvgMs > 0.0f ? 1000.0f / displayAvgMs : 0.0f;

            frameTimesMs[frameTimeOffset] = displayAvgMs;
            frameTimeOffset = (frameTimeOffset + 1) % kFrameHistoryCount;

            statsAccumulator = 0.0;
            statsSumMs = 0.0;
            statsMinMs = DBL_MAX;
            statsMaxMs = 0.0;
            statsFrameCount = 0;
        }

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT) running = false;
        }

        const bool* keys = SDL_GetKeyboardState(nullptr);
        const Vector2 moveDir = ReadMoveDir(keys);

        while (accumulator >= kFixedDt) {
            world.Update(static_cast<float>(kFixedDt), moveDir);
            accumulator -= kFixedDt;
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Debug");
        ImGui::Text("FPS: %.1f", displayFps);
        ImGui::Text("Frame Time avg/min/max (ms): %.3f / %.3f / %.3f (%.1fs 주기 갱신)",
                    displayAvgMs, displayMinMs, displayMaxMs, kStatsSampleInterval);
        ImGui::PlotLines("##FrameTimeGraph", frameTimesMs, kFrameHistoryCount, frameTimeOffset,
                          nullptr, FLT_MAX, FLT_MAX, ImVec2(0.0f, 60.0f));
        const TickStats& tick = world.GetLastTickStats();
        ImGui::Text("Tick (ms) total %.4f = move %.4f + collision %.4f + cleanup %.4f", tick.totalMs,
                    tick.moveMs, tick.collisionMs, tick.cleanupMs);
        ImGui::Text("Collision checks/tick: %llu  Enemies: %zu  Bullets: %zu",
                    static_cast<unsigned long long>(tick.collisionChecks), world.GetEnemies().size(),
                    world.GetBullets().size());
        ImGui::End();
        ImGui::Render();

        renderer.Draw(world);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), sdlRenderer);
        SDL_RenderPresent(sdlRenderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
