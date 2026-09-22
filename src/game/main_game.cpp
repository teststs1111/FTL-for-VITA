#include "game/main_game.hpp"
#include "game/game_state.hpp"
#include "data/ship_content.hpp"
#include "game/ship_runtime.hpp"
#include "render/graphics.hpp"
#include "platform/input.hpp"
#include <algorithm>

namespace wormhole {

class ShipScene final : public GameState {
public:
    ShipScene(Graphics& graphics, Input& input, const char* archivePath) : graphics_(graphics), input_(input) {
        if (archivePath) {
            content_.open(archivePath);
            content_.loadPlayerShip();
            runtime_.load(content_);
        }
    }

    void update(float dt) override {
        (void)dt;
        if (!runtime_.valid || runtime_.content.layout.rooms.empty()) return;
        const int roomCount = static_cast<int>(runtime_.content.layout.rooms.size());
        if (input_.pressed(Button::Left) || input_.pressed(Button::Up))
            selectedRoom_ = (selectedRoom_ + roomCount - 1) % roomCount;
        if (input_.pressed(Button::Right) || input_.pressed(Button::Down))
            selectedRoom_ = (selectedRoom_ + 1) % roomCount;

        if (input_.pressed(Button::Triangle) && !runtime_.crew.empty())
            selectedCrew_ = (selectedCrew_ + 1) % static_cast<int>(runtime_.crew.size());

        if (input_.pressed(Button::Cross)) {
            const int roomId = runtime_.content.layout.rooms[selectedRoom_].id;
            for (int i = 0; i < static_cast<int>(runtime_.systems.size()); ++i) {
                if (runtime_.systems[i].room == roomId) {
                    runtime_.setSystemPowered(i, !runtime_.systems[i].powered);
                    break;
                }
            }
        }
        if (input_.pressed(Button::R) && !runtime_.systems.empty()) {
            const int roomId = runtime_.content.layout.rooms[selectedRoom_].id;
            for (int i = 0; i < static_cast<int>(runtime_.systems.size()); ++i) {
                if (runtime_.systems[i].room == roomId) {
                    runtime_.setSystemPower(i, runtime_.systems[i].power + 1);
                    break;
                }
            }
        }
        if (input_.pressed(Button::L) && !runtime_.crew.empty()) {
            const int target = runtime_.content.layout.rooms[selectedRoom_].id;
            runtime_.moveCrew(selectedCrew_, target);
        }
        if (input_.pressed(Button::Circle))
            runtime_.repairRoom(runtime_.content.layout.rooms[selectedRoom_].id, 1);
        if (input_.pressed(Button::Square))
            runtime_.damageRoom(runtime_.content.layout.rooms[selectedRoom_].id, 1);

    }

    void render() override {
        const LoadedShip* ship = content_.playerShip();
        if (!ship) {
            graphics_.fillRect(60.f, 70.f, 840.f, 400.f, {0.10f, 0.11f, 0.15f, 1.f});
            graphics_.drawLine(60.f, 70.f, 900.f, 70.f, {0.2f, 0.7f, 1.f, 1.f});
            return;
        }

        constexpr float scale = 28.f;
        constexpr float originX = 250.f;
        constexpr float originY = 105.f;

        for (const auto& room : ship->layout.rooms) {
            const float x = originX + (room.x + ship->layout.xOffset) * scale;
            const float y = originY + (room.y + ship->layout.yOffset) * scale;
            const float w = std::max(1, room.w) * scale;
            const float h = std::max(1, room.h) * scale;

            const bool selected = room.id == ship->layout.rooms[std::min(selectedRoom_, static_cast<int>(ship->layout.rooms.size()) - 1)].id;
            const int damage = (room.id >= 0 && room.id < static_cast<int>(runtime_.roomDamage.size()))
                ? runtime_.roomDamage[room.id] : 0;
            graphics_.fillRect(x, y, w, h, selected
                ? Color{0.18f, 0.32f, 0.42f, 1.f}
                : (damage > 0 ? Color{0.28f, 0.12f, 0.12f, 1.f} : Color{0.10f, 0.18f, 0.25f, 1.f}));
            graphics_.drawLine(x, y, x + w, y, {0.35f, 0.65f, 0.85f, 1.f});
            graphics_.drawLine(x + w, y, x + w, y + h, {0.35f, 0.65f, 0.85f, 1.f});
            graphics_.drawLine(x + w, y + h, x, y + h, {0.35f, 0.65f, 0.85f, 1.f});
            graphics_.drawLine(x, y + h, x, y, {0.35f, 0.65f, 0.85f, 1.f});
        }

        for (const auto& system : runtime_.systems) {
            for (const auto& room : ship->layout.rooms) {
                if (room.id != system.room) continue;
                const float x = originX + (room.x + ship->layout.xOffset) * scale + 4.f;
                const float y = originY + (room.y + ship->layout.yOffset) * scale + 4.f;
                const float w = std::min(18.f, std::max(4.f, static_cast<float>(system.power) * 5.f));
                graphics_.fillRect(x, y, w, 5.f,
                    system.powered ? Color{0.25f, 0.9f, 0.45f, 1.f} : Color{0.35f, 0.35f, 0.35f, 1.f});
                break;
            }
        }

        for (const auto& door : ship->layout.doors) {
            const float x = originX + (door.x + ship->layout.xOffset) * scale;
            const float y = originY + (door.y + ship->layout.yOffset) * scale;
            if (door.vertical)
                graphics_.drawLine(x, y, x, y + scale, {0.9f, 0.75f, 0.3f, 1.f});
            else
                graphics_.drawLine(x, y, x + scale, y, {0.9f, 0.75f, 0.3f, 1.f});
        }
    }

private:
    Graphics& graphics_;
    Input& input_;
    ShipContent content_;
    ShipRuntime runtime_;
    int selectedRoom_{0};
    int selectedCrew_{0};
};

MainGame::MainGame() = default;
MainGame::~MainGame() { shutdown(); }

void MainGame::init(Graphics& graphics, Input& input, const char* archivePath) {
    if (initialized_) return;
    state_ = std::make_unique<ShipScene>(graphics, input, archivePath);
    initialized_ = true;
}

void MainGame::update(float dt) {
    if (state_) state_->update(dt);
}

void MainGame::render() {
    if (state_) state_->render();
}

void MainGame::shutdown() {
    state_.reset();
    initialized_ = false;
}

void MainGame::setState(std::unique_ptr<GameState> state) {
    state_ = std::move(state);
}

}
