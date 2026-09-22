#include "game/main_game.hpp"
#include "game/game_state.hpp"
namespace wormhole {
MainGame::MainGame() = default;
MainGame::~MainGame() { shutdown(); }
void MainGame::init() { initialized_ = true; }
void MainGame::update(float dt) { if (state_) state_->update(dt); }
void MainGame::render() { if (state_) state_->render(); }
void MainGame::shutdown() { state_.reset(); initialized_ = false; }
void MainGame::setState(std::unique_ptr<GameState> state) { state_ = std::move(state); }
}
