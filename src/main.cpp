#include "game/main_game.hpp"
#include "platform/input.hpp"
#include "render/graphics.hpp"
#include <cstdlib>

#ifdef __vita__
// vita-elf-create appends SCE module metadata to the module's first LOAD
// segment. Keep enough real read-only payload in that segment to cross the
// next 0x10000 boundary, leaving room for the metadata without overlap.
__attribute__((used, section(".rodata")))
static const unsigned char vitaElfMetadataPadding[0x1000] = {};
#endif

int main() {
    wormhole::Graphics graphics;
    if (!graphics.init()) return 1;

    wormhole::Input input;
    wormhole::MainGame game;
#ifndef __vita__
    const char* archivePath = std::getenv("FTL_DAT_PATH");
    game.init(graphics, input, archivePath ? archivePath : "ftl.dat");
#else
    // Reference the padding as a volatile read so the linker keeps the whole
    // .rodata section in the first LOAD segment.
    volatile const unsigned char vitaElfPaddingAnchor = vitaElfMetadataPadding[0];
    (void)vitaElfPaddingAnchor;

    // Keep the runtime archive path explicit on Vita. The VPK contains the
    // executable, while the user-provided FTL data archive lives outside it.
    game.init(graphics, input, "ux0:data/wormhole/ftl.dat");
#endif

#ifdef __vita__
    for (;;) {
        input.beginFrame();
        input.poll();
        if (input.down(wormhole::Button::Start) && input.down(wormhole::Button::Select))
            break;

        graphics.beginFrame({0.035f, 0.045f, 0.065f, 1.f});
                game.update(1.0f / 60.0f);
        game.render();
        graphics.endFrame();
    }
#else
    graphics.beginFrame({0.f, 0.f, 0.f, 1.f});
    game.update(1.0f / 60.0f);
    game.render();
    graphics.endFrame();
#endif

    game.shutdown();
    graphics.shutdown();
    return 0;
}
