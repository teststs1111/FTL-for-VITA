#include "game/main_game.hpp"
#include "platform/input.hpp"
#include "platform/runtime_diagnostics.hpp"
#include "render/graphics.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>

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
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        lastLine = line;
        if (line.find("stage=main_game_shutdown_begin") != std::string::npos) {
            ++shutdownBeginCount;
            assert(!foundCleanShutdown);
        }
        if (line == "CLEAN_SHUTDOWN") foundCleanShutdown = true;
    }

    assert(shutdownBeginCount == 1);
    assert(foundCleanShutdown);
    assert(lastLine == "CLEAN_SHUTDOWN");
    return 0;
}
