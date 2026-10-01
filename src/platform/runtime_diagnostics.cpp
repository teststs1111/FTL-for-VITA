#include "platform/runtime_diagnostics.hpp"
#include "platform/platform.hpp"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>

namespace wormhole {
namespace {
std::mutex gMutex;
std::string gPath;
bool gStarted = false;

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
}

void RuntimeDiagnostics::startSession(const std::string& executable, const std::string& archivePath) {
    std::lock_guard<std::mutex> lock(gMutex);
    gPath = hostPath();

    std::ofstream out(gPath, std::ios::app);
    if (!out) return;

    out << "\n=== FTL runtime session " << timestamp() << " ===\n";
#ifdef __vita__
    out << "platform=vita\n";
#else
    out << "platform=host\n";
#endif
    out << "executable=" << executable << "\n";
    out << "archive=" << archivePath << "\n";
    out << "previous_session=" << (gStarted ? "active" : "unknown") << "\n";
    out << "NOTE=For native crashes, also collect the Vita .psp2dmp from ux0:data; this file records game context immediately before the crash.\n";
    out.flush();
    gStarted = true;
}

void RuntimeDiagnostics::checkpoint(const std::string& stage, const std::string& detail) {
    std::lock_guard<std::mutex> lock(gMutex);
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
