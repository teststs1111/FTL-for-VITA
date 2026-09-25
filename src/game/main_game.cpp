#include "game/main_game.hpp"
#include "game/game_state.hpp"
#include "data/ship_content.hpp"
#include "game/ship_runtime.hpp"
#include "game/combat_runtime.hpp"
#include "render/graphics.hpp"
#include "render/texture_cache.hpp"
#include "render/text_renderer.hpp"
#include "platform/input.hpp"
#include <algorithm>
#include <cctype>
#include <utility>

namespace wormhole {

class ShipScene final : public GameState {
public:
    ShipScene(Graphics& graphics, Input& input, Localization& localization, const char* archivePath) : graphics_(graphics), input_(input), localization_(localization) {
        // Initialize text first so archive failures can be diagnosed on-device.
        if (!text_.init()) {
            startupError_ = "Text renderer initialization failed";
            return;
        }
        if (!archivePath) {
            startupError_ = "FTL archive path is not configured";
            return;
        }
        if (!content_.open(archivePath)) {
            startupError_ = "FTL archive not found: " + std::string(archivePath);
            return;
        }
        {
            if (const auto* bytes = content_.assets().getBytes("data/text-ja.xml"))
                localization_.loadFtlTextXml(*bytes);
            if (!content_.loadPlayerShip()) {
                startupError_ = "Player ship blueprint could not be loaded";
                return;
            }
            if (!runtime_.load(content_)) {
                startupError_ = "Ship runtime initialization failed";
                return;
            }
            LoadedShip enemy;
            const LoadedShip* player = content_.playerShip();
            std::string enemyId;
            if (player) {
                for (const auto& entry : content_.blueprints().ships()) {
                    if (entry.first != player->blueprint.id) { enemyId = entry.first; break; }
                }
            }
            if (enemyId.empty() || !content_.loadShip(enemyId, enemy)) {
                startupError_ = "Enemy ship blueprint could not be loaded";
                return;
            }
            if (!enemy.blueprint.id.empty()) {
                if (!combat_.load(content_, enemy)) {
                    startupError_ = "Combat runtime initialization failed";
                    return;
                }
            } else {
                startupError_ = "Enemy ship blueprint could not be loaded";
                return;
            }
            discoverRoomTextures();
            discoverWeaponAndDroneTextures();
            discoverCrewTextures();
            discoverShipTexture();
        }
    }

    ~ShipScene() override {
        textures_.clear(graphics_);
        text_.shutdown(graphics_);
    }

    void discoverRoomTextures() {
        roomTextureNames_.clear();
        enemyRoomTextureNames_.clear();

        auto discover = [&](const LoadedShip* ship, std::unordered_map<int, std::string>& dst) {
            if (!ship) return;
            for (const auto& system : ship->blueprint.systems) {
                if (system.room < 0 || system.system.empty()) continue;
                const std::string candidate = "img/ship/interior/room_" + system.system + ".png";
                if (textures_.load(graphics_, content_.assets(), candidate))
                    dst[system.room] = candidate;
            }
        };

        discover(content_.playerShip(), roomTextureNames_);
        discover(&combat_.enemy.content, enemyRoomTextureNames_);
    }

    void discoverWeaponAndDroneTextures() {
        weaponTextureNames_.clear();
        droneTextureNames_.clear();
        for (const auto& weapon : content_.blueprints().weapons()) {
            const auto add = [&](std::unordered_map<std::string, std::string>& dst, const std::string& stem) {
                if (stem.empty()) return;
                std::vector<std::string> candidates = {
                    "img/weapons/" + stem + ".png",
                    "img/weapons/" + stem + "_base.png",
                    "img/weapon/" + stem + ".png",
                    "img/weapon/" + stem + "_base.png"
                };
                for (const auto& name : content_.assets().fileNames()) {
                    const std::string prefix = "img/weapons/" + stem + "_";
                    if (name.rfind(prefix, 0) == 0 && name.size() >= 4 &&
                        name.compare(name.size() - 4, 4, ".png") == 0)
                        candidates.push_back(name);
                }
                for (const auto& candidate : candidates) {
                    if (textures_.load(graphics_, content_.assets(), candidate)) {
                        dst[weapon.first] = candidate;
                        if (!weapon.second.name.empty())
                            dst[weapon.second.name] = candidate;
                        break;
                    }
                }
            };
            add(weaponTextureNames_, weapon.second.projectile);
        }
        for (const auto& drone : content_.blueprints().drones()) {
            if (drone.second.droneImage.empty()) continue;
            const std::string stem = drone.second.droneImage;
            const std::vector<std::string> candidates = {
                "img/ship/drones/" + stem + ".png",
                "img/ship/drones/" + stem + "_base.png",
                "img/drones/" + stem + ".png",
                "img/drones/" + stem + "_base.png",
                "img/drone/" + stem + ".png",
                "img/drone/" + stem + "_base.png"
            };
            for (const auto& candidate : candidates) {
                if (textures_.load(graphics_, content_.assets(), candidate)) {
                    droneTextureNames_[drone.first] = candidate;
                    if (!drone.second.name.empty())
                        droneTextureNames_[drone.second.name] = candidate;
                    break;
                }
            }
        }
    }

    void discoverCrewTextures() {
        crewTextureNames_.clear();
        std::vector<std::string> races;
        if (const auto* player = content_.playerShip()) {
            for (const auto& crew : player->blueprint.crew)
                if (!crew.race.empty()) races.push_back(crew.race);
        }
        for (const auto& crew : combat_.enemy.crew)
            if (!crew.race.empty()) races.push_back(crew.race);
        for (const auto& race : races) {
            const std::vector<std::string> candidates = {
                "img/people/" + race + "_base.png",
                "img/people/" + race + ".png"
            };
            for (const auto& candidate : candidates) {
                if (textures_.load(graphics_, content_.assets(), candidate)) {
                    crewTextureNames_[race] = candidate;
                    break;
                }
            }
        }
    }

    void discoverShipTexture() {
        const LoadedShip* ship = content_.playerShip();
        if (!ship) return;

        // FTL's blueprints explicitly identify the ship artwork stem via img=.
        // Prefer that exact asset before falling back to a deterministic scan.
        std::vector<std::string> candidates;
        if (!ship->blueprint.image.empty()) {
            candidates.push_back("img/ship/" + ship->blueprint.image + "_base.png");
            candidates.push_back("img/ship/" + ship->blueprint.image + ".png");
        }
        if (!ship->blueprint.layout.empty())
            candidates.push_back("img/ship/" + ship->blueprint.layout + "_base.png");

        for (const auto& candidate : candidates) {
            if (textures_.load(graphics_, content_.assets(), candidate)) {
                shipTextureName_ = candidate;
                return;
            }
        }

        std::string best;
        for (const auto& name : content_.assets().fileNames()) {
            std::string lower = name;
            for (char& ch : lower)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (lower.size() < 4 || lower.substr(lower.size() - 4) != ".png") continue;
            if (lower.find("/ship/") == std::string::npos) continue;
            if (lower.find("_base.png") == std::string::npos) continue;
            if (lower.find("gib") != std::string::npos) continue;
            best = name;
            break;
        }
        if (!best.empty() && textures_.load(graphics_, content_.assets(), best))
            shipTextureName_ = best;
    }

    std::string localized(const std::string& key, const std::string& fallback) const {
        const std::string value(localization_.tr(key));
        return value.empty() || value == key ? fallback : value;
    }

    std::string systemLabel(const RuntimeSystem& system) const {
        return localized("system_" + system.type + "_title", system.type);
    }

    std::string crewLabel(const RuntimeCrew& crew) const {
        return localized("crew_" + crew.race + "_title", crew.name.empty() ? crew.race : crew.name);
    }

    std::string weaponLabel(const RuntimeWeapon& weapon) const {
        const std::string byName = localized("weapon_" + weapon.name + "_title", "");
        return byName.empty() ? weapon.name : byName;
    }

    std::string droneLabel(const RuntimeDrone& drone) const {
        const std::string byName = localized("drone_" + drone.name + "_title", "");
        return byName.empty() ? drone.name : byName;
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

        auto drawShip = [&](const ShipRuntime& ship, float originX, bool selectedSide,
                            const std::unordered_map<int, std::string>& roomTextures) {
            for (const auto& room : ship.content.layout.rooms) {
                const float x = originX + room.x * scale;
                const float y = originY + room.y * scale;
                const float w = std::max(1, room.w) * scale;
                const float h = std::max(1, room.h) * scale;
                const bool selected = selectedSide && room.id == combatTargetRoom_;
                const int damage = (room.id >= 0 && room.id < static_cast<int>(ship.roomDamage.size()))
                    ? ship.roomDamage[room.id] : 0;
                const auto textureIt = roomTextures.find(room.id);
                const Texture* roomTexture = textureIt == roomTextures.end()
                    ? nullptr : textures_.get(textureIt->second);
                if (roomTexture && roomTexture->width() > 0 && roomTexture->height() > 0) {
                    graphics_.drawTexture(*roomTexture, x, y, w, h);
                    if (selected)
                        graphics_.fillRect(x, y, w, h, {0.25f, 0.38f, 0.48f, 0.35f});
                    else if (damage > 0)
                        graphics_.fillRect(x, y, w, h, {0.45f, 0.08f, 0.05f, 0.30f});
                } else {
                    graphics_.fillRect(x, y, w, h, selected
                        ? Color{0.25f, 0.38f, 0.48f, 1.f}
                        : (damage > 0 ? Color{0.30f, 0.12f, 0.12f, 1.f}
                                      : Color{0.10f, 0.18f, 0.25f, 1.f}));
                }
                graphics_.drawLine(x, y, x + w, y, {0.35f, 0.65f, 0.85f, 1.f});
                graphics_.drawLine(x + w, y, x + w, y + h, {0.35f, 0.65f, 0.85f, 1.f});
                graphics_.drawLine(x + w, y + h, x, y + h, {0.35f, 0.65f, 0.85f, 1.f});
                graphics_.drawLine(x, y + h, x, y, {0.35f, 0.65f, 0.85f, 1.f});
            }
        };

        drawShip(combat_.player, leftX, false, roomTextureNames_);
        drawShip(combat_.enemy, rightX, true, enemyRoomTextureNames_);

        // Render shield layers as a compact HUD around each combat ship. The
        // runtime already tracks current/max layers and recharge progress, so
        // this visual stays tied to actual combat state instead of being a
        // decorative placeholder.
        auto drawShieldHud = [&](const ShipRuntime& ship, float originX, bool enemySide) {
            const float x = originX - 82.f;
            const float y = 120.f;
            const float width = 164.f;
            const float layerW = 22.f;
            const float gap = 3.f;
            const int maxLayers = std::max(0, ship.maxShieldLayers);
            const int shownLayers = std::min(std::max(0, ship.shieldLayers), maxLayers);
            if (maxLayers > 0) {
                for (int i = 0; i < maxLayers; ++i) {
                    const float lx = x + i * (layerW + gap);
                    const bool active = i < shownLayers;
                    graphics_.fillRect(lx, y, layerW, 7.f,
                        active ? (enemySide ? Color{0.35f, 0.55f, 1.f, 1.f}
                                          : Color{0.30f, 0.80f, 1.f, 1.f})
                               : Color{0.12f, 0.16f, 0.22f, 1.f});
                }
            }
            const float charge = std::clamp(ship.shieldCharge, 0.f, 1.f);
            graphics_.fillRect(x, y + 11.f, width, 4.f, {0.10f, 0.12f, 0.16f, 1.f});
            if (charge > 0.f)
                graphics_.fillRect(x, y + 11.f, width * charge, 4.f,
                    enemySide ? Color{0.35f, 0.55f, 1.f, 0.85f}
                              : Color{0.30f, 0.80f, 1.f, 0.85f});
            text_.draw(graphics_, "シールド " + std::to_string(shownLayers) + "/" + std::to_string(maxLayers),
                x, y - 16.f, 11.f, {0.72f, 0.82f, 0.94f, 1.f});
        };
        drawShieldHud(combat_.player, leftX, false);
        drawShieldHud(combat_.enemy, rightX, true);

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
            const auto weaponTextureIt = weaponTextureNames_.find(shot.weapon.name);
            const Texture* weaponTexture = weaponTextureIt == weaponTextureNames_.end()
                ? nullptr : textures_.get(weaponTextureIt->second);
            bool drewProjectileFrame = false;
            if (weaponTexture && weaponTexture->width() > 0 && weaponTexture->height() > 0) {
                int frameCount = 1;
                const std::string& assetName = weaponTextureIt->second;
                const std::size_t stripPos = assetName.rfind("_strip");
                if (stripPos != std::string::npos) {
                    std::size_t p = stripPos + 5;
                    int parsed = 0;
                    while (p < assetName.size() && std::isdigit(static_cast<unsigned char>(assetName[p]))) {
                        parsed = parsed * 10 + (assetName[p] - '0');
                        ++p;
                    }
                    if (parsed > 1) frameCount = parsed;
                }
                const int frame = frameCount > 1
                    ? std::min(frameCount - 1, static_cast<int>(t * frameCount)) : 0;
                const float u0 = static_cast<float>(frame) / frameCount;
                const float u1 = static_cast<float>(frame + 1) / frameCount;
                const float aspect = (static_cast<float>(weaponTexture->width()) / frameCount) /
                                     std::max(1, weaponTexture->height());
                const float size = 14.f;
                const float h = std::min(18.f, size / std::max(0.1f, aspect));
                graphics_.drawTextureRegion(*weaponTexture, x - size * 0.5f, y - h * 0.5f,
                    size, h, u0, 0.f, u1, 1.f,
                    shot.fromPlayer ? Color{1.f, 0.95f, 0.65f, 1.f}
                                    : Color{1.f, 0.55f, 0.45f, 1.f});
                drewProjectileFrame = true;
            }
            if (!drewProjectileFrame) {
                const float size = 8.f;
                graphics_.fillRect(x - size * 0.5f, y - size * 0.5f, size, size,
                    shot.fromPlayer ? Color{1.f, 0.85f, 0.25f, 1.f}
                                     : Color{1.f, 0.3f, 0.2f, 1.f});
            }
        }


        // Use the real FTL weapon/drone artwork discovered from the blueprints.
        auto drawRuntimeArtwork = [&](const ShipRuntime& ship, float originX) {
            int weaponRoom = -1;
            for (const auto& system : ship.systems) {
                if (system.type == "weapons" && system.room >= 0) {
                    weaponRoom = system.room;
                    break;
                }
            }
            if (weaponRoom >= 0) {
                const auto center = roomCenter(ship, originX, weaponRoom);
                int weaponSlot = 0;
                for (const auto& weapon : ship.weapons) {
                    const auto it = weaponTextureNames_.find(weapon.name);
                    if (it == weaponTextureNames_.end()) continue;
                    const Texture* texture = textures_.get(it->second);
                    if (!texture || texture->width() <= 0 || texture->height() <= 0) continue;
                    const float aspect = static_cast<float>(texture->width()) / texture->height();
                    const float w = 24.f;
                    const float h = std::min(28.f, w / std::max(0.1f, aspect));
                    const float x = center.first - 32.f + (weaponSlot % 3) * 32.f;
                    const float y = center.second - 18.f + (weaponSlot / 3) * 32.f;
                    graphics_.drawTexture(*texture, x - w * 0.5f, y - h * 0.5f, w, h);
                    text_.draw(graphics_, weaponLabel(weapon), x - 20.f, y + 12.f, 9.f,
                        {0.92f, 0.86f, 0.62f, 1.f});
                    ++weaponSlot;
                }
            }

            int droneSlot = 0;
            for (const auto& drone : ship.drones) {
                const auto it = droneTextureNames_.find(drone.name);
                if (it == droneTextureNames_.end()) continue;
                const Texture* texture = textures_.get(it->second);
                if (!texture || texture->width() <= 0 || texture->height() <= 0) continue;
                const float x = originX + 18.f + droneSlot * 34.f;
                const float y = originY - 42.f;
                const float aspect = static_cast<float>(texture->width()) / texture->height();
                const float w = 24.f;
                const float h = w / std::max(0.1f, aspect);
                graphics_.drawTexture(*texture, x - w * 0.5f, y - h * 0.5f, w, h);
                text_.draw(graphics_, droneLabel(drone), x - 22.f, y + 15.f, 10.f,
                    {0.78f, 0.88f, 0.95f, 1.f});
                ++droneSlot;
            }

            int crewSlot = 0;
            for (const auto& crew : ship.crew) {
                if (!crew.alive || crew.room < 0) continue;
                const auto textureIt = crewTextureNames_.find(crew.race);
                const Texture* texture = textureIt == crewTextureNames_.end()
                    ? nullptr : textures_.get(textureIt->second);
                const auto center = roomCenter(ship, originX, crew.room);
                const float x = center.first - 12.f + (crewSlot % 2) * 24.f;
                const float y = center.second - 10.f + (crewSlot / 2) * 20.f;
                if (texture && texture->width() > 0 && texture->height() > 0) {
                    const float w = 11.f;
                    const float h = std::min(18.f, w * static_cast<float>(texture->height()) / texture->width());
                    graphics_.drawTexture(*texture, x - w * 0.5f, y - h * 0.5f, w, h);
                } else {
                    graphics_.fillRect(x - 3.f, y - 3.f, 6.f, 6.f,
                        {0.9f, 0.9f, 0.35f, 1.f});
                }
                ++crewSlot;
            }
        };

        drawRuntimeArtwork(combat_.player, leftX);
        drawRuntimeArtwork(combat_.enemy, rightX);

        const float playerHull = combat_.player.maxHull > 0
            ? static_cast<float>(combat_.player.hull) / combat_.player.maxHull : 0.f;
        const float enemyHull = combat_.enemy.maxHull > 0
            ? static_cast<float>(combat_.enemy.hull) / combat_.enemy.maxHull : 0.f;
        graphics_.fillRect(leftX, 100.f, 280.f, 12.f, {0.15f, 0.15f, 0.15f, 1.f});
        graphics_.fillRect(leftX, 100.f, 280.f * playerHull, 12.f, {0.2f, 0.8f, 0.35f, 1.f});
        graphics_.fillRect(rightX, 100.f, 280.f, 12.f, {0.15f, 0.15f, 0.15f, 1.f});
        graphics_.fillRect(rightX, 100.f, 280.f * enemyHull, 12.f, {0.85f, 0.25f, 0.25f, 1.f});
        text_.draw(graphics_, "船体 " + std::to_string(std::max(0, combat_.player.hull)) +
            "/" + std::to_string(std::max(0, combat_.player.maxHull)),
            leftX, 86.f, 11.f, {0.72f, 0.92f, 0.78f, 1.f});
        text_.draw(graphics_, "敵船体 " + std::to_string(std::max(0, combat_.enemy.hull)) +
            "/" + std::to_string(std::max(0, combat_.enemy.maxHull)),
            rightX, 86.f, 11.f, {0.96f, 0.72f, 0.72f, 1.f});

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
        if (!startupError_.empty()) {
            graphics_.fillRect(40.f, 40.f, 880.f, 464.f, {0.06f, 0.07f, 0.10f, 1.f});
            text_.draw(graphics_, "FTL: Faster Than Light", 70.f, 95.f, 30.f,
                {0.85f, 0.90f, 1.f, 1.f});
            text_.draw(graphics_, "起動データを読み込めませんでした", 70.f, 145.f, 22.f,
                {1.f, 0.75f, 0.35f, 1.f});
            text_.draw(graphics_, startupError_, 70.f, 190.f, 15.f,
                {0.80f, 0.84f, 0.90f, 1.f});
            text_.draw(graphics_, "ux0:data/wormhole/ftl.dat を確認してください", 70.f, 235.f, 15.f,
                {0.70f, 0.78f, 0.88f, 1.f});
            return;
        }
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
            const auto textureIt = roomTextureNames_.find(room.id);
            const Texture* roomTexture = textureIt == roomTextureNames_.end()
                ? nullptr : textures_.get(textureIt->second);
            if (roomTexture && roomTexture->width() > 0 && roomTexture->height() > 0) {
                graphics_.drawTexture(*roomTexture, x, y, w, h);
            } else {
                graphics_.fillRect(x, y, w, h, selected
                    ? Color{0.18f, 0.32f, 0.42f, 1.f}
                    : (damage > 0 ? Color{0.28f, 0.12f, 0.12f, 1.f} : Color{0.10f, 0.18f, 0.25f, 1.f}));
            }
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
                text_.draw(graphics_, systemLabel(system), x, y + 8.f, 13.f,
                    system.powered ? Color{0.75f, 0.95f, 0.82f, 1.f} : Color{0.65f, 0.68f, 0.72f, 1.f});
                break;
            }
        }

        for (const auto& crew : runtime_.crew) {
            if (!crew.alive || crew.room < 0) continue;
            for (const auto& room : ship->layout.rooms) {
                if (room.id != crew.room) continue;
                const float x = originX + (room.x + ship->layout.xOffset) * scale + scale * 0.5f;
                const float y = originY + (room.y + ship->layout.yOffset) * scale + scale * 0.5f;
                const auto it = crewTextureNames_.find(crew.race);
                const Texture* texture = it == crewTextureNames_.end() ? nullptr : textures_.get(it->second);
                if (texture && texture->width() > 0 && texture->height() > 0) {
                    const float w = 13.f;
                    const float h = w * static_cast<float>(texture->height()) / texture->width();
                    graphics_.drawTexture(*texture, x - w * 0.5f, y - std::min(20.f, h) * 0.5f,
                        w, std::min(20.f, h));
                } else {
                    const float size = 5.f;
                    graphics_.fillRect(x - size * 0.5f, y - size * 0.5f, size, size,
                        {0.9f, 0.9f, 0.35f, 1.f});
                }
                text_.draw(graphics_, crewLabel(crew), x + 8.f, y - 7.f, 11.f,
                    {0.92f, 0.92f, 0.78f, 1.f});
                break;
            }
        }

        text_.draw(graphics_, "FTL", 60.f, 35.f, 28.f, {0.85f, 0.9f, 1.f, 1.f});
        text_.draw(graphics_, "艦内システム", 60.f, 68.f, 20.f, {0.7f, 0.85f, 1.f, 1.f});

        if (!shipTextureName_.empty()) {
            const Texture* texture = textures_.get(shipTextureName_);
            if (texture && texture->width() > 0 && texture->height() > 0) {
                constexpr float maxW = 300.f;
                constexpr float maxH = 220.f;
                const float aspect = static_cast<float>(texture->width()) / texture->height();
                float w = maxW;
                float h = w / aspect;
                if (h > maxH) {
                    h = maxH;
                    w = h * aspect;
                }
                graphics_.drawTexture(*texture, 620.f + (maxW - w) * 0.5f,
                    285.f + (maxH - h) * 0.5f, w, h);
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
    Localization& localization_;
    ShipContent content_;
    ShipRuntime runtime_;
    CombatRuntime combat_;
    CombatResult lastCombatResult_{};
    bool combatMode_{false};
    int combatTargetRoom_{0};
    int selectedRoom_{0};
    int selectedCrew_{0};
    TextureCache textures_;
    TextRenderer text_;
    std::unordered_map<int, std::string> roomTextureNames_;
    std::unordered_map<int, std::string> enemyRoomTextureNames_;
    std::unordered_map<std::string, std::string> weaponTextureNames_;
    std::unordered_map<std::string, std::string> droneTextureNames_;
    std::unordered_map<std::string, std::string> crewTextureNames_;
    std::string shipTextureName_;
    std::string startupError_;
};

MainGame::MainGame() = default;
MainGame::~MainGame() { shutdown(); }

void MainGame::init(Graphics& graphics, Input& input, const char* archivePath) {
    if (initialized_) return;
    state_ = std::make_unique<ShipScene>(graphics, input, localization_, archivePath);
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
