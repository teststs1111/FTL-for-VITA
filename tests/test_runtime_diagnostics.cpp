#include "platform/runtime_diagnostics.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>

int main() {
    // The production diagnostic file is append-only. Isolate this test so
    // repeated local/CI runs do not count records from an earlier invocation.
    std::remove(wormhole::RuntimeDiagnostics::path().c_str());

    wormhole::RuntimeDiagnostics::startSession("test", "ftl.dat");
    wormhole::RuntimeDiagnostics::checkpoint("test_stage", "sector=1");
    wormhole::RuntimeDiagnostics::checkpoint("first_frame_render_complete");
    // The Vita main loop currently reports this milestone on every frame.
    // It must remain a one-shot persisted checkpoint for the whole session.
    wormhole::RuntimeDiagnostics::checkpoint("first_frame_render_complete");
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

    // Late destructor activity must not append after the clean marker.
    wormhole::RuntimeDiagnostics::checkpoint("late_destructor_checkpoint");

    // A subsequent session re-enables logging and reads the persisted clean
    // marker instead of relying on the current process's static state.
    wormhole::RuntimeDiagnostics::startSession("test-clean", "ftl.dat");
    wormhole::RuntimeDiagnostics::checkpoint("second_session_checkpoint");
    wormhole::RuntimeDiagnostics::markCleanShutdown();

    // Simulate a crash by starting a session without a clean marker. The next
    // session must report that immediately preceding session as unclean.
    wormhole::RuntimeDiagnostics::startSession("test-unclean", "ftl.dat");
    wormhole::RuntimeDiagnostics::startSession("test-after-unclean", "ftl.dat");
    wormhole::RuntimeDiagnostics::markCleanShutdown();

    // A newer clean exit must supersede the older unclean session.
    wormhole::RuntimeDiagnostics::startSession("test-after-clean-after-unclean", "ftl.dat");
    wormhole::RuntimeDiagnostics::markCleanShutdown();

    // If the previous session header has aged out of the bounded tail scan,
    // report unknown rather than guessing from a stale CLEAN_SHUTDOWN marker.
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
    std::string currentExecutable;
    std::string initialPreviousSession;
    std::string cleanPreviousSession;
    std::string uncleanAfterCrashSession;
    std::string cleanAfterUncleanPreviousSession;
    std::string expiredPreviousSession;
    std::string lastLine;
    bool foundStage = false;
    bool foundFirstFrame = false;
    bool foundClean = false;
    bool foundLateDestructor = false;
    bool foundSecondSession = false;
    int fillRectBeginCount = 0;
    int fillRectCompleteCount = 0;
    int drawLineBeginCount = 0;
    int drawLineCompleteCount = 0;
    int firstFrameCompleteCount = 0;

    while (std::getline(in, line)) {
        if (!line.empty()) lastLine = line;
        if (line.rfind("=== FTL runtime session ", 0) == 0)
            currentExecutable.clear();
        if (line.rfind("executable=", 0) == 0)
            currentExecutable = line.substr(std::string("executable=").size());
        if (line.rfind("previous_session=", 0) == 0) {
            const std::string status = line.substr(std::string("previous_session=").size());
            if (currentExecutable == "test") initialPreviousSession = status;
            if (currentExecutable == "test-clean") cleanPreviousSession = status;
            if (currentExecutable == "test-after-unclean") uncleanAfterCrashSession = status;
            if (currentExecutable == "test-after-clean-after-unclean")
                cleanAfterUncleanPreviousSession = status;
            if (currentExecutable == "test-history-outside-window")
                expiredPreviousSession = status;
        }
        if (line.find("stage=test_stage") != std::string::npos) foundStage = true;
        if (line.find("stage=first_frame_render_complete") != std::string::npos)
            foundFirstFrame = true;
        if (line == "CLEAN_SHUTDOWN") foundClean = true;
        if (line.find("stage=late_destructor_checkpoint") != std::string::npos)
            foundLateDestructor = true;
        if (line.find("stage=second_session_checkpoint") != std::string::npos)
            foundSecondSession = true;
        if (line.find("stage=fill_rect_color_begin") != std::string::npos)
            ++fillRectBeginCount;
        if (line.find("stage=fill_rect_color_complete") != std::string::npos)
            ++fillRectCompleteCount;
        if (line.find("stage=draw_line_begin") != std::string::npos)
            ++drawLineBeginCount;
        if (line.find("stage=draw_line_color_complete") != std::string::npos)
            ++drawLineCompleteCount;
        if (line.find("stage=first_frame_render_complete") != std::string::npos)
            ++firstFrameCompleteCount;
    }

    assert(foundStage);
    assert(foundFirstFrame);
    assert(foundClean);
    assert(!foundLateDestructor);
    assert(foundSecondSession);
    assert(lastLine == "CLEAN_SHUTDOWN");
    assert(fillRectBeginCount == 1);
    assert(fillRectCompleteCount == 1);
    assert(drawLineBeginCount == 1);
    assert(drawLineCompleteCount == 1);
    assert(firstFrameCompleteCount == 1);

    assert(initialPreviousSession == "unknown");
    assert(cleanPreviousSession == "clean");
    assert(uncleanAfterCrashSession == "unclean");
    assert(cleanAfterUncleanPreviousSession == "clean");
    assert(expiredPreviousSession == "unknown");
    return 0;
}
