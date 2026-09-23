#include "game/main_game.hpp"
#include "game/game_state.hpp"
#include "data/ship_content.hpp"
#include "game/ship_runtime.hpp"
#include "game/combat_runtime.hpp"
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
            LoadedShip enemy;
            if (!content_.loadShip("ENEMY_SHIP", enemy))
                enemy = *content_.playerShip();
            combat_.load(content_, enemy);
        }
    }

    void update(float dt) override {
        if (combatMode_) {
            combat_.update(dt);
            updateCombat();
            if (combat_.outcome == CombatOutcome::EnemyDestroyed) {
                runtime_.hull = combat_.player.hull;
                combatMode_ = false;
            }
            return;
        }

        runtime_.updateEnvironment(dt);
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

        const int selectedRoomId = runtime_.content.layout.rooms[selectedRoom_].id;
        if (input_.pressed(Button::R) && !runtime_.systems.empty()) {
            for (int i = 0; i < static_cast<int>(runtime_.systems.size()); ++i) {
                if (runtime_.systems[i].room == selectedRoomId) {
                    runtime_.setSystemPower(i, runtime_.systems[i].power + 1);
                    break;
                }
            }
        }
        if (input_.pressed(Button::Start) && !runtime_.systems.empty()) {
            for (int i = 0; i < static_cast<int>(runtime_.systems.size()); ++i) {
                if (runtime_.systems[i].room == selectedRoomId) {
                    runtime_.setSystemPower(i, runtime_.systems[i].power - 1);
                    break;
                }
            }
        }

        if (input_.pressed(Button::Select)) {
            combatMode_ = true;
            combatTargetRoom_ = selectedRoomId;
            combat_.setTargetRoom(selectedRoomId);
            return;
        }


        if (input_.pressed(Button::L) && !runtime_.crew.empty()) {
            runtime_.moveCrew(selectedCrew_, selectedRoomId);
        }
        if (input_.pressed(Button::Circle))
            runtime_.repairRoom(selectedRoomId, 1);
        if (input_.pressed(Button::Square))
            runtime_.damageRoom(selectedRoomId, 1);
    }

    void updateCombat() {
        if (!combat_.enemy.valid || combat_.enemy.content.layout.rooms.empty()) {
            combatMode_ = false;
            return;
        }

        const int roomCount = static_cast<int>(combat_.enemy.content.layout.rooms.size());
        if (input_.pressed(Button::Left) || input_.pressed(Button::Up))
            combatTargetRoom_ = (combatTargetRoom_ + roomCount - 1) % roomCount;
        if (input_.pressed(Button::Right) || input_.pressed(Button::Down))
            combatTargetRoom_ = (combatTargetRoom_ + 1) % roomCount;

        if (input_.pressed(Button::L) && !combat_.player.weapons.empty())
            combat_.selectedWeapon = (combat_.selectedWeapon +
                static_cast<int>(combat_.player.weapons.size()) - 1) %
                static_cast<int>(combat_.player.weapons.size());
        if (input_.pressed(Button::R) && !combat_.player.weapons.empty())
            combat_.selectedWeapon = (combat_.selectedWeapon + 1) %
                static_cast<int>(combat_.player.weapons.size());

        combat_.setTargetRoom(combatTargetRoom_);
        if (input_.pressed(Button::Cross))
            lastCombatResult_ = combat_.fireSelectedWeapon();

        if (input_.pressed(Button::Circle))
            combatMode_ = false;
    }

    void renderCombat() {
        constexpr float scale = 28.f;
        constexpr float leftX = 120.f;
        constexpr float rightX = 580.f;
        constexpr float originY = 170.f;

        auto drawShip = [&](const ShipRuntime& ship, float originX, bool selectedSide) {
            for (const auto& room : ship.content.layout.rooms) {
                const float x = originX + room.x * scale;
                const float y = originY + room.y * scale;
                const float w = std::max(1, room.w) * scale;
                const float h = std::max(1, room.h) * scale;
                const bool selected = selectedSide && room.id == combatTargetRoom_;
                const int damage = (room.id >= 0 && room.id < static_cast<int>(ship.roomDamage.size()))
                    ? ship.roomDamage[room.id] : 0;
                graphics_.fillRect(x, y, w, h, selected
                    ? Color{0.25f, 0.38f, 0.48f, 1.f}
                    : (damage > 0 ? Color{0.30f, 0.12f, 0.12f, 1.f}
                                  : Color{0.10f, 0.18f, 0.25f, 1.f}));
                graphics_.drawLine(x, y, x + w, y, {0.35f, 0.65f, 0.85f, 1.f});
                graphics_.drawLine(x + w, y, x + w, y + h, {0.35f, 0.65f, 0.85f, 1.f});
                graphics_.drawLine(x + w, y + h, x, y + h, {0.35f, 0.65f, 0.85f, 1.f});
                graphics_.drawLine(x, y + h, x, y, {0.35f, 0.65f, 0.85f, 1.f});
            }
        };

        drawShip(combat_.player, leftX, false);
        drawShip(combat_.enemy, rightX, true);

        auto roomCenter = [&](const ShipRuntime& ship, float originX, int roomId) {
            for (const auto& room : ship.content.layout.rooms) {
                if (room.id != roomId) continue;
                const float x = originX + room.x * scale;
                const float y = originY + room.y * scale;
                const float w = std::max(1, room.w) * scale;
                const float h = std::max(1, room.h) * scale;
                return std::pair<float, float>{x + w * 0.5f, y + h * 0.5f};
            }
            return std::pair<float, float>{originX, originY};
        };

        for (const auto& shot : combat_.pendingShots()) {
            const ShipRuntime& attacker = shot.fromPlayer ? combat_.player : combat_.enemy;
            const ShipRuntime& target = shot.fromPlayer ? combat_.enemy : combat_.player;
            const float attackerOrigin = shot.fromPlayer ? leftX : rightX;
            const float targetOrigin = shot.fromPlayer ? rightX : leftX;

            int weaponRoom = shot.fromPlayer ? 0 : 0;
            for (const auto& system : attacker.systems) {
                if (system.type == "weapons" && system.room >= 0) {
                    weaponRoom = system.room;
                    break;
                }
            }

            const auto start = roomCenter(attacker, attackerOrigin, weaponRoom);
            const auto end = roomCenter(target, targetOrigin, shot.targetRoom);
            const float t = shot.duration > 0.f
                ? std::min(1.f, std::max(0.f, shot.elapsed / shot.duration)) : 1.f;
            const float x = start.first + (end.first - start.first) * t;
            const float y = start.second + (end.second - start.second) * t;
            const float size = 8.f;
            graphics_.fillRect(x - size * 0.5f, y - size * 0.5f, size, size,
                shot.fromPlayer ? Color{1.f, 0.85f, 0.25f, 1.f}
                                 : Color{1.f, 0.3f, 0.2f, 1.f});
        }

        const float playerHull = combat_.player.maxHull > 0
            ? static_cast<float>(combat_.player.hull) / combat_.player.maxHull : 0.f;
        const float enemyHull = combat_.enemy.maxHull > 0
            ? static_cast<float>(combat_.enemy.hull) / combat_.enemy.maxHull : 0.f;
        graphics_.fillRect(leftX, 100.f, 280.f, 12.f, {0.15f, 0.15f, 0.15f, 1.f});
        graphics_.fillRect(leftX, 100.f, 280.f * playerHull, 12.f, {0.2f, 0.8f, 0.35f, 1.f});
        graphics_.fillRect(rightX, 100.f, 280.f, 12.f, {0.15f, 0.15f, 0.15f, 1.f});
        graphics_.fillRect(rightX, 100.f, 280.f * enemyHull, 12.f, {0.85f, 0.25f, 0.25f, 1.f});

        for (int i = 0; i < combat_.enemy.shieldLayers; ++i)
            graphics_.fillRect(rightX + i * 14.f, 125.f, 10.f, 6.f, {0.25f, 0.65f, 0.95f, 1.f});

        if (combat_.selectedWeapon >= 0 &&
            combat_.selectedWeapon < static_cast<int>(combat_.player.weapons.size())) {
            const auto& weapon = combat_.player.weapons[combat_.selectedWeapon];
            const float ratio = weapon.cooldown > 0.f
                ? std::min(1.f, weapon.charge / weapon.cooldown) : 1.f;
            graphics_.fillRect(leftX, 140.f, 280.f, 8.f, {0.15f, 0.15f, 0.15f, 1.f});
            graphics_.fillRect(leftX, 140.f, 280.f * ratio, 8.f,
                weapon.ready ? Color{0.95f, 0.8f, 0.2f, 1.f}
                              : Color{0.3f, 0.65f, 0.9f, 1.f});
        }

        if (combat_.lastImpactResult().fired) {
            const auto& impact = combat_.lastImpactResult();
            const float feedbackX = impact.targetDestroyed ? rightX + 55.f : rightX + 35.f;
            const float feedbackW = impact.shieldsAbsorbed > 0 ? 8.f : 14.f;
            graphics_.fillRect(feedbackX, 158.f, feedbackW, 8.f,
                impact.shieldsAbsorbed > 0
                    ? Color{0.25f, 0.7f, 1.f, 1.f}
                    : (impact.targetDestroyed
                        ? Color{1.f, 0.8f, 0.2f, 1.f}
                        : Color{0.9f, 0.3f, 0.2f, 1.f}));
        }

        if (lastCombatResult_.fired) {
            graphics_.fillRect(rightX + 20.f, 140.f, 12.f, 12.f,
                lastCombatResult_.targetDestroyed
                    ? Color{1.f, 0.8f, 0.2f, 1.f}
                    : Color{0.9f, 0.3f, 0.2f, 1.f});
        }
    }

    void render() override {
        if (combatMode_) {
            renderCombat();
            return;
        }

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

            if (room.id >= 0 && room.id < static_cast<int>(runtime_.roomFire.size()) && runtime_.roomFire[room.id]) {
                graphics_.fillRect(x + w * 0.38f, y + h * 0.28f, w * 0.24f, h * 0.44f,
                    {1.f, 0.45f, 0.08f, 0.9f});
            }
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

        for (const auto& crew : runtime_.crew) {
            if (!crew.alive || crew.room < 0) continue;
            for (const auto& room : ship->layout.rooms) {
                if (room.id != crew.room) continue;
                const float x = originX + (room.x + ship->layout.xOffset) * scale + scale * 0.5f;
                const float y = originY + (room.y + ship->layout.yOffset) * scale + scale * 0.5f;
                const float size = 5.f;
                graphics_.fillRect(x - size * 0.5f, y - size * 0.5f, size, size,
                    {0.9f, 0.9f, 0.35f, 1.f});
                break;
            }
        }

        for (const auto& door : ship->layout.doors) {
            const float x = originX + (door.x + ship->layout.xOffset) * scale;
            const float y = originY + (door.y + ship->layout.yOffset) * scale;
            const int doorIndex = static_cast<int>(&door - ship->layout.doors.data());
            if (doorIndex < 0 || doorIndex >= static_cast<int>(runtime_.doorOpen.size())) continue;
            const Color doorColor = runtime_.doorOpen[doorIndex]
                ? Color{0.2f, 0.9f, 0.35f, 1.f}
                : Color{0.9f, 0.75f, 0.3f, 1.f};
            if (door.vertical)
                graphics_.drawLine(x, y, x, y + scale, doorColor);
            else
                graphics_.drawLine(x, y, x + scale, y, doorColor);
        }
    }

private:
    Graphics& graphics_;
    Input& input_;
    ShipContent content_;
    ShipRuntime runtime_;
    CombatRuntime combat_;
    CombatResult lastCombatResult_{};
    bool combatMode_{false};
    int combatTargetRoom_{0};
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
