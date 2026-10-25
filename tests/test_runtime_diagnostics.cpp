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

    // A new process/session should identify the previous clean exit from the
    // persisted log, not from the current process's static state.
    wormhole::RuntimeDiagnostics::startSession("test-clean", "ftl.dat");
    wormhole::RuntimeDiagnostics::markCleanShutdown();

    // Simulate a crash by starting another session without writing
    // CLEAN_SHUTDOWN, then verify the next session reports it as unclean.
    wormhole::RuntimeDiagnostics::startSession("test-unclean", "ftl.dat");
    wormhole::RuntimeDiagnostics::startSession("test-after-unclean", "ftl.dat");
    wormhole::RuntimeDiagnostics::markCleanShutdown();

    // The clean exit of the newest session must supersede an older unclean one.
    wormhole::RuntimeDiagnostics::startSession("test-after-clean-after-unclean", "ftl.dat");
    wormhole::RuntimeDiagnostics::markCleanShutdown();

    // If the previous session header has aged out of the bounded tail scan,
    // the next run must report unknown instead of reusing stale history.
    {
        std::ofstream padding(wormhole::RuntimeDiagnostics::path(), std::ios::app);
        assert(padding.good());
        padding << "diagnostic-padding=" << std::string(70 * 1024, 'x') << '\n';
    }
    wormhole::RuntimeDiagnostics::startSession("test-history-outside-window", "ftl.dat");
    wormhole::RuntimeDiagnostics::markCleanShutdown();

    std::ifstream in(wormhole::RuntimeDiagnostics::path());
    assert(in.good());

    std::string line;
    bool foundStage = false;
    bool foundFirstFrame = false;
    bool foundClean = false;
    bool foundUnknownPreviousSession = false;
    bool foundCleanPreviousSession = false;
    bool foundUncleanPreviousSession = false;
    int fillRectBeginCount = 0;
    int fillRectCompleteCount = 0;
    int drawLineBeginCount = 0;
    int drawLineCompleteCount = 0;
    std::string latestPreviousSession;
    std::string currentExecutable;
    std::string initialPreviousSession;
    std::string uncleanAfterCrashSession;
    std::string cleanAfterUncleanPreviousSession;
    while (std::getline(in, line)) {
        if (line.rfind("=== FTL runtime session ", 0) == 0) latestPreviousSession.clear();
        if (line.rfind("executable=", 0) == 0)
            currentExecutable = line.substr(std::string("executable=").size());
        if (line.rfind("previous_session=", 0) == 0) {
            latestPreviousSession = line.substr(std::string("previous_session=").size());
            if (currentExecutable == "test")
                initialPreviousSession = latestPreviousSession;
            if (currentExecutable == "test-after-unclean")
                uncleanAfterCrashSession = latestPreviousSession;
            if (currentExecutable == "test-after-clean-after-unclean")
                cleanAfterUncleanPreviousSession = latestPreviousSession;
        }
        if (line.find("stage=test_stage") != std::string::npos) foundStage = true;
        if (line.find("stage=first_frame_render_complete") != std::string::npos) foundFirstFrame = true;
        if (line == "CLEAN_SHUTDOWN") foundClean = true;
        if (line == "previous_session=unknown") foundUnknownPreviousSession = true;
        if (line == "previous_session=clean") foundCleanPreviousSession = true;
        if (line == "previous_session=unclean") foundUncleanPreviousSession = true;
        if (line.find("stage=fill_rect_color_begin") != std::string::npos) ++fillRectBeginCount;
        if (line.find("stage=fill_rect_color_complete") != std::string::npos) ++fillRectCompleteCount;
        if (line.find("stage=draw_line_begin") != std::string::npos) ++drawLineBeginCount;
        if (line.find("stage=draw_line_color_complete") != std::string::npos) ++drawLineCompleteCount;
    }
    assert(foundStage);
    assert(foundFirstFrame);
    assert(foundClean);
    assert(foundUnknownPreviousSession);
    assert(foundCleanPreviousSession);
    assert(foundUncleanPreviousSession);
    assert(fillRectBeginCount == 1);
    assert(fillRectCompleteCount == 1);
    assert(drawLineBeginCount == 1);
    assert(drawLineCompleteCount == 1);
    assert(initialPreviousSession == "unknown");
    assert(uncleanAfterCrashSession == "unclean");
    assert(cleanAfterUncleanPreviousSession == "clean");
    assert(latestPreviousSession == "unknown");
    return 0;
}
