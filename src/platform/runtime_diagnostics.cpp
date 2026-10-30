#include "platform/runtime_diagnostics.hpp"
#include "platform/platform.hpp"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#ifdef __vita__
#include <psp2/io/stat.h>
#endif
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>

namespace wormhole {
namespace {
std::mutex gMutex;
std::string gPath;
bool gStarted = false;
// Once a session is marked clean, ignore late destructor checkpoints until
// startSession() explicitly begins another session.
bool gCleanShutdown = false;
unsigned int gDrawStageMask = 0;

std::string hostPath() {
#ifdef __vita__
    return Platform::saveDirectory() + "/ftl_runtime_dump.txt";
#else
    return "ftl_runtime_dump.txt";
#endif
}

void writeLineLocked(const std::string& line) {
    if (gPath.empty()) gPath = hostPath();
    std::ofstream out(gPath, std::ios::app);
    if (!out) return;
    out << line << '\n';
    out.flush();
}

std::string timestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// Inspect only the tail so an append-only diagnostic log cannot make startup
// increasingly expensive. If the previous session header is outside this
// window, report "unknown" rather than guessing.
std::string previousSessionStatus(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "unknown";

    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size <= 0) return "unknown";

    constexpr std::streamoff kScanLimit = 64 * 1024;
    const std::streamoff start = size > kScanLimit ? size - kScanLimit : 0;
    in.seekg(start, std::ios::beg);
    if (start > 0) {
        std::string partialLine;
        std::getline(in, partialLine);
    }

    bool foundSession = false;
    std::string status = "unknown";
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("=== FTL runtime session ", 0) == 0) {
            foundSession = true;
            status = "unclean";
        } else if (foundSession && line == "CLEAN_SHUTDOWN") {
            status = "clean";
        }
    }
    return foundSession ? status : "unknown";
}
}

void RuntimeDiagnostics::startSession(const std::string& executable, const std::string& archivePath) {
    std::lock_guard<std::mutex> lock(gMutex);
#ifdef __vita__
    // The diagnostic directory is not guaranteed to exist after a fresh VPK
    // install. Create it before the first checkpoint so startup failures are
    // still recorded on a clean system.
    sceIoMkdir("ux0:data", 0777);
    sceIoMkdir("ux0:data/wormhole", 0777);
#endif
    gPath = hostPath();
    gDrawStageMask = 0;
    const std::string previousStatus = previousSessionStatus(gPath);

    std::ofstream out(gPath, std::ios::app);
    if (!out) return;

    // Re-enable logging only after the new session file was opened.
    gCleanShutdown = false;
    out << "\n=== FTL runtime session " << timestamp() << " ===\n";
#ifdef __vita__
    out << "platform=vita\n";
#else
    out << "platform=host\n";
#endif
    out << "executable=" << executable << "\n";
    out << "archive=" << archivePath << "\n";
    out << "previous_session=" << previousStatus << "\n";
    out << "NOTE=For native crashes, also collect the Vita .psp2dmp from ux0:data; this file records game context immediately before the crash.\n";
    out.flush();
    gStarted = true;
}

void RuntimeDiagnostics::checkpoint(const std::string& stage, const std::string& detail) {
    // These stages are called once per rendered frame. Reopening, writing, and
    // flushing a file for every frame can stall the Vita's render loop badly.
    // Keep startup/crash diagnostics intact, but never persist the frame-loop
    // bookkeeping stages.
    if (stage == "begin_frame_clear_begin" ||
        stage == "begin_frame_clear_color_complete" ||
        stage == "begin_frame_clear_complete")
        return;

    std::lock_guard<std::mutex> lock(gMutex);
    if (gCleanShutdown) return;
    // Persist each known primitive checkpoint only once per session. A fixed
    // bitmask keeps the individual GL crash boundaries visible without a
    // dynamic container or repeated file open/flush for every rectangle.
    unsigned int drawStageBit = 0;
    if (stage == "fill_rect_color_begin") drawStageBit = 1u << 0;
    else if (stage == "fill_rect_color_complete") drawStageBit = 1u << 1;
    else if (stage == "fill_rect_begin_begin") drawStageBit = 1u << 2;
    else if (stage == "fill_rect_array_begin") drawStageBit = 1u << 3;
    else if (stage == "fill_rect_draw_begin") drawStageBit = 1u << 4;
    else if (stage == "fill_rect_draw_complete") drawStageBit = 1u << 5;
    else if (stage == "fill_rect_array_complete") drawStageBit = 1u << 6;
    else if (stage == "draw_line_begin") drawStageBit = 1u << 7;
    else if (stage == "draw_line_color_complete") drawStageBit = 1u << 8;
    else if (stage == "draw_line_array_begin") drawStageBit = 1u << 9;
    else if (stage == "draw_line_draw_begin") drawStageBit = 1u << 10;
    else if (stage == "draw_line_draw_complete") drawStageBit = 1u << 11;
    else if (stage == "draw_line_complete") drawStageBit = 1u << 12;
    if (drawStageBit != 0) {
        if ((gDrawStageMask & drawStageBit) != 0) return;
        gDrawStageMask |= drawStageBit;
    }
    if (!gStarted) gStarted = true;
    std::ostringstream ss;
    ss << "[" << timestamp() << "] stage=" << stage;
    if (!detail.empty()) ss << " " << detail;
    writeLineLocked(ss.str());
}

void RuntimeDiagnostics::markCleanShutdown() {
    std::lock_guard<std::mutex> lock(gMutex);
    if (!gStarted) return;
    writeLineLocked("CLEAN_SHUTDOWN");
    gStarted = false;
    gCleanShutdown = true;
}

void RuntimeDiagnostics::terminateHandler() {
    std::lock_guard<std::mutex> lock(gMutex);
    if (!gStarted) return;
    writeLineLocked("FATAL_TERMINATE: uncaught C++ exception or std::terminate()");
}

const std::string& RuntimeDiagnostics::path() {
    if (gPath.empty()) gPath = hostPath();
    return gPath;
}

} // namespace wormhole
