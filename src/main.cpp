#include "game/main_game.hpp"
#include "platform/input.hpp"
#include "render/graphics.hpp"
#include "platform/runtime_diagnostics.hpp"
#include <cstdlib>
#include <exception>

#ifdef __vita__
// vita-elf-create appends SCE module metadata to the module's first LOAD
// segment. Keep enough real read-only payload in that segment to cross the
// next 0x10000 boundary, leaving room for the metadata without overlap.
__attribute__((used, section(".rodata")))
static const unsigned char vitaElfMetadataPadding[0x1000] = {};
#endif

int main() {
    std::set_terminate(&wormhole::RuntimeDiagnostics::terminateHandler);
#ifdef __vita__
    wormhole::RuntimeDiagnostics::startSession("eboot.bin", "ux0:data/wormhole/ftl.dat");
#else
    const char* diagnosticArchive = std::getenv("FTL_DAT_PATH");
    wormhole::RuntimeDiagnostics::startSession("vita_wormhole_prototype", diagnosticArchive ? diagnosticArchive : "ftl.dat");
#endif
    wormhole::RuntimeDiagnostics::checkpoint("process_start");

    wormhole::Graphics graphics;
    if (!graphics.init()) {
        wormhole::RuntimeDiagnostics::checkpoint("graphics_init_failed");
        return 1;
    }
    wormhole::RuntimeDiagnostics::checkpoint("graphics_ready");

    wormhole::Input input;
    wormhole::RuntimeDiagnostics::checkpoint("input_object_ready");
    wormhole::MainGame game;
    wormhole::RuntimeDiagnostics::checkpoint("main_game_object_ready");
#ifndef __vita__
    const char* archivePath = std::getenv("FTL_DAT_PATH");
    game.init(graphics, input, archivePath ? archivePath : "ftl.dat");
    wormhole::RuntimeDiagnostics::checkpoint("game_init_returned");
#else
    // Reference the padding as a volatile read so the linker keeps the whole
    // .rodata section in the first LOAD segment.
    volatile const unsigned char vitaElfPaddingAnchor = vitaElfMetadataPadding[0];
    (void)vitaElfPaddingAnchor;

    // Keep the runtime archive path explicit on Vita. The VPK contains the
    // executable, while the user-provided FTL data archive lives outside it.
    game.init(graphics, input, "ux0:data/wormhole/ftl.dat");
    wormhole::RuntimeDiagnostics::checkpoint("game_init_returned");
#endif

#ifdef __vita__
    bool firstFrame = true;
    for (;;) {
        input.beginFrame();
        if (firstFrame) wormhole::RuntimeDiagnostics::checkpoint("first_frame_begin");
        input.poll();
        if (firstFrame) wormhole::RuntimeDiagnostics::checkpoint("first_input_poll_complete");
        if (input.down(wormhole::Button::Start) && input.down(wormhole::Button::Select))
            break;

        graphics.beginFrame({0.035f, 0.045f, 0.065f, 1.f});
                game.update(1.0f / 60.0f);
        game.render();
        graphics.endFrame();
        if (firstFrame) {
            wormhole::RuntimeDiagnostics::checkpoint("first_frame_render_complete");
            firstFrame = false;
        }
    }
#else
    graphics.beginFrame({0.f, 0.f, 0.f, 1.f});
    game.update(1.0f / 60.0f);
    game.render();
    graphics.endFrame();
#endif

    game.shutdown();
    wormhole::RuntimeDiagnostics::checkpoint("game_shutdown_complete");
    graphics.shutdown();
    wormhole::RuntimeDiagnostics::checkpoint("graphics_shutdown_complete");
    wormhole::RuntimeDiagnostics::markCleanShutdown();
    return 0;
}
