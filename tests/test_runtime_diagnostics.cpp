#include "platform/runtime_diagnostics.hpp"

#include <cassert>
#include <fstream>
#include <string>

int main() {
    wormhole::RuntimeDiagnostics::startSession("test", "ftl.dat");
    wormhole::RuntimeDiagnostics::checkpoint("test_stage", "sector=1");
    wormhole::RuntimeDiagnostics::markCleanShutdown();

    std::ifstream in(wormhole::RuntimeDiagnostics::path());
    assert(in.good());

    std::string line;
    bool foundStage = false;
    bool foundClean = false;
    while (std::getline(in, line)) {
        if (line.find("stage=test_stage") != std::string::npos) foundStage = true;
        if (line == "CLEAN_SHUTDOWN") foundClean = true;
    }
    assert(foundStage);
    assert(foundClean);
    return 0;
}
