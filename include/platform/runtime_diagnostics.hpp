#pragma once

#include <string>

namespace wormhole {

class RuntimeDiagnostics {
public:
    static void startSession(const std::string& executable, const std::string& archivePath);
    static void checkpoint(const std::string& stage, const std::string& detail = {});
    static void markCleanShutdown();
    static void terminateHandler();

    static const std::string& path();
};

} // namespace wormhole
