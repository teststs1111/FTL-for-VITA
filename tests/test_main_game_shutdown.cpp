#include "game/main_game.hpp"
#include "platform/input.hpp"
#include "platform/runtime_diagnostics.hpp"
#include "render/graphics.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

int main() {
    std::remove(wormhole::RuntimeDiagnostics::path().c_str());
    wormhole::RuntimeDiagnostics::startSession("test-main-game-shutdown", "missing-ftl.dat");

    wormhole::Graphics graphics;
    assert(graphics.init());
    wormhole::Input input;

    {
        wormhole::MainGame game;
        // A missing archive leaves a valid startup-error scene, which is
        // enough to exercise the initialized -> shutdown -> destructor path.
        game.init(graphics, input, "__missing_test_archive__.dat");

        // Exercise two diagnostic sampling points (frame 0 and frame 300)
        // through the same update/render wrappers used by the Vita main loop.
        for (unsigned int frame = 0; frame <= 300; ++frame) {
            graphics.beginFrame({0.f, 0.f, 0.f, 1.f});
            game.update(1.0f / 60.0f);
            game.render();
            graphics.endFrame();
        }

        game.shutdown();
        wormhole::RuntimeDiagnostics::markCleanShutdown();
    }

    graphics.shutdown();

    std::ifstream in(wormhole::RuntimeDiagnostics::path());
    assert(in.good());
    std::string line;
    std::string lastLine;
    int shutdownBeginCount = 0;
    bool foundCleanShutdown = false;
    std::vector<std::string> phaseMarkers;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        lastLine = line;
        const auto stagePos = line.find("stage=main_update_begin");
        if (stagePos != std::string::npos) phaseMarkers.push_back(line.substr(stagePos));
        const auto updateCompletePos = line.find("stage=main_update_complete");
        if (updateCompletePos != std::string::npos) phaseMarkers.push_back(line.substr(updateCompletePos));
        const auto renderBeginPos = line.find("stage=main_render_begin");
        if (renderBeginPos != std::string::npos) phaseMarkers.push_back(line.substr(renderBeginPos));
        const auto renderCompletePos = line.find("stage=main_render_complete");
        if (renderCompletePos != std::string::npos) phaseMarkers.push_back(line.substr(renderCompletePos));
        if (line.find("stage=main_game_shutdown_begin") != std::string::npos) {
            ++shutdownBeginCount;
            assert(!foundCleanShutdown);
        }
        if (line == "CLEAN_SHUTDOWN") foundCleanShutdown = true;
    }

    const std::vector<std::string> expectedPhaseMarkers = {
        "stage=main_update_begin frame=0",
        "stage=main_update_complete frame=0",
        "stage=main_render_begin frame=0",
        "stage=main_render_complete frame=0",
        "stage=main_update_begin frame=300",
        "stage=main_update_complete frame=300",
        "stage=main_render_begin frame=300",
        "stage=main_render_complete frame=300"
    };
    assert(phaseMarkers == expectedPhaseMarkers);
    assert(shutdownBeginCount == 1);
    assert(foundCleanShutdown);
    assert(lastLine == "CLEAN_SHUTDOWN");
    return 0;
}
