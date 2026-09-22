#include "game/main_game.hpp"
#include "platform/input.hpp"
#include "render/graphics.hpp"

int main() {
    wormhole::Graphics graphics;
    if (!graphics.init()) return 1;

    wormhole::Input input;
    wormhole::MainGame game;
    game.init(graphics);

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
