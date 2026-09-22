#pragma once
namespace wormhole {
class GameState {
public:
    virtual ~GameState() = default;
    virtual void update(float dt) = 0;
    virtual void render() = 0;
};
}
