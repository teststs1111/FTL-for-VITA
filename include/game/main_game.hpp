#pragma once
#include <memory>
namespace wormhole {
class GameState;
class MainGame {
public:
    MainGame();
    ~MainGame();
    void init();
    void update(float dt);
    void render();
    void shutdown();
    void setState(std::unique_ptr<GameState> state);
private:
    std::unique_ptr<GameState> state_;
    bool initialized_{false};
};
}
