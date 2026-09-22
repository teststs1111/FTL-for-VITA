#pragma once
#include <memory>

namespace wormhole { class Input; }
namespace wormhole {
class GameState;
class Graphics;
class MainGame {
public:
    MainGame();
    ~MainGame();
    void init(Graphics& graphics, Input& input, const char* archivePath = "ux0:data/wormhole/ftl.dat");
    void update(float dt);
    void render();
    void shutdown();
    void setState(std::unique_ptr<GameState> state);
private:
    std::unique_ptr<GameState> state_;
    bool initialized_{false};
};
}
