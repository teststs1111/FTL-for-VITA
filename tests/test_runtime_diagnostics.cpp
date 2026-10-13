#include "platform/runtime_diagnostics.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>

int main() {
    // The diagnostic file is append-only in production. Isolate this test so
    // repeated local runs do not count records from an earlier invocation.
    std::remove(wormhole::RuntimeDiagnostics::path().c_str());
    wormhole::RuntimeDiagnostics::startSession("test", "ftl.dat");
    wormhole::RuntimeDiagnostics::checkpoint("test_stage", "sector=1");
    wormhole::RuntimeDiagnostics::checkpoint("first_frame_render_complete");
    wormhole::RuntimeDiagnostics::checkpoint("fill_rect_color_begin");
    wormhole::RuntimeDiagnostics::checkpoint("fill_rect_color_begin");
    wormhole::RuntimeDiagnostics::checkpoint("fill_rect_color_complete");
    wormhole::RuntimeDiagnostics::checkpoint("fill_rect_color_complete");
    wormhole::RuntimeDiagnostics::checkpoint("draw_line_begin");
    wormhole::RuntimeDiagnostics::checkpoint("draw_line_begin");
    wormhole::RuntimeDiagnostics::checkpoint("draw_line_color_complete");
    wormhole::RuntimeDiagnostics::checkpoint("draw_line_color_complete");
    wormhole::RuntimeDiagnostics::markCleanShutdown();

    std::ifstream in(wormhole::RuntimeDiagnostics::path());
    assert(in.good());

    std::string line;
    bool foundStage = false;
    bool foundFirstFrame = false;
    bool foundClean = false;
    int fillRectBeginCount = 0;
    int fillRectCompleteCount = 0;
    int drawLineBeginCount = 0;
    int drawLineCompleteCount = 0;
    while (std::getline(in, line)) {
        if (line.find("stage=test_stage") != std::string::npos) foundStage = true;
        if (line.find("stage=first_frame_render_complete") != std::string::npos) foundFirstFrame = true;
        if (line == "CLEAN_SHUTDOWN") foundClean = true;
        if (line.find("stage=fill_rect_color_begin") != std::string::npos) ++fillRectBeginCount;
        if (line.find("stage=fill_rect_color_complete") != std::string::npos) ++fillRectCompleteCount;
        if (line.find("stage=draw_line_begin") != std::string::npos) ++drawLineBeginCount;
        if (line.find("stage=draw_line_color_complete") != std::string::npos) ++drawLineCompleteCount;
    }
    assert(foundStage);
    assert(foundFirstFrame);
    assert(foundClean);
    assert(fillRectBeginCount == 1);
    assert(fillRectCompleteCount == 1);
    assert(drawLineBeginCount == 1);
    assert(drawLineCompleteCount == 1);
    return 0;
}
