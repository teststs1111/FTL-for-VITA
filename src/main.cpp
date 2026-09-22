#include "game/main_game.hpp"
int main() {
    wormhole::MainGame game;
    game.init();
    for (int i = 0; i < 1; ++i) {
        game.update(1.0f / 60.0f);
        game.render();
    }
    game.shutdown();
    return 0;
}
