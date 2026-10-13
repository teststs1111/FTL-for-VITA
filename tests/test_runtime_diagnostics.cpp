#include "platform/runtime_diagnostics.hpp"

#include <cassert>
#include <fstream>
#include <string>

int main() {
    wormhole::RuntimeDiagnostics::startSession("test", "ftl.dat");
    wormhole::RuntimeDiagnostics::checkpoint("test_stage", "sector=1");
    wormhole::RuntimeDiagnostics::checkpoint("fill_rect_test_begin");
    wormhole::RuntimeDiagnostics::checkpoint("fill_rect_test_begin");
    wormhole::RuntimeDiagnostics::checkpoint("draw_line_test_begin");
    wormhole::RuntimeDiagnostics::checkpoint("draw_line_test_begin");
    wormhole::RuntimeDiagnostics::markCleanShutdown();

    std::ifstream in(wormhole::RuntimeDiagnostics::path());
    assert(in.good());

    std::string line;
    bool foundStage = false;
    bool foundClean = false;
    int fillRectCount = 0;
    int drawLineCount = 0;
    while (std::getline(in, line)) {
        if (line.find("stage=test_stage") != std::string::npos) foundStage = true;
        if (line == "CLEAN_SHUTDOWN") foundClean = true;
        if (line.find("stage=fill_rect_test_begin") != std::string::npos) ++fillRectCount;
        if (line.find("stage=draw_line_test_begin") != std::string::npos) ++drawLineCount;
    }
    assert(foundStage);
    assert(foundClean);
    assert(fillRectCount == 1);
    assert(drawLineCount == 1);
    return 0;
}
