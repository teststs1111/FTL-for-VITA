#include "game/main_game.hpp"
#include "game/game_state.hpp"
#include "data/ship_content.hpp"
#include "data/event_database.hpp"
#include "data/sector_database.hpp"
#include "data/sector_graph.hpp"
#include "game/ship_runtime.hpp"
#include "game/combat_runtime.hpp"
#include "render/graphics.hpp"
#include "render/texture_cache.hpp"
#include "render/text_renderer.hpp"
#include "platform/input.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <utility>
#include <vector>
#include <array>
#include <unordered_map>

namespace {
std::vector<std::string> loadArchiveSet(const char* basePath) {
    std::vector<std::string> paths;
    if (!basePath || !*basePath) return paths;
    paths.emplace_back(basePath);

    // Optional sidecar manifest. The first archive is always the base game;
    // following non-empty, non-comment lines are layered on top of it.
    std::ifstream manifest(std::string(basePath) + ".dlc");
    std::string line;
    while (std::getline(manifest, line)) {
        const auto first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos || line[first] == '#') continue;
        const auto last = line.find_last_not_of(" \t\r\n");
        paths.push_back(line.substr(first, last - first + 1));
    }
    return paths;
}
}

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
        const auto archivePaths = loadArchiveSet(archivePath);
        if (archivePaths.empty() || !content_.openArchives(archivePaths)) {
            startupError_ = "FTL archive not found: " + std::string(archivePath);
            return;
        }
        {
            if (const auto* bytes = content_.assets().getBytes("data/text-ja.xml"))
                localization_.loadFtlTextXml(*bytes);
            eventDatabase_.load();
            eventOrder_ = eventDatabase_.ids();
            sectorDatabase_.load();
            sectorGraph_.generate(sector_, seed_);
            selectedBeacon_ = sectorGraph_.startNode();
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
            buildShipSelection();
            sceneMode_ = shipChoices_.empty() ? SceneMode::SectorMap : SceneMode::ShipSelect;
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

    struct StoreOffer {
        enum class Kind { Fuel, Missiles, DroneParts, Repair, Weapon, Drone, Augment, SellWeapon, SellDrone, SellAugment };
        Kind kind{Kind::Fuel};
        std::string id;
        int cost{0};
    };

    void buildStoreOffers() {
        storeOffers_.clear();
        storeSelection_ = 0;
        storeOffers_.push_back({StoreOffer::Kind::Fuel, {}, 3});
        storeOffers_.push_back({StoreOffer::Kind::Missiles, {}, 6});
        storeOffers_.push_back({StoreOffer::Kind::DroneParts, {}, 8});
        storeOffers_.push_back({StoreOffer::Kind::Repair, {}, 2});

        std::vector<std::string> weaponIds;
        for (const auto& entry : content_.blueprints().weapons()) weaponIds.push_back(entry.first);
        std::sort(weaponIds.begin(), weaponIds.end());
        if (!weaponIds.empty()) {
            const std::size_t start = static_cast<std::size_t>((seed_ + static_cast<unsigned>(sector_ * 31 + std::max(0, currentBeacon_))) % weaponIds.size());
            for (std::size_t i = 0; i < weaponIds.size() && i < 3; ++i) {
                const auto& id = weaponIds[(start + i) % weaponIds.size()];
                const auto* weapon = content_.blueprints().findWeapon(id);
                if (!weapon || weapon->cost <= 0) continue;
                bool owned = false;
                for (const auto& current : runtime_.weapons)
                    if (current.name == weapon->name) { owned = true; break; }
                if (!owned) storeOffers_.push_back({StoreOffer::Kind::Weapon, id, weapon->cost});
            }
        }

        std::vector<std::string> droneIds;
        for (const auto& entry : content_.blueprints().drones()) droneIds.push_back(entry.first);
        std::sort(droneIds.begin(), droneIds.end());
        if (!droneIds.empty()) {
            const std::size_t start = static_cast<std::size_t>((seed_ + static_cast<unsigned>(sector_ * 17 + std::max(0, currentBeacon_))) % droneIds.size());
            for (std::size_t i = 0; i < droneIds.size() && i < 2; ++i) {
                const auto& id = droneIds[(start + i) % droneIds.size()];
                const auto* drone = content_.blueprints().findDrone(id);
                if (!drone || drone->cost <= 0) continue;
                bool owned = false;
                for (const auto& current : runtime_.drones)
                    if (current.name == drone->name) { owned = true; break; }
                if (!owned) storeOffers_.push_back({StoreOffer::Kind::Drone, id, drone->cost});
            }
        }

        std::vector<std::string> augmentIds;
        for (const auto& entry : content_.blueprints().augments()) augmentIds.push_back(entry.first);
        std::sort(augmentIds.begin(), augmentIds.end());
        if (!augmentIds.empty()) {
            const std::size_t start = static_cast<std::size_t>((seed_ + static_cast<unsigned>(sector_ * 43 + std::max(0, currentBeacon_))) % augmentIds.size());
            for (std::size_t i = 0; i < augmentIds.size() && i < 3; ++i) {
                const auto& id = augmentIds[(start + i) % augmentIds.size()];
                const auto* augment = content_.blueprints().findAugment(id);
                if (!augment || augment->cost <= 0) continue;
                if (std::find(augmentIds_.begin(), augmentIds_.end(), id) == augmentIds_.end())
                    storeOffers_.push_back({StoreOffer::Kind::Augment, id, augment->cost});
            }
        }

        // FTL stores buy back equipment for roughly half its blueprint price.
        // Keep these entries at the end so buying and selling are both available
        // without introducing a separate shop scene.
        for (const auto& weapon : runtime_.weapons) {
            int sellValue = 0;
            for (const auto& entry : content_.blueprints().weapons()) {
                if (entry.second.name == weapon.name) {
                    sellValue = std::max(1, entry.second.cost / 2);
                    break;
                }
            }
            if (sellValue > 0)
                storeOffers_.push_back({StoreOffer::Kind::SellWeapon, weapon.name, sellValue});
        }
        for (const auto& augmentId : augmentIds_) {
            const auto* augment = content_.blueprints().findAugment(augmentId);
            if (augment && augment->cost > 0)
                storeOffers_.push_back({StoreOffer::Kind::SellAugment, augmentId, std::max(1, augment->cost / 2)});
        }
        for (const auto& drone : runtime_.drones) {
            int sellValue = 0;
            for (const auto& entry : content_.blueprints().drones()) {
                if (entry.second.name == drone.name) {
                    sellValue = std::max(1, entry.second.cost / 2);
                    break;
                }
            }
            if (sellValue > 0)
                storeOffers_.push_back({StoreOffer::Kind::SellDrone, drone.name, sellValue});
        }
    }

    void openStore() {
        buildStoreOffers();
        storeOpen_ = true;
        sceneMode_ = SceneMode::Ship;
    }

    RuntimeWeapon makeRuntimeWeapon(const WeaponBlueprint& blueprint) const {
        RuntimeWeapon weapon;
        weapon.name = blueprint.name; weapon.type = blueprint.type;
        weapon.power = std::max(1, blueprint.power);
        weapon.cooldown = std::max(0.1f, blueprint.cooldown);
        weapon.speed = std::max(0, blueprint.speed);
        weapon.shots = std::max(1, blueprint.shots);
        weapon.damage = std::max(0, blueprint.damage);
        weapon.systemDamage = std::max(0, blueprint.systemDamage);
        weapon.ionDamage = std::max(0, blueprint.ionDamage);
        weapon.shieldPiercing = std::max(0, blueprint.shieldPiercing);
        weapon.missilesUsed = std::max(0, blueprint.missilesUsed);
        weapon.personnelDamage = std::max(0, blueprint.personnelDamage);
        weapon.hullBust = std::max(0, blueprint.hullBust);
        weapon.fireChance = std::clamp(blueprint.fireChance, 0, 100);
        weapon.breachChance = std::clamp(blueprint.breachChance, 0, 100);
        weapon.stunChance = std::clamp(blueprint.stunChance, 0, 100);
        weapon.stunDuration = std::max(0, blueprint.stunDuration);
        return weapon;
    }

    RuntimeDrone makeRuntimeDrone(const DroneBlueprint& blueprint) const {
        RuntimeDrone drone;
        drone.type = blueprint.type; drone.name = blueprint.name;
        drone.power = std::max(1, blueprint.power);
        drone.speed = std::max(0, blueprint.speed);
        drone.cooldown = std::max(0, blueprint.cooldown);
        drone.dodge = std::clamp(blueprint.dodge, 0, 100);
        drone.defenceTarget = blueprint.defenceTarget;
        drone.weaponCooldown = std::max(0.1f, blueprint.weaponCooldown);
        drone.weaponShots = std::max(1, blueprint.weaponShots);
        drone.weaponDamage = std::max(0, blueprint.weaponDamage);
        drone.weaponSystemDamage = std::max(0, blueprint.weaponSystemDamage);
        drone.weaponIonDamage = std::max(0, blueprint.weaponIonDamage);
        drone.weaponShieldPiercing = std::max(0, blueprint.weaponShieldPiercing);
        drone.weaponPersonnelDamage = std::max(0, blueprint.weaponPersonnelDamage);
        drone.weaponSpeed = std::max(0, blueprint.speed);
        return drone;
    }

    bool purchaseStoreOffer() {
        if (storeSelection_ < 0 || storeSelection_ >= static_cast<int>(storeOffers_.size())) return false;
        const StoreOffer offer = storeOffers_[static_cast<std::size_t>(storeSelection_)];
        if (scrap_ < offer.cost) {
            combatFeedback_ = "スクラップが足りない"; combatFeedbackTimer_ = 1.4f; return false;
        }
        switch (offer.kind) {
        case StoreOffer::Kind::Fuel:
            if (fuel_ >= 30) { combatFeedback_ = "燃料は満タン"; break; }
            scrap_ -= offer.cost; fuel_ = std::min(30, fuel_ + 1); combatFeedback_ = "燃料を購入"; return true;
        case StoreOffer::Kind::Missiles:
            if (runtime_.missiles >= 50) { combatFeedback_ = "ミサイルは満タン"; break; }
            scrap_ -= offer.cost; runtime_.missiles = std::min(50, runtime_.missiles + 1); combatFeedback_ = "ミサイルを購入"; return true;
        case StoreOffer::Kind::DroneParts:
            if (droneParts_ >= 50) { combatFeedback_ = "ドローン部品は満タン"; break; }
            scrap_ -= offer.cost; ++droneParts_; combatFeedback_ = "ドローン部品を購入"; return true;
        case StoreOffer::Kind::Repair:
            if (runtime_.hull >= runtime_.maxHull) { combatFeedback_ = "船体は無傷"; break; }
            scrap_ -= offer.cost; runtime_.hull = std::min(runtime_.maxHull, runtime_.hull + 1); combatFeedback_ = "船体を1修理"; return true;
        case StoreOffer::Kind::Weapon: {
            const auto* weapon = content_.blueprints().findWeapon(offer.id);
            if (!weapon || static_cast<int>(runtime_.weapons.size()) >= runtime_.content.blueprint.weaponSlots) { combatFeedback_ = "武器スロットがいっぱい"; break; }
            scrap_ -= offer.cost; runtime_.weapons.push_back(makeRuntimeWeapon(*weapon));
            combatFeedback_ = "武器を購入: " + weaponLabel(runtime_.weapons.back());
            storeOffers_.erase(storeOffers_.begin() + storeSelection_);
            storeSelection_ = std::min(storeSelection_, static_cast<int>(storeOffers_.size()) - 1);
            return true;
        }
        case StoreOffer::Kind::Drone: {
            const auto* drone = content_.blueprints().findDrone(offer.id);
            if (!drone || static_cast<int>(runtime_.drones.size()) >= runtime_.content.blueprint.droneSlots) { combatFeedback_ = "ドローンスロットがいっぱい"; break; }
            scrap_ -= offer.cost; runtime_.drones.push_back(makeRuntimeDrone(*drone));
            combatFeedback_ = "ドローンを購入: " + droneLabel(runtime_.drones.back());
            storeOffers_.erase(storeOffers_.begin() + storeSelection_);
            storeSelection_ = std::min(storeSelection_, static_cast<int>(storeOffers_.size()) - 1);
            return true;
        }
        case StoreOffer::Kind::Augment: {
            const auto* augment = content_.blueprints().findAugment(offer.id);
            if (!augment) { combatFeedback_ = "オーグメントが見つかりません"; break; }
            if (augmentIds_.size() >= 3) { combatFeedback_ = "オーグメント枠がいっぱい"; break; }
            if (std::find(augmentIds_.begin(), augmentIds_.end(), augment->id) != augmentIds_.end()) { combatFeedback_ = "すでに装備中"; break; }
            if (scrap_ < offer.cost) { combatFeedback_ = "スクラップ不足"; break; }
            scrap_ -= offer.cost;
            augmentIds_.push_back(augment->id);
            combatFeedback_ = "オーグメント装備: " + (augment->title.empty() ? augment->id : augment->title);
            storeOffers_.erase(storeOffers_.begin() + storeSelection_);
            storeSelection_ = std::min(storeSelection_, static_cast<int>(storeOffers_.size()) - 1);
            return true;
        }
        case StoreOffer::Kind::SellAugment: {
            auto it = std::find(augmentIds_.begin(), augmentIds_.end(), offer.id);
            if (it == augmentIds_.end()) { combatFeedback_ = "売却対象がありません"; break; }
            scrap_ += offer.cost;
            const auto* augment = content_.blueprints().findAugment(offer.id);
            combatFeedback_ = "オーグメントを売却: " + (augment && !augment->title.empty() ? augment->title : offer.id);
            augmentIds_.erase(it);
            storeOffers_.erase(storeOffers_.begin() + storeSelection_);
            storeSelection_ = std::min(storeSelection_, static_cast<int>(storeOffers_.size()) - 1);
            return true;
        }
        case StoreOffer::Kind::SellWeapon: {
            auto it = std::find_if(runtime_.weapons.begin(), runtime_.weapons.end(),
                [&](const RuntimeWeapon& w) { return w.name == offer.id; });
            if (it == runtime_.weapons.end()) { combatFeedback_ = "売却対象がありません"; break; }
            scrap_ += offer.cost;
            combatFeedback_ = "武器を売却: " + weaponLabel(*it);
            runtime_.weapons.erase(it);
            storeOffers_.erase(storeOffers_.begin() + storeSelection_);
            storeSelection_ = std::min(storeSelection_, static_cast<int>(storeOffers_.size()) - 1);
            return true;
        }
        case StoreOffer::Kind::SellDrone: {
            auto it = std::find_if(runtime_.drones.begin(), runtime_.drones.end(),
                [&](const RuntimeDrone& d) { return d.name == offer.id; });
            if (it == runtime_.drones.end()) { combatFeedback_ = "売却対象がありません"; break; }
            scrap_ += offer.cost;
            combatFeedback_ = "ドローンを売却: " + droneLabel(*it);
            runtime_.drones.erase(it);
            storeOffers_.erase(storeOffers_.begin() + storeSelection_);
            storeSelection_ = std::min(storeSelection_, static_cast<int>(storeOffers_.size()) - 1);
            return true;
        }
        }
        combatFeedbackTimer_ = 1.4f;
        return false;
    }

    void updateStore(float dt) {
        if (input_.pressed(Button::Up) && !storeOffers_.empty())
            storeSelection_ = (storeSelection_ + static_cast<int>(storeOffers_.size()) - 1) % static_cast<int>(storeOffers_.size());
        if (input_.pressed(Button::Down) && !storeOffers_.empty())
            storeSelection_ = (storeSelection_ + 1) % static_cast<int>(storeOffers_.size());
        if (input_.pressed(Button::Cross)) purchaseStoreOffer();
        if (input_.pressed(Button::Circle)) {
            storeOpen_ = false;
            ++visitedBeacons_;
            sceneMode_ = SceneMode::SectorMap;
        }
        combatFeedbackTimer_ = std::max(0.0f, combatFeedbackTimer_ - dt);
    }

    void renderStore() {
        graphics_.fillRect(45.f, 35.f, 870.f, 470.f, {0.055f, 0.07f, 0.10f, 1.f});
        text_.draw(graphics_, "STORE / ショップ", 75.f, 75.f, 27.f, {0.90f, 0.94f, 1.f, 1.f});
        text_.draw(graphics_, "スクラップ " + std::to_string(scrap_) + "   燃料 " + std::to_string(fuel_) +
            "   ミサイル " + std::to_string(runtime_.missiles) + "   ドローン " + std::to_string(droneParts_),
            75.f, 105.f, 15.f, {0.78f, 0.84f, 0.92f, 1.f});
        for (std::size_t i = 0; i < storeOffers_.size(); ++i) {
            const auto& offer = storeOffers_[i];
            const bool selected = static_cast<int>(i) == storeSelection_;
            const float y = 140.f + static_cast<float>(i) * 34.f;
            if (selected) graphics_.fillRect(65.f, y - 22.f, 820.f, 32.f, {0.15f, 0.25f, 0.34f, 1.f});
            std::string name;
            switch (offer.kind) {
            case StoreOffer::Kind::Fuel: name = "燃料 +1"; break;
            case StoreOffer::Kind::Missiles: name = "ミサイル +1"; break;
            case StoreOffer::Kind::DroneParts: name = "ドローン部品 +1"; break;
            case StoreOffer::Kind::Repair: name = "船体修理 +1"; break;
            case StoreOffer::Kind::Weapon:
                if (const auto* weapon = content_.blueprints().findWeapon(offer.id)) name = weapon->name;
                break;
            case StoreOffer::Kind::Drone:
                if (const auto* drone = content_.blueprints().findDrone(offer.id)) name = drone->name;
                break;
            case StoreOffer::Kind::Augment:
                if (const auto* augment = content_.blueprints().findAugment(offer.id))
                    name = "AUG: " + (augment->title.empty() ? offer.id : augment->title);
                break;
            case StoreOffer::Kind::SellAugment:
                if (const auto* augment = content_.blueprints().findAugment(offer.id))
                    name = "売却: " + (augment->title.empty() ? offer.id : augment->title);
                break;
            case StoreOffer::Kind::SellWeapon:
                name = "売却: " + offer.id;
                break;
            case StoreOffer::Kind::SellDrone:
                name = "売却: " + offer.id;
                break;
            }
            text_.draw(graphics_, name + "   " + std::to_string(offer.cost) + " scrap", 85.f, y, 16.f,
                selected ? Color{0.98f, 0.84f, 0.48f, 1.f} : Color{0.82f, 0.87f, 0.94f, 1.f});
        }
        text_.draw(graphics_, "↑↓: 選択   ×: 購入/売却   ○: ショップ終了", 75.f, 475.f, 15.f, {0.68f, 0.76f, 0.86f, 1.f});
        if (combatFeedbackTimer_ > 0.0f && !combatFeedback_.empty())
            text_.draw(graphics_, combatFeedback_, 550.f, 475.f, 14.f, {0.95f, 0.76f, 0.40f, 1.f});
    }

    enum class SceneMode { ShipSelect, SectorMap, Ship, Event, Combat, Pause, GameOver, Victory };


    static constexpr const char* savePath() { return "ux0:data/wormhole/save.dat"; }

    bool hasSaveGame() const {
        std::ifstream in(savePath());
        return in.good();
    }

    bool saveGame() {
        if (combatMode_ || storeOpen_ || sceneMode_ == SceneMode::GameOver || sceneMode_ == SceneMode::Victory)
            return false;
        std::ofstream out(savePath(), std::ios::trunc);
        if (!out) return false;

        out << "FTL_VITA_SAVE 3\n";
        out << "ship " << std::quoted(runtime_.content.blueprint.id) << "\n";
        out << "seed " << seed_ << "\n";
        out << "sector " << sector_ << "\n";
        out << "flagship " << flagshipPhase_ << "\n";
        out << "beacon " << currentBeacon_ << ' ' << selectedBeacon_ << "\n";
        out << "fleet " << fleetRow_ << "\n";
        out << "event_usage " << sectorEventUsage_.size() << "\n";
        for (const auto& entry : sectorEventUsage_)
            out << std::quoted(entry.first) << ' ' << entry.second << "\n";
        out << "visited " << visitedBeacons_ << "\n";
        out << "resources " << fuel_ << ' ' << scrap_ << ' ' << droneParts_ << ' ' << runtime_.missiles << "\n";
        out << "hull " << runtime_.hull << "\n";
        out << "augments " << augmentIds_.size() << "\n";
        for (const auto& augment : augmentIds_) out << std::quoted(augment) << "\n";

        out << "systems " << runtime_.systems.size() << "\n";
        for (const auto& s : runtime_.systems)
            out << std::quoted(s.type) << ' ' << s.room << ' ' << s.power << ' ' << s.damage << ' '
                << s.ionDamage << ' ' << (s.powered ? 1 : 0) << "\n";

        out << "crew " << runtime_.crew.size() << "\n";
        for (const auto& crew : runtime_.crew)
            out << std::quoted(crew.race) << ' ' << std::quoted(crew.name) << ' '
                << crew.room << ' ' << crew.health << ' ' << crew.maxHealth << ' ' << (crew.alive ? 1 : 0) << "\n";

        out << "weapons " << runtime_.weapons.size() << "\n";
        for (const auto& w : runtime_.weapons)
            out << std::quoted(w.name) << ' ' << std::quoted(w.type) << ' '
                << w.charge << ' ' << (w.ready ? 1 : 0) << "\n";

        out << "drones " << runtime_.drones.size() << "\n";
        for (const auto& d : runtime_.drones)
            out << std::quoted(d.name) << ' ' << static_cast<int>(d.type) << ' '
                << (d.powered ? 1 : 0) << ' ' << (d.active ? 1 : 0) << "\n";

        out << "doors " << runtime_.doorOpen.size();
        for (bool open : runtime_.doorOpen) out << ' ' << (open ? 1 : 0);
        out << "\n";
        out << "quests " << activeQuestIds_.size() << "\n";
        for (const auto& quest : activeQuestIds_) {
            const auto it = questTargets_.find(quest);
            out << std::quoted(quest) << ' '
                << std::quoted(it == questTargets_.end() ? std::string{} : it->second) << "\n";
        }
        return static_cast<bool>(out);
    }

    bool loadGame() {
        std::ifstream in(savePath());
        if (!in) return false;

        std::string header;
        std::getline(in, header);
        const bool legacySave = header == "FTL_VITA_SAVE 2";
        const bool currentSave = header == "FTL_VITA_SAVE 3";
        if (!legacySave && !currentSave) return false;

        std::string key, shipId;
        in >> key >> std::quoted(shipId);
        if (key != "ship" || shipId.empty()) return false;
        if (!content_.loadPlayerShip("data/blueprints.xml", shipId) || !runtime_.load(content_))
            return false;

        in >> key >> seed_; in >> key >> sector_; in >> key >> flagshipPhase_;
        std::size_t count = 0;
        in >> key >> currentBeacon_ >> selectedBeacon_;
        in >> key >> fleetRow_;
        sectorEventUsage_.clear();
        if (currentSave) {
            in >> key >> count;
            if (key != "event_usage") return false;
            for (std::size_t i = 0; i < count; ++i) {
                std::string eventName;
                int usage = 0;
                in >> std::quoted(eventName) >> usage;
                if (!eventName.empty()) sectorEventUsage_[eventName] = std::max(0, usage);
            }
        }
        in >> key >> visitedBeacons_;
        in >> key >> fuel_ >> scrap_ >> droneParts_ >> runtime_.missiles;
        in >> key >> runtime_.hull;

        in >> key >> count;
        for (std::size_t i = 0; i < count; ++i) {
            std::string type; int room, power, damage, ionDamage, powered;
            in >> std::quoted(type) >> room >> power >> damage >> ionDamage >> powered;
            for (auto& s : runtime_.systems) {
                if (s.type != type || s.room != room) continue;
                s.power = std::clamp(power, 0, s.maxPower);
                s.damage = std::clamp(damage, 0, s.maxPower);
                s.ionDamage = std::clamp(ionDamage, 0, s.maxPower - s.damage);
                s.powered = powered != 0 && s.power > 0;
                break;
            }
        }

        in >> key >> count;
        for (std::size_t i = 0; i < count; ++i) {
            std::string race, name; int room, health, maxHealth, alive;
            in >> std::quoted(race) >> std::quoted(name) >> room >> health >> maxHealth >> alive;
            for (auto& crew : runtime_.crew) {
                if (crew.race != race || crew.name != name) continue;
                crew.room = room;
                crew.maxHealth = maxHealth;
                crew.health = std::clamp(health, 0, maxHealth);
                crew.alive = alive != 0 && crew.health > 0;
                break;
            }
        }

        in >> key >> count;
        for (std::size_t i = 0; i < count; ++i) {
            std::string name, type; float charge; int ready;
            in >> std::quoted(name) >> std::quoted(type) >> charge >> ready;
            for (auto& w : runtime_.weapons) {
                if (w.name != name) continue;
                w.charge = std::max(0.0f, charge);
                w.ready = ready != 0;
                break;
            }
        }

        in >> key >> count;
        for (std::size_t i = 0; i < count; ++i) {
            std::string name; int type, powered, active;
            in >> std::quoted(name) >> type >> powered >> active;
            for (auto& d : runtime_.drones) {
                if (d.name != name) continue;
                d.powered = powered != 0;
                d.active = active != 0;
                break;
            }
        }

        in >> key >> count;
        runtime_.doorOpen.assign(runtime_.content.layout.doors.size(), false);
        for (std::size_t i = 0; i < count; ++i) {
            int open = 0; in >> open;
            if (i < runtime_.doorOpen.size()) runtime_.doorOpen[i] = open != 0;
        }

        activeQuestIds_.clear();
        questTargets_.clear();
        in >> key >> count;
        for (std::size_t i = 0; i < count; ++i) {
            std::string quest, target;
            in >> std::quoted(quest) >> std::quoted(target);
            activeQuestIds_.push_back(quest);
            if (!target.empty()) questTargets_[quest] = target;
        }

        sectorGraph_.generate(sector_, static_cast<std::uint32_t>(seed_ + sector_));
        combat_.player = runtime_;
        currentBeacon_ = std::clamp(currentBeacon_, -1, static_cast<int>(sectorGraph_.nodes().size()) - 1);
        selectedBeacon_ = std::clamp(selectedBeacon_, 0, static_cast<int>(sectorGraph_.nodes().size()) - 1);
        combatMode_ = false;
        storeOpen_ = false;
        jumpCharging_ = false;
        jumpCharge_ = 0.0f;
        sceneMode_ = SceneMode::SectorMap;
        discoverRoomTextures();
        discoverWeaponAndDroneTextures();
        discoverCrewTextures();
        discoverShipTexture();
        return true;
    }

    void buildShipSelection() {
        shipChoices_.clear();
        for (const auto& entry : content_.blueprints().ships()) {
            if (entry.first.rfind("PLAYER_SHIP_", 0) != 0) continue;
            if (entry.second.maxHealth <= 0 || entry.second.layout.empty()) continue;
            shipChoices_.push_back(entry.first);
        }
        std::sort(shipChoices_.begin(), shipChoices_.end());
        shipSelection_ = 0;
        for (std::size_t i = 0; i < shipChoices_.size(); ++i)
            if (shipChoices_[i] == "PLAYER_SHIP_HARD") { shipSelection_ = static_cast<int>(i); break; }
    }

    std::string shipChoiceLabel(const std::string& id) const {
        const auto* ship = content_.blueprints().findShip(id);
        if (!ship) return id;
        if (!ship->name.empty()) return ship->name;
        return id;
    }

    bool applySelectedShip() {
        if (shipSelection_ < 0 || shipSelection_ >= static_cast<int>(shipChoices_.size())) return false;
        const std::string id = shipChoices_[static_cast<std::size_t>(shipSelection_)];
        if (!content_.loadPlayerShip("data/blueprints.xml", id)) return false;
        if (!runtime_.load(content_)) return false;
        combat_.player = runtime_;
        fuel_ = 16;
        scrap_ = 0;
        droneParts_ = 0;
        visitedBeacons_ = 0;
        currentBeacon_ = -1;
        fleetRow_ = -1;
        sector_ = 0;
        flagshipPhase_ = 0;
        selectedBeacon_ = sectorGraph_.startNode();
        activeQuestIds_.clear();
        questTargets_.clear();
        storeOffers_.clear();
        storeOpen_ = false;
        discoverRoomTextures();
        discoverWeaponAndDroneTextures();
        discoverCrewTextures();
        discoverShipTexture();
        sceneMode_ = SceneMode::SectorMap;
        return true;
    }

    void updateShipSelect() {
        if (shipChoices_.empty()) {
            sceneMode_ = SceneMode::SectorMap;
            return;
        }
        if (input_.pressed(Button::Up))
            shipSelection_ = (shipSelection_ + static_cast<int>(shipChoices_.size()) - 1) % static_cast<int>(shipChoices_.size());
        if (input_.pressed(Button::Down))
            shipSelection_ = (shipSelection_ + 1) % static_cast<int>(shipChoices_.size());
        if (input_.pressed(Button::Cross)) {
            if (!applySelectedShip()) {
                combatFeedback_ = "艦の読み込みに失敗";
                combatFeedbackTimer_ = 2.0f;
            }
        }
        if (input_.pressed(Button::Square) && hasSaveGame()) {
            if (!loadGame()) {
                combatFeedback_ = "セーブデータを読み込めません";
                combatFeedbackTimer_ = 2.0f;
            }
        }
    }

    void renderShipSelect() {
        graphics_.fillRect(35.f, 30.f, 890.f, 485.f, {0.045f, 0.06f, 0.09f, 1.f});
        text_.draw(graphics_, "FTL: Faster Than Light", 70.f, 70.f, 28.f, {0.90f, 0.94f, 1.f, 1.f});
        text_.draw(graphics_, "艦を選択", 70.f, 105.f, 22.f, {0.72f, 0.84f, 1.f, 1.f});
        const int first = std::max(0, std::min(shipSelection_ - 4, static_cast<int>(shipChoices_.size()) - 8));
        const int last = std::min(static_cast<int>(shipChoices_.size()), first + 8);
        for (int i = first; i < last; ++i) {
            const bool selected = i == shipSelection_;
            const float y = 145.f + static_cast<float>(i - first) * 38.f;
            text_.draw(graphics_, (selected ? "> " : "  ") + shipChoiceLabel(shipChoices_[static_cast<std::size_t>(i)]),
                90.f, y, 18.f, selected ? Color{0.98f, 0.84f, 0.48f, 1.f} : Color{0.78f, 0.84f, 0.92f, 1.f});
            const auto* ship = content_.blueprints().findShip(shipChoices_[static_cast<std::size_t>(i)]);
            if (selected && ship) {
                text_.draw(graphics_, "HP " + std::to_string(ship->maxHealth) +
                    "  武器 " + std::to_string(ship->weaponSlots) +
                    "  ドローン " + std::to_string(ship->droneSlots) +
                    "  クルー " + std::to_string(ship->crew.size()),
                    530.f, y, 14.f, {0.65f, 0.76f, 0.88f, 1.f});
            }
        }
        text_.draw(graphics_, "↑↓: 選択   ×: この艦で開始   □: セーブから再開", 75.f, 475.f, 15.f, {0.68f, 0.76f, 0.86f, 1.f});
        if (hasSaveGame())
            text_.draw(graphics_, "セーブデータあり", 700.f, 105.f, 14.f, {0.82f, 0.78f, 0.48f, 1.f});
    }

    bool hasAugment(const char* id) const {
        if (std::find(augmentIds_.begin(), augmentIds_.end(), id) != augmentIds_.end()) return true;
        const std::string wanted = id;
        for (const auto& owned : augmentIds_) {
            if ((wanted == "AUTO_RELOADER" && (owned == "RELOADER" || owned == "AUTO_RELOADER")) ||
                (wanted == "FTL_RECHARGE" && (owned == "FTL_RECHARGE_BOOSTER" || owned == "FTL_RECHARGE")) ||
                (wanted == "SCRAP_ARM" && (owned == "SCRAP_RECOVERY_ARM" || owned == "SCRAP_ARM")))
                return true;
        }
        return false;
    }

    float weaponCooldownMultiplier() const {
        // Automated Reloader: 10% faster weapon charging.
        return hasAugment("AUTO_RELOADER") ? 0.90f : 1.0f;
    }

    int applyScrapAugments(int baseReward) {
        if (baseReward <= 0) return 0;
        int reward = baseReward;
        if (hasAugment("SCRAP_ARM"))
            reward += baseReward / 10;
        if (hasAugment("REPAIR_ARM") && runtime_.hull < runtime_.maxHull) {
            const int repair = std::min(2, runtime_.maxHull - runtime_.hull);
            runtime_.hull += repair;
            reward = std::max(0, reward * 85 / 100);
        }
        return reward;
    }

    void enterCombatFromBeacon(const std::string& enemyShipId = {}) {
        // Start every encounter from a clean CombatRuntime state. This resets
        // the previous outcome, projectile queue, boarding timers and enemy
        // fire timers while preserving the persistent player ship below.
        LoadedShip enemyShip;
        if (!enemyShipId.empty()) {
            if (!content_.loadShip(enemyShipId, enemyShip) || enemyShip.blueprint.id.empty()) {
                combatFeedback_ = "敵艦の読み込みに失敗";
                combatFeedbackTimer_ = 2.0f;
                return;
            }
        } else {
            enemyShip = combat_.enemy.content;
            if (enemyShip.blueprint.id.empty()) {
                combatFeedback_ = "敵艦データがありません";
                combatFeedbackTimer_ = 2.0f;
                return;
            }
        }

        if (!combat_.load(content_, enemyShip)) {
            combatFeedback_ = "戦闘状態の初期化に失敗";
            combatFeedbackTimer_ = 2.0f;
            return;
        }

        // Combat owns a working copy while the player is in the combat scene.
        // The persistent ship remains the source of truth between encounters.
        combat_.player = runtime_;
        if (sector_ >= 7 && flagshipPhase_ > 0)
            combat_.configureFlagshipPhase(flagshipPhase_);
        combat_.setPlayerWeaponCooldownMultiplier(weaponCooldownMultiplier());
        discoverRoomTextures();
        discoverWeaponAndDroneTextures();
        discoverCrewTextures();
        combatMode_ = true;
        jumpCharging_ = false;
        jumpCharge_ = 0.0f;
        sceneMode_ = SceneMode::Combat;
        combatTargetRoom_ = combat_.enemy.content.layout.rooms.empty()
            ? 0 : combat_.enemy.content.layout.rooms.front().id;
        combat_.setTargetRoom(combatTargetRoom_);
    }

    std::string selectSectorEvent(const SectorDefinition& sector, int beacon) {
        if (sector.events.empty()) return sector.startEvent;

        // In the original data, min/max describe how often an event may appear
        // within the sector. Track selections during the current sector so we
        // do not repeatedly pick an event that has already reached its maximum.
        std::vector<std::size_t> candidates;
        std::vector<std::size_t> minimums;
        for (std::size_t i = 0; i < sector.events.size(); ++i) {
            const auto& entry = sector.events[i];
            const int used = sectorEventUsage_[entry.name];
            const bool unlimited = entry.max <= 0;
            if (unlimited || used < entry.max) {
                candidates.push_back(i);
                if (used < entry.min) minimums.push_back(i);
            }
        }
        if (!minimums.empty()) candidates = minimums;
        if (candidates.empty()) {
            for (std::size_t i = 0; i < sector.events.size(); ++i)
                candidates.push_back(i);
        }

        std::uint32_t hash = seed_ ^ (static_cast<std::uint32_t>(sector_) * 0x9e3779b9u)
            ^ (static_cast<std::uint32_t>(visitedBeacons_ + 1) * 0x85ebca6bu)
            ^ static_cast<std::uint32_t>(beacon * 0xc2b2ae35u);
        hash ^= hash >> 16;
        hash *= 0x7feb352du;
        hash ^= hash >> 15;
        const auto index = candidates[static_cast<std::size_t>(hash % candidates.size())];
        ++sectorEventUsage_[sector.events[index].name];
        return sector.events[index].name;
    }

    bool beginBeaconEvent(int beacon) {
        if (eventDatabase_.size() == 0) return false;
        // Prefer the original sectorDescription event pools; fall back to the
        // loaded event table when a data file is unavailable.
        if (eventOrder_.empty()) return false;
        activeEventId_.clear();
        if (const auto* sector = sectorDatabase_.select(sector_, static_cast<std::size_t>(beacon))) {
            if (!sector->events.empty())
                activeEventId_ = selectSectorEvent(*sector, beacon);
            if (activeEventId_.empty()) activeEventId_ = sector->startEvent;
        }
        if (activeEventId_.empty())
            activeEventId_ = eventOrder_[static_cast<std::size_t>((sector_ * 5 + beacon) % eventOrder_.size())];
        const auto* event = eventDatabase_.resolve(activeEventId_, seed_ + static_cast<unsigned>(visitedBeacons_) * 53u);
        if (!event) return false;
        // A sector event pool resolves to a concrete event; keep that concrete
        // id so update/render operate on the same definition.
        activeEventId_ = event->id;
        completeQuestForEvent(activeEventId_);
        applyEventImmediateEffects(*event);
        registerQuest(*event);
        activeEventChoice_ = 0;
        sceneMode_ = SceneMode::Event;
        return true;
    }

    int rollEventRange(int minimum, int maximum, std::uint32_t salt) const {
        if (maximum <= minimum) return minimum;
        std::uint32_t value = seed_ ^ (static_cast<std::uint32_t>(visitedBeacons_) * 0x9e3779b9u) ^ salt;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        const std::uint32_t span = static_cast<std::uint32_t>(maximum - minimum + 1);
        return minimum + static_cast<int>(value % span);
    }

    void registerQuest(const EventDefinition& event) {
        if (!event.questId.empty()) {
            if (std::find(activeQuestIds_.begin(), activeQuestIds_.end(), event.questId) == activeQuestIds_.end())
                activeQuestIds_.push_back(event.questId);
        }
        if (!event.questTargetId.empty()) {
            questTargets_[event.questId.empty() ? event.id : event.questId] = event.questTargetId;
        }
    }

    void completeQuestForEvent(const std::string& eventId) {
        for (auto it = questTargets_.begin(); it != questTargets_.end();) {
            if (it->second == eventId) {
                activeQuestIds_.erase(std::remove(activeQuestIds_.begin(), activeQuestIds_.end(), it->first), activeQuestIds_.end());
                it = questTargets_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void applyEventImmediateEffects(const EventDefinition& event) {
        scrap_ = std::max(0, scrap_ + applyScrapAugments(rollEventRange(event.initialScrap, event.initialScrapMax, 0x11u)));
        fuel_ = std::max(0, fuel_ + rollEventRange(event.initialFuel, event.initialFuelMax, 0x23u));
        runtime_.missiles = std::max(0, runtime_.missiles + rollEventRange(event.initialMissiles, event.initialMissilesMax, 0x37u));
        combat_.player.missiles = runtime_.missiles;
        droneParts_ = std::max(0, droneParts_ + rollEventRange(event.initialDrones, event.initialDronesMax, 0x49u));
    }

    bool eventChoiceAvailable(const EventChoice& choice) const {
        if (choice.requirement.empty()) return true;
        std::string req = choice.requirement;
        std::transform(req.begin(), req.end(), req.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        // Crew-race Blue Options: the original event data uses the race name
        // as the requirement (engi, mantis, zoltan, rock, slug, crystal, etc.).
        for (const auto& crew : runtime_.crew) {
            if (!crew.alive) continue;
            std::string race = crew.race;
            std::transform(race.begin(), race.end(), race.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (race == req) return true;
        }

        // System-level requirements such as req="doors" lvl="3".
        // FTL data uses a few historical aliases for the same system; map
        // those aliases to the runtime's canonical type before comparing.
        const auto canonicalSystem = [](const std::string& value) {
            if (value == "piloting" || value == "pilot") return std::string("piloting");
            if (value == "engines" || value == "engine") return std::string("engines");
            if (value == "shields" || value == "shield") return std::string("shields");
            if (value == "weapons" || value == "weapon") return std::string("weapons");
            if (value == "doors" || value == "door") return std::string("doors");
            if (value == "oxygen" || value == "o2") return std::string("oxygen");
            if (value == "medbay" || value == "med") return std::string("medbay");
            if (value == "clonebay" || value == "clone_bay") return std::string("clonebay");
            if (value == "teleporter" || value == "teleport") return std::string("teleporter");
            if (value == "cloaking" || value == "cloak") return std::string("cloaking");
            if (value == "hacking" || value == "hack") return std::string("hacking");
            if (value == "mind" || value == "mindcontrol" || value == "mind_control") return std::string("mind");
            if (value == "artillery" || value == "artillerybeam") return std::string("artillery");
            if (value == "battery" || value == "backupbattery") return std::string("battery");
            if (value == "sensors" || value == "sensor") return std::string("sensors");
            return value;
        };
        req = canonicalSystem(req);
        for (const auto& system : runtime_.systems) {
            std::string type = system.type;
            std::transform(type.begin(), type.end(), type.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            type = canonicalSystem(type);
            if (type == req && system.level >= choice.requirementLevel &&
                system.damage < system.maxPower) return true;
        }

        // Weapon/drone/augmentation-style requirements. Blueprint identifiers
        // are compared case-insensitively so the original XML can be reused.
        for (const auto& weapon : runtime_.weapons) {
            std::string name = weapon.name;
            std::transform(name.begin(), name.end(), name.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (name == req || name.find(req) != std::string::npos) return true;
        }
        for (const auto& drone : runtime_.drones) {
            std::string name = drone.name;
            std::transform(name.begin(), name.end(), name.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (name == req || name.find(req) != std::string::npos) return true;
        }
        return false;
    }

    int nextAvailableEventChoice(const EventDefinition& event, int start, int direction) const {
        if (event.choices.empty()) return 0;
        int index = start;
        for (std::size_t n = 0; n < event.choices.size(); ++n) {
            if (index >= 0 && index < static_cast<int>(event.choices.size()) &&
                eventChoiceAvailable(event.choices[static_cast<std::size_t>(index)]))
                return index;
            index += direction;
            if (index < 0) index = static_cast<int>(event.choices.size()) - 1;
            if (index >= static_cast<int>(event.choices.size())) index = 0;
        }
        return start;
    }

    void updateEvent() {
        const auto* event = eventDatabase_.find(activeEventId_);
        if (!event) {
            sceneMode_ = SceneMode::SectorMap;
            return;
        }
        activeEventChoice_ = nextAvailableEventChoice(*event, activeEventChoice_, 1);
        if (input_.pressed(Button::Up) || input_.pressed(Button::Left))
            activeEventChoice_ = nextAvailableEventChoice(*event, activeEventChoice_ - 1, -1);
        if (input_.pressed(Button::Down) || input_.pressed(Button::Right))
            activeEventChoice_ = nextAvailableEventChoice(*event, activeEventChoice_ + 1, 1);
        if (input_.pressed(Button::Circle)) {
            sceneMode_ = SceneMode::SectorMap;
            return;
        }
        if (!input_.pressed(Button::Cross)) return;
        if (event->choices.empty()) {
            if (event->store) {
                openStore();
                return;
            }
            if (event->hostile) {
                enterCombatFromBeacon(event->hostileShipId);
                return;
            }
            if (event->repair) {
                runtime_.hull = std::min(runtime_.content.blueprint.maxHealth, runtime_.hull + 2);
            }
            ++visitedBeacons_;
            sceneMode_ = SceneMode::SectorMap;
            return;
        }

        if (activeEventChoice_ < 0 ||
            activeEventChoice_ >= static_cast<int>(event->choices.size()) ||
            !eventChoiceAvailable(event->choices[static_cast<std::size_t>(activeEventChoice_)]))
            return;

        const auto& choice = event->choices[static_cast<std::size_t>(activeEventChoice_)];
        if (!choice.questId.empty() &&
            std::find(activeQuestIds_.begin(), activeQuestIds_.end(), choice.questId) == activeQuestIds_.end())
            activeQuestIds_.push_back(choice.questId);
        if (!choice.questTargetId.empty()) {
            const std::string questKey = choice.questId.empty() ? activeEventId_ : choice.questId;
            questTargets_[questKey] = choice.questTargetId;
        }
        // Apply all resource modifications from the original event data, not
        // only scrap/fuel. Missiles and drone parts are carried by the combat
        // runtime, so event rewards immediately affect the actual inventory.
        scrap_ = std::max(0, scrap_ + rollEventRange(choice.scrap, choice.scrapMax, 0x61u + static_cast<std::uint32_t>(activeEventChoice_)));
        fuel_ = std::max(0, fuel_ + rollEventRange(choice.fuel, choice.fuelMax, 0x73u + static_cast<std::uint32_t>(activeEventChoice_)));
        // Resource rewards belong to the persistent ship state. Combat copies
        // this state when a fight starts, so updating runtime_ here prevents
        // event rewards from disappearing on the next combat.
        runtime_.missiles = std::max(0, runtime_.missiles + rollEventRange(choice.missiles, choice.missilesMax, 0x89u + static_cast<std::uint32_t>(activeEventChoice_)));
        combat_.player.missiles = runtime_.missiles;
        droneParts_ = std::max(0, droneParts_ + rollEventRange(choice.drones, choice.dronesMax, 0x97u + static_cast<std::uint32_t>(activeEventChoice_)));
        if (choice.load.empty() && !choice.hostile && !choice.store && !choice.repair) {
            ++visitedBeacons_;
            sceneMode_ = SceneMode::SectorMap;
            return;
        }
        if (choice.store || event->store) {
            openStore();
            return;
        }
        if (choice.repair || event->repair) {
            runtime_.hull = std::min(runtime_.content.blueprint.maxHealth, runtime_.hull + 2);
            sceneMode_ = SceneMode::SectorMap;
            ++visitedBeacons_;
            return;
        }
        if (!choice.load.empty()) {
            const auto* next = eventDatabase_.resolve(choice.load, seed_ + static_cast<unsigned>(activeEventChoice_) * 71u + static_cast<unsigned>(visitedBeacons_));
            if (next && next->hostile) {
                enterCombatFromBeacon(next->hostileShipId);
                return;
            }
            if (next) {
                activeEventId_ = next->id;
                applyEventImmediateEffects(*next);
                registerQuest(*next);
                activeEventChoice_ = 0;
                return;
            }
        }
        if (choice.hostile) {
            enterCombatFromBeacon(choice.hostileShipId);
            return;
        }
        ++visitedBeacons_;
        sceneMode_ = SceneMode::SectorMap;
    }

    void renderEvent() {
        const auto* event = eventDatabase_.find(activeEventId_);
        if (!event) return;
        graphics_.fillRect(70.f, 55.f, 820.f, 430.f, {0.06f, 0.075f, 0.105f, 1.f});
        text_.draw(graphics_, "ビーコンイベント", 105.f, 95.f, 24.f, {0.88f, 0.93f, 1.f, 1.f});
        text_.draw(graphics_, "セクター " + std::to_string(sector_ + 1), 735.f, 95.f, 14.f, {0.65f, 0.75f, 0.88f, 1.f});
        std::string message = event->text.empty() ? "このビーコンでは特に何も起きなかった。" : event->text;
        if (message.size() > 110) message.resize(110);
        text_.draw(graphics_, message, 105.f, 150.f, 16.f, {0.82f, 0.86f, 0.92f, 1.f});
        if (event->choices.empty()) {
            text_.draw(graphics_, "×: 続行", 105.f, 410.f, 16.f, {0.95f, 0.82f, 0.42f, 1.f});
        } else {
            for (std::size_t i=0;i<event->choices.size() && i<6;++i) {
                const bool selected = static_cast<int>(i) == activeEventChoice_;
                const float y = 245.f + static_cast<float>(i) * 38.f;
                if (selected)
                    graphics_.fillRect(95.f, y - 20.f, 770.f, 30.f, {0.16f, 0.25f, 0.34f, 1.f});
                const auto& choice = event->choices[i];
                const bool available = eventChoiceAvailable(choice);
                std::string label = choice.text;
                if (label.empty()) label = choice.load.empty() ? "続行" : "次へ";
                if (choice.blue && !choice.requirement.empty()) label = "[青] " + label;
                if (!available && !choice.requirement.empty()) label += "  (条件未達)";
                if (label.size() > 90) label.resize(90);
                const Color normal = available
                    ? Color{0.78f,0.83f,0.90f,1.f}
                    : Color{0.42f,0.45f,0.50f,1.f};
                text_.draw(graphics_, (selected ? "> " : "  ") + label, 110.f, y, 15.f,
                    selected ? (available ? Color{0.98f,0.86f,0.45f,1.f} : Color{0.52f,0.55f,0.60f,1.f}) : normal);
            }
            text_.draw(graphics_, "十字キー: 選択   ×: 決定   ○: 戻る", 105.f, 455.f, 14.f,
                {0.64f,0.72f,0.82f,1.f});
        }
    }

    void updateSectorMap() {
        // FTL advances through a connected beacon map. The current geometry is
        // a compatibility graph for the original FTL sector flow; its encounter data now comes
        // from the original sectorDescription/event XML.
        const auto choices = sectorGraph_.selectable(currentBeacon_, fleetRow_);
        if (!choices.empty()) {
            auto it = std::find(choices.begin(), choices.end(), selectedBeacon_);
            int pos = it == choices.end() ? 0 : static_cast<int>(it - choices.begin());
            if (input_.pressed(Button::Left) || input_.pressed(Button::Up)) pos = std::max(0, pos - 1);
            if (input_.pressed(Button::Right) || input_.pressed(Button::Down)) pos = std::min(static_cast<int>(choices.size()) - 1, pos + 1);
            selectedBeacon_ = choices[static_cast<std::size_t>(pos)];
        }
        if (input_.pressed(Button::Cross) && !choices.empty()) {
            if (fuel_ <= 0) return;
            fuel_--;
            currentBeacon_ = selectedBeacon_;
            if (const auto* n = sectorGraph_.node(currentBeacon_)) {
                if (n->row == sectorGraph_.exitRow()) {
                    if (sector_ >= 7) {
                        // The final sector leads into the Rebel Flagship battle.
                        // The three combat phases are kept as separate encounters
                        // so the player's surviving systems/crew carry forward.
                        if (flagshipPhase_ == 0) {
                            LoadedShip flagship;
                            if (content_.loadShip("BOSS_1", flagship) &&
                                !flagship.blueprint.id.empty()) {
                                flagshipPhase_ = 1;
                                enterCombatFromBeacon("BOSS_1");
                                return;
                            }
                            // If the supplied archive does not contain the
                            // flagship blueprint, retain the old safe fallback.
                            sceneMode_ = SceneMode::Victory;
                            return;
                        }
                        sceneMode_ = SceneMode::Victory;
                        return;
                    }
                    ++sector_;
                    sectorEventUsage_.clear();
                    sectorGraph_.generate(sector_, static_cast<std::uint32_t>(seed_ + sector_));
                    currentBeacon_ = -1;
                    selectedBeacon_ = sectorGraph_.startNode();
                    return;
                }
            }
            if (beginBeaconEvent(currentBeacon_)) return;
            enterCombatFromBeacon();
            return;
        }
        if (input_.pressed(Button::Circle)) {
            sceneMode_ = SceneMode::Ship;
            return;
        }
    }

    void renderSectorMap() {
        graphics_.fillRect(0.f, 0.f, 960.f, 544.f, {0.035f, 0.045f, 0.065f, 1.f});
        text_.draw(graphics_, "FTL: Faster Than Light", 48.f, 42.f, 24.f, {0.88f,0.92f,1.f,1.f});
        text_.draw(graphics_, "セクター " + std::to_string(sector_ + 1) + " / 8", 48.f, 74.f, 15.f, {0.65f,0.75f,0.88f,1.f});
        if (!activeQuestIds_.empty())
            text_.draw(graphics_, "クエスト " + std::to_string(activeQuestIds_.size()), 360.f, 74.f, 14.f, {0.86f,0.78f,0.46f,1.f});
        text_.draw(graphics_, "燃料 " + std::to_string(fuel_) + "   ミサイル " + std::to_string(combat_.player.missiles),
            620.f,42.f,14.f,{0.78f,0.86f,0.94f,1.f});
        text_.draw(graphics_, "スクラップ " + std::to_string(scrap_) +
            "   ドローン " + std::to_string(droneParts_),620.f,66.f,14.f,{0.82f,0.76f,0.58f,1.f});

        const float x0=150.f, dx=105.f, y0=145.f, dy=43.f;
        for (const auto& n : sectorGraph_.nodes()) {
            const float x=x0+n.column*dx, y=y0+n.row*dy;
            for (const int to:n.links) {
                const auto* dst=sectorGraph_.node(to);
                if(dst) graphics_.drawLine(x,y,x0+dst->column*dx,y0+dst->row*dy,{0.20f,0.34f,0.46f,1.f});
            }
        }
        const auto choices=sectorGraph_.selectable(currentBeacon_, fleetRow_);
        for (std::size_t index = 0; index < sectorGraph_.nodes().size(); ++index) {
            const auto& n = sectorGraph_.nodes()[index];
            const float x=x0+n.column*dx,y=y0+n.row*dy;
            const auto* selectedNode = sectorGraph_.node(selectedBeacon_);
            const bool selected = selectedNode && n.row != sectorGraph_.exitRow() && n.row == selectedNode->row && n.column == selectedNode->column;
            const bool current = static_cast<int>(index) == currentBeacon_;
            const bool reachable = std::find(choices.begin(),choices.end(),static_cast<int>(index)) != choices.end();
            const float r=current?8.f:(selected?9.f:6.f);
            graphics_.fillRect(x-r,y-r,r*2.f,r*2.f,current?Color{0.40f,0.90f,0.55f,1.f}:(selected?Color{0.98f,0.75f,0.20f,1.f}:(reachable?Color{0.35f,0.65f,0.85f,1.f}:Color{0.20f,0.30f,0.38f,1.f})));
        }
        text_.draw(graphics_,"十字キー: 接続ビーコン選択   ×: ジャンプ   ○: 戻る",48.f,500.f,14.f,{0.68f,0.76f,0.86f,1.f});
    }

    void updatePause() {
        if (input_.pressed(Button::Triangle)) {
            if (saveGame())
                saveFeedback_ = "セーブしました";
            else
                saveFeedback_ = combatMode_ ? "戦闘中はセーブできません" : "セーブに失敗しました";
        }
        if (input_.pressed(Button::Start) || input_.pressed(Button::Circle))
            sceneMode_ = combatMode_ ? SceneMode::Combat : SceneMode::Ship;
    }

    void renderPause() {
        graphics_.fillRect(260.f, 145.f, 440.f, 250.f, {0.06f,0.08f,0.12f,0.97f});
        graphics_.drawLine(260.f,145.f,700.f,145.f,{0.45f,0.65f,0.82f,1.f});
        text_.draw(graphics_, "ポーズ", 315.f, 205.f, 28.f, {0.90f,0.94f,1.f,1.f});
        text_.draw(graphics_, "スタート: 再開", 315.f, 255.f, 17.f, {0.75f,0.82f,0.92f,1.f});
        text_.draw(graphics_, "○: 再開", 315.f, 290.f, 17.f, {0.75f,0.82f,0.92f,1.f});
        text_.draw(graphics_, "△: セーブ", 315.f, 325.f, 17.f, {0.75f,0.82f,0.92f,1.f});
        text_.draw(graphics_, "現在のセクター: " + std::to_string(sector_ + 1), 315.f, 360.f, 15.f,
            {0.65f,0.72f,0.82f,1.f});
        if (!saveFeedback_.empty())
            text_.draw(graphics_, saveFeedback_, 315.f, 395.f, 14.f,
                {1.f,0.84f,0.45f,1.f});
    }

    void update(float dt) override {
        if (!startupError_.empty()) return;

        if (input_.pressed(Button::Start) && sceneMode_ != SceneMode::Pause) {
            sceneMode_ = SceneMode::Pause;
            saveFeedback_.clear();
            return;
        }
        if (sceneMode_ == SceneMode::Pause) {
            updatePause();
            return;
        }
        if (sceneMode_ == SceneMode::ShipSelect) {
            updateShipSelect();
            return;
        }
        if (sceneMode_ == SceneMode::SectorMap) {
            updateSectorMap();
            return;
        }
        if (sceneMode_ == SceneMode::Event) {
            updateEvent();
            return;
        }
        if (storeOpen_) {
            updateStore(dt);
            return;
        }
        if (sceneMode_ == SceneMode::GameOver || sceneMode_ == SceneMode::Victory) return;

        if (combatMode_) {
            combat_.update(dt);
            CombatResult impact;
            while (combat_.consumeImpactResult(impact)) {
                if (impact.evaded > 0) {
                    combatFeedback_ = "攻撃を回避";
                } else if (impact.firesStarted > 0) {
                    combatFeedback_ = "火災発生";
                } else if (impact.breachesStarted > 0) {
                    combatFeedback_ = "船体に亀裂";
                } else if (impact.systemsStunned > 0) {
                    combatFeedback_ = "システムをスタン";
                } else if (impact.shieldsAbsorbed > 0 && impact.hullDamage == 0) {
                    combatFeedback_ = "シールドが攻撃を吸収";
                } else if (impact.targetDestroyed) {
                    combatFeedback_ = "敵艦撃沈";
                } else if (impact.hullDamage > 0) {
                    combatFeedback_ = "船体ダメージ " + std::to_string(impact.hullDamage);
                } else if (impact.ionDamage > 0) {
                    combatFeedback_ = "イオンダメージ " + std::to_string(impact.ionDamage);
                } else if (impact.systemDamage > 0) {
                    combatFeedback_ = "システムダメージ " + std::to_string(impact.systemDamage);
                } else if (impact.personnelDamage > 0) {
                    combatFeedback_ = "クルーダメージ " + std::to_string(impact.personnelDamage);
                } else if (impact.fired) {
                    combatFeedback_ = "攻撃命中";
                }
                combatFeedbackTimer_ = 1.4f;
            }
            combatFeedbackTimer_ = std::max(0.0f, combatFeedbackTimer_ - dt);
            updateCombat();
            if (combat_.outcome == CombatOutcome::EnemyDestroyed) {
                // Persist all combat-side changes, not just hull damage:
                // systems, crew, weapons, missiles, shields, fires and breaches
                // must survive the return to the ship scene.
                runtime_ = combat_.player;
                scrap_ += applyScrapAugments(20 + sector_ * 5);

                if (sector_ >= 7 && flagshipPhase_ > 0 && flagshipPhase_ < 3) {
                    // Flagship phase transition: keep the damaged player ship,
                    // but load a fresh flagship for the next phase.
                    ++flagshipPhase_;
                    combatFeedback_ = "反乱軍旗艦 Phase " + std::to_string(flagshipPhase_);
                    combatFeedbackTimer_ = 2.0f;
                    LoadedShip nextFlagship;
                    const std::string nextId = flagshipPhase_ == 2 ? "BOSS_2" : "BOSS_3";
                    if (content_.loadShip(nextId, nextFlagship) &&
                        combat_.loadFlagshipPhase(content_, nextFlagship, combat_.enemy.crew)) {
                        combat_.player = runtime_;
                        combat_.configureFlagshipPhase(flagshipPhase_);
                        combatMode_ = true;
                        jumpCharging_ = false;
                        jumpCharge_ = 0.0f;
                        sceneMode_ = SceneMode::Combat;
                        combatTargetRoom_ = combat_.enemy.content.layout.rooms.empty()
                            ? 0 : combat_.enemy.content.layout.rooms.front().id;
                        combat_.setTargetRoom(combatTargetRoom_);
                        discoverRoomTextures();
                        discoverWeaponAndDroneTextures();
                        discoverCrewTextures();
                    } else {
                        combatFeedback_ = "旗艦次フェーズの読み込みに失敗";
                        combatFeedbackTimer_ = 2.0f;
                        combatMode_ = false;
                        sceneMode_ = SceneMode::GameOver;
                    }
                } else if (sector_ >= 7 && flagshipPhase_ >= 3) {
                    combatMode_ = false;
                    sceneMode_ = SceneMode::Victory;
                } else {
                    combatMode_ = false;
                    visitedBeacons_++;
                    sceneMode_ = SceneMode::SectorMap;
                }
            } else if (combat_.outcome == CombatOutcome::PlayerDestroyed) {
                combatMode_ = false;
                sceneMode_ = SceneMode::GameOver;
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
        // Start is reserved for the global pause handled at the top of
        // update(). Use Circle for deterministic system power-down instead.
        if (input_.pressed(Button::Circle) && !runtime_.systems.empty()) {
            for (int i = 0; i < static_cast<int>(runtime_.systems.size()); ++i) {
                if (runtime_.systems[i].room == selectedRoomId) {
                    runtime_.setSystemPower(i, runtime_.systems[i].power - 1);
                    break;
                }
            }
        }

        if (input_.pressed(Button::Select)) {
            sceneMode_ = SceneMode::SectorMap;
            return;
        }

        if (input_.pressed(Button::L) && !runtime_.crew.empty()) {
            runtime_.moveCrew(selectedCrew_, selectedRoomId);
        }
        if (input_.pressed(Button::Square)) {
            for (int i = 0; i < static_cast<int>(runtime_.content.layout.doors.size()); ++i) {
                const auto& door = runtime_.content.layout.doors[i];
                if (door.leftRoom == selectedRoomId || door.rightRoom == selectedRoomId) {
                    runtime_.setDoorOpen(i, !runtime_.doorOpen[i]);
                    break;
                }
            }
        }
    }

    void updateCombat() {
        if (!combat_.enemy.valid || combat_.enemy.content.layout.rooms.empty()) {
            combatMode_ = false;
            return;
        }

        const auto& enemyRooms = combat_.enemy.content.layout.rooms;
        const int roomCount = static_cast<int>(enemyRooms.size());

        // During combat the player can also manage crew, doors and system power
        // without leaving the battle view. selectedRoom_ points at a real player
        // room, while selectedCrew_ identifies the crew member being commanded.
        bool powerCommand = false;
        if (!combat_.player.systems.empty()) {
            selectedSystem_ = std::clamp(selectedSystem_, 0,
                static_cast<int>(combat_.player.systems.size()) - 1);
            if (input_.pressed(Button::Circle) && input_.down(Button::Left)) {
                selectedSystem_ = (selectedSystem_ + static_cast<int>(combat_.player.systems.size()) - 1) %
                    static_cast<int>(combat_.player.systems.size());
                powerCommand = true;
            } else if (input_.pressed(Button::Circle) && input_.down(Button::Right)) {
                selectedSystem_ = (selectedSystem_ + 1) %
                    static_cast<int>(combat_.player.systems.size());
                powerCommand = true;
            } else if (input_.pressed(Button::Circle) && input_.down(Button::Up)) {
                powerCommand = true;
                auto& system = combat_.player.systems[selectedSystem_];
                if (!combat_.player.setSystemPower(selectedSystem_, system.power + 1))
                    combatFeedback_ = "電力が足りない";
                else
                    combatFeedback_ = systemLabel(system) + "へ電力+1";
                combatFeedbackTimer_ = 1.0f;
            } else if (input_.pressed(Button::Circle) && input_.down(Button::Down)) {
                powerCommand = true;
                auto& system = combat_.player.systems[selectedSystem_];
                if (!combat_.player.setSystemPower(selectedSystem_, system.power - 1))
                    combatFeedback_ = system.power <= 0 ? "これ以上下げられない" : "電力を変更できない";
                else
                    combatFeedback_ = systemLabel(system) + "の電力-1";
                combatFeedbackTimer_ = 1.0f;
            }
        }

        const auto& playerRooms = combat_.player.content.layout.rooms;
        if (!playerRooms.empty()) {
            const int playerRoomCount = static_cast<int>(playerRooms.size());
            selectedRoom_ = std::clamp(selectedRoom_, 0, playerRoomCount - 1);
            if (input_.pressed(Button::Triangle) && !combat_.player.crew.empty())
                selectedCrew_ = (selectedCrew_ + 1) % static_cast<int>(combat_.player.crew.size());
            if (!powerCommand && input_.pressed(Button::Up)) {
                selectedRoom_ = (selectedRoom_ + playerRoomCount - 1) % playerRoomCount;
            } else if (!powerCommand && input_.pressed(Button::Down)) {
                selectedRoom_ = (selectedRoom_ + 1) % playerRoomCount;
            }
            const int selectedPlayerRoomId = playerRooms[selectedRoom_].id;
            if (input_.pressed(Button::L) && !combat_.player.crew.empty()) {
                RuntimeCrew& crew = combat_.player.crew[selectedCrew_];
                if (crew.alive && crew.room == selectedPlayerRoomId) {
                    if (selectedPlayerRoomId >= 0 &&
                        selectedPlayerRoomId < static_cast<int>(combat_.player.roomFire.size()) &&
                        combat_.player.roomFire[selectedPlayerRoomId]) {
                        if (combat_.player.extinguishFire(selectedCrew_))
                            combatFeedback_ = crewLabel(crew) + "が消火";
                        else
                            combatFeedback_ = "消火できない";
                    } else {
                        const int repaired = combat_.player.repairSystemInRoom(selectedPlayerRoomId, 1);
                        combatFeedback_ = repaired > 0
                            ? crewLabel(crew) + "がシステムを修理"
                            : "修理するシステムがない";
                    }
                    combatFeedbackTimer_ = 1.0f;
                } else if (!combat_.player.moveCrew(selectedCrew_, selectedPlayerRoomId)) {
                    combatFeedback_ = "クルーはその部屋へ移動できない";
                    combatFeedbackTimer_ = 1.2f;
                } else {
                    combatFeedback_ = crewLabel(crew) + "を移動";
                    combatFeedbackTimer_ = 1.0f;
                }
            }
            if (input_.pressed(Button::Square)) {
                for (int i = 0; i < static_cast<int>(combat_.player.content.layout.doors.size()); ++i) {
                    const auto& door = combat_.player.content.layout.doors[i];
                    if (door.leftRoom == selectedPlayerRoomId || door.rightRoom == selectedPlayerRoomId) {
                        if (combat_.player.setDoorOpen(i, !combat_.player.doorOpen[i]))
                            combatFeedback_ = combat_.player.doorOpen[i] ? "ドアを開いた" : "ドアを閉じた";
                        combatFeedbackTimer_ = 1.0f;
                        break;
                    }
                }
            }
        }

        // Room IDs are data identifiers, not guaranteed to be contiguous indices.
        // Cycle through the actual room list so targeting always points at a
        // real enemy room, including archives with sparse/non-zero IDs.
        int targetIndex = 0;
        for (int i = 0; i < roomCount; ++i) {
            if (enemyRooms[i].id == combatTargetRoom_) {
                targetIndex = i;
                break;
            }
        }
        if (!powerCommand && input_.pressed(Button::Left))
            targetIndex = (targetIndex + roomCount - 1) % roomCount;
        if (!powerCommand && input_.pressed(Button::Right))
            targetIndex = (targetIndex + 1) % roomCount;
        combatTargetRoom_ = enemyRooms[targetIndex].id;

        // L is reserved for the selected crew action; weapon cycling uses
        // Triangle+L/R so crew commands cannot silently change the weapon.
        if (input_.pressed(Button::Triangle) && input_.down(Button::L) &&
            !combat_.player.weapons.empty())
            combat_.selectedWeapon = (combat_.selectedWeapon +
                static_cast<int>(combat_.player.weapons.size()) - 1) %
                static_cast<int>(combat_.player.weapons.size());
        if (input_.pressed(Button::Triangle) && input_.down(Button::R) &&
            !combat_.player.weapons.empty())
            combat_.selectedWeapon = (combat_.selectedWeapon + 1) %
                static_cast<int>(combat_.player.weapons.size());

        combat_.setTargetRoom(combatTargetRoom_);
        if (input_.pressed(Button::Cross))
            lastCombatResult_ = combat_.fireSelectedWeapon();

        // FTL retreat: the player must charge the FTL drive before leaving
        // combat. Fuel is consumed when the retreat jump is completed.
        if (combat_.outcome == CombatOutcome::Ongoing && !powerCommand && input_.pressed(Button::Circle)) {
            if (!jumpCharging_) {
                if (fuel_ > 0) {
                    jumpCharging_ = true;
                    jumpCharge_ = 0.0f;
                    combatFeedback_ = "FTLジャンプ充電開始";
                    combatFeedbackTimer_ = 1.4f;
                } else {
                    combatFeedback_ = "燃料がない";
                    combatFeedbackTimer_ = 1.4f;
                }
            } else {
                jumpCharging_ = false;
                jumpCharge_ = 0.0f;
                combatFeedback_ = "FTLジャンプを中止";
                combatFeedbackTimer_ = 1.4f;
            }
        }

        if (jumpCharging_ && combat_.outcome == CombatOutcome::Ongoing) {
            constexpr float jumpChargeTime = 10.0f;
            jumpCharge_ = std::min(jumpChargeTime, jumpCharge_ + 1.0f / 60.0f);
            if (jumpCharge_ >= jumpChargeTime) {
                --fuel_;
                runtime_ = combat_.player;
                combatMode_ = false;
                jumpCharging_ = false;
                jumpCharge_ = 0.0f;
                visitedBeacons_++;
                combatFeedback_.clear();
                combatFeedbackTimer_ = 0.0f;
                sceneMode_ = SceneMode::SectorMap;
                return;
            }
        }
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
            constexpr float shieldRechargeSeconds = 2.0f;
            const float charge = std::clamp(
                ship.shieldCharge / shieldRechargeSeconds, 0.f, 1.f);
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

        // Highlight the exact enemy room selected by the combat runtime.
        for (const auto& room : combat_.enemy.content.layout.rooms) {
            if (room.id != combatTargetRoom_) continue;
            const float x = rightX + room.x * scale;
            const float y = originY + room.y * scale;
            const float w = std::max(1, room.w) * scale;
            const float h = std::max(1, room.h) * scale;
            const float corner = 12.f;
            graphics_.drawLine(x, y, x + std::min(w, corner), y, {1.f, 0.82f, 0.25f, 1.f});
            graphics_.drawLine(x, y, x, y + std::min(h, corner), {1.f, 0.82f, 0.25f, 1.f});
            graphics_.drawLine(x + w, y + h, x + w - std::min(w, corner), y + h,
                {1.f, 0.82f, 0.25f, 1.f});
            graphics_.drawLine(x + w, y + h, x + w, y + h - std::min(h, corner),
                {1.f, 0.82f, 0.25f, 1.f});
            break;
        }

        if (combat_.selectedWeapon >= 0 &&
            combat_.selectedWeapon < static_cast<int>(combat_.player.weapons.size())) {
            const auto& weapon = combat_.player.weapons[combat_.selectedWeapon];
            text_.draw(graphics_, weaponLabel(weapon), leftX, 152.f, 12.f,
                weapon.ready ? Color{1.f, 0.88f, 0.45f, 1.f}
                             : Color{0.72f, 0.82f, 0.92f, 1.f});
        }

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
                const float health = crew.maxHealth > 0
                    ? std::clamp(static_cast<float>(crew.health) / crew.maxHealth, 0.f, 1.f) : 0.f;
                graphics_.fillRect(x - 8.f, y + 10.f, 16.f, 3.f, {0.12f, 0.12f, 0.15f, 1.f});
                if (health > 0.f)
                    graphics_.fillRect(x - 8.f, y + 10.f, 16.f * health, 3.f,
                        health > 0.5f ? Color{0.25f, 0.85f, 0.45f, 1.f}
                                      : (health > 0.25f ? Color{0.95f, 0.72f, 0.2f, 1.f}
                                                        : Color{0.9f, 0.25f, 0.2f, 1.f}));
                ++crewSlot;
            }
        };

        drawRuntimeArtwork(combat_.player, leftX);
        drawRuntimeArtwork(combat_.enemy, rightX);

        // Boarders are rendered on the player's ship in a distinct hostile marker.
        int boarderSlot = 0;
        for (const auto& boarder : combat_.boarders) {
            if (!boarder.alive) continue;
            const auto center = roomCenter(combat_.player, leftX, boarder.room);
            const auto textureIt = crewTextureNames_.find(boarder.race);
            const Texture* texture = textureIt == crewTextureNames_.end()
                ? nullptr : textures_.get(textureIt->second);
            const float x = center.first + (boarderSlot % 2) * 16.f;
            const float y = center.second + (boarderSlot / 2) * 16.f;
            if (texture && texture->width() > 0 && texture->height() > 0) {
                const float w = 12.f;
                const float h = std::min(18.f, w * static_cast<float>(texture->height()) / texture->width());
                graphics_.drawTexture(*texture, x - w * 0.5f, y - h * 0.5f, w, h,
                    {1.f, 0.45f, 0.40f, 1.f});
            } else {
                graphics_.fillRect(x - 4.f, y - 4.f, 8.f, 8.f, {0.9f, 0.2f, 0.18f, 1.f});
            }
            graphics_.drawLine(x - 7.f, y - 7.f, x + 7.f, y - 7.f, {1.f, 0.25f, 0.2f, 1.f});
            graphics_.drawLine(x - 7.f, y + 7.f, x + 7.f, y + 7.f, {1.f, 0.25f, 0.2f, 1.f});
            ++boarderSlot;
        }

        // Combat crew-management cursor: this is deliberately separate from
        // the enemy targeting cursor so crew commands do not change the shot target.
        if (!combat_.player.content.layout.rooms.empty()) {
            const int roomIndex = std::clamp(selectedRoom_, 0,
                static_cast<int>(combat_.player.content.layout.rooms.size()) - 1);
            const auto& room = combat_.player.content.layout.rooms[roomIndex];
            const float x = leftX + room.x * scale;
            const float y = originY + room.y * scale;
            const float w = std::max(1, room.w) * scale;
            const float h = std::max(1, room.h) * scale;
            graphics_.drawLine(x, y, x + std::min(w, 10.f), y, {0.45f, 1.f, 0.55f, 1.f});
            graphics_.drawLine(x, y, x, y + std::min(h, 10.f), {0.45f, 1.f, 0.55f, 1.f});
            graphics_.drawLine(x + w, y + h, x + w - std::min(w, 10.f), y + h,
                {0.45f, 1.f, 0.55f, 1.f});
            graphics_.drawLine(x + w, y + h, x + w, y + h - std::min(h, 10.f),
                {0.45f, 1.f, 0.55f, 1.f});
        }

        // Show system damage directly on the room containing the damaged system.
        // This keeps combat feedback tied to the same runtime values used by
        // damage resolution, rather than adding a separate visual-only state.
        auto drawSystemDamage = [&](const ShipRuntime& ship, float originX) {
            for (const auto& system : ship.systems) {
                if (system.room < 0 || system.maxPower <= 0 || system.damage <= 0) continue;
                const auto center = roomCenter(ship, originX, system.room);
                const float ratio = std::clamp(
                    static_cast<float>(system.damage) / system.maxPower, 0.f, 1.f);
                graphics_.fillRect(center.first - 12.f, center.second + 12.f, 24.f, 4.f,
                    {0.12f, 0.10f, 0.10f, 0.95f});
                graphics_.fillRect(center.first - 12.f, center.second + 12.f, 24.f * ratio, 4.f,
                    {0.95f, 0.28f, 0.18f, 0.95f});
            }
        };
        drawSystemDamage(combat_.player, leftX);
        drawSystemDamage(combat_.enemy, rightX);

        const float playerHull = combat_.player.maxHull > 0
            ? static_cast<float>(combat_.player.hull) / combat_.player.maxHull : 0.f;
        const float enemyHull = combat_.enemy.maxHull > 0
            ? static_cast<float>(combat_.enemy.hull) / combat_.enemy.maxHull : 0.f;
        graphics_.fillRect(leftX, 100.f, 280.f, 12.f, {0.15f, 0.15f, 0.15f, 1.f});
        graphics_.fillRect(leftX, 100.f, 280.f * playerHull, 12.f, {0.2f, 0.8f, 0.35f, 1.f});
        graphics_.fillRect(rightX, 100.f, 280.f, 12.f, {0.15f, 0.15f, 0.15f, 1.f});
        graphics_.fillRect(rightX, 100.f, 280.f * enemyHull, 12.f, {0.85f, 0.25f, 0.25f, 1.f});
        if (sector_ >= 7 && flagshipPhase_ > 0)
            text_.draw(graphics_, "反乱軍旗艦 Phase " + std::to_string(flagshipPhase_) + " / 3",
                390.f, 86.f, 12.f, {0.98f, 0.70f, 0.36f, 1.f});
        if (combat_.superShieldRemaining() > 0)
            text_.draw(graphics_, "Phase 3: Zoltan超シールド",
                390.f, 102.f, 11.f, {0.55f, 0.88f, 1.f, 1.f});
        text_.draw(graphics_, "船体 " + std::to_string(std::max(0, combat_.player.hull)) +
            "/" + std::to_string(std::max(0, combat_.player.maxHull)),
            leftX, 86.f, 11.f, {0.72f, 0.92f, 0.78f, 1.f});
        text_.draw(graphics_, "敵船体 " + std::to_string(std::max(0, combat_.enemy.hull)) +
            "/" + std::to_string(std::max(0, combat_.enemy.maxHull)),
            rightX, 86.f, 11.f, {0.96f, 0.72f, 0.72f, 1.f});

        for (int i = 0; i < combat_.enemy.shieldLayers; ++i)
            graphics_.fillRect(rightX + i * 14.f, 125.f, 10.f, 6.f, {0.25f, 0.65f, 0.95f, 1.f});
        if (combat_.superShieldRemaining() > 0) {
            const int superShield = combat_.superShieldRemaining();
            text_.draw(graphics_, "超シールド " + std::to_string(superShield) + " / 10",
                rightX, 116.f, 10.f, {0.62f, 0.90f, 1.f, 1.f});
            for (int i = 0; i < superShield; ++i)
                graphics_.fillRect(rightX + i * 13.f, 132.f, 10.f, 5.f,
                    {0.55f, 0.90f, 1.f, 1.f});
        }

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

        text_.draw(graphics_, "←→: 敵ターゲット   ↑↓: クルー部屋   △: クルー   △+L/R: 武器   □: ドア   ○: FTL",
            leftX, 448.f, 11.f, {0.62f, 0.74f, 0.86f, 1.0f});
        if (!combat_.player.systems.empty()) {
            const int systemIndex = std::clamp(selectedSystem_, 0,
                static_cast<int>(combat_.player.systems.size()) - 1);
            const auto& system = combat_.player.systems[systemIndex];
            text_.draw(graphics_, "電力対象: " + systemLabel(system) + " " +
                std::to_string(system.power) + "/" + std::to_string(system.maxPower) +
                "   ○+←→:対象   ○+↑↓:電力",
                leftX, 464.f, 10.f, {0.68f, 0.80f, 0.90f, 1.0f});
        }

        if (jumpCharging_) {
            constexpr float jumpChargeTime = 10.0f;
            const float ratio = std::clamp(jumpCharge_ / jumpChargeTime, 0.0f, 1.0f);
            text_.draw(graphics_, "○ FTLジャンプ充電中", leftX, 470.f, 13.f,
                {0.55f, 0.82f, 1.0f, 1.0f});
            graphics_.fillRect(leftX, 488.f, 280.f, 8.f, {0.12f, 0.14f, 0.18f, 1.f});
            graphics_.fillRect(leftX, 488.f, 280.f * ratio, 8.f,
                {0.35f, 0.78f, 1.0f, 1.0f});
            text_.draw(graphics_, std::to_string(static_cast<int>(jumpCharge_)) + " / 10秒",
                leftX + 290.f, 494.f, 11.f, {0.65f, 0.78f, 0.90f, 1.0f});
        } else {
            text_.draw(graphics_, "○ FTLジャンプ", leftX, 488.f, 13.f,
                {0.55f, 0.68f, 0.80f, 1.0f});
        }

        if (combatFeedbackTimer_ > 0.0f && !combatFeedback_.empty()) {
            text_.draw(graphics_, combatFeedback_, 360.f, 505.f, 14.f,
                {1.f, 0.88f, 0.52f, 1.f});
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
        if (sceneMode_ == SceneMode::ShipSelect) {
            renderShipSelect();
            return;
        }
        if (sceneMode_ == SceneMode::SectorMap) {
            renderSectorMap();
            return;
        }
        if (sceneMode_ == SceneMode::Event) {
            renderEvent();
            return;
        }
        if (storeOpen_) {
            renderStore();
            return;
        }
        if (sceneMode_ == SceneMode::Pause) {
            if (combatMode_) renderCombat();
            else {
                const LoadedShip* ship = content_.playerShip();
                (void)ship;
            }
            renderPause();
            return;
        }
        if (sceneMode_ == SceneMode::GameOver || sceneMode_ == SceneMode::Victory) {
            graphics_.fillRect(0.f,0.f,960.f,544.f,{0.03f,0.04f,0.06f,1.f});
            text_.draw(graphics_, sceneMode_ == SceneMode::Victory ? "銀河を脱出した" : "ゲームオーバー",
                285.f,230.f,30.f, sceneMode_ == SceneMode::Victory ? Color{0.9f,0.85f,0.45f,1.f} : Color{0.95f,0.35f,0.30f,1.f});
            text_.draw(graphics_, "スタートで終了", 385.f,285.f,16.f,{0.72f,0.78f,0.88f,1.f});
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
    EventDatabase eventDatabase_{content_.assets()};
    SectorDatabase sectorDatabase_{content_.assets()};
    SectorGraph sectorGraph_;
    ShipRuntime runtime_;
    CombatRuntime combat_;
    CombatResult lastCombatResult_{};
    bool combatMode_{false};
    int combatTargetRoom_{0};
    int selectedRoom_{0};
    int selectedCrew_{0};
    int selectedSystem_{0};
    TextureCache textures_;
    TextRenderer text_;
    std::unordered_map<int, std::string> roomTextureNames_;
    std::unordered_map<int, std::string> enemyRoomTextureNames_;
    std::unordered_map<std::string, std::string> weaponTextureNames_;
    std::unordered_map<std::string, std::string> droneTextureNames_;
    std::unordered_map<std::string, std::string> crewTextureNames_;
    std::string shipTextureName_;
    std::string startupError_;
    std::string combatFeedback_;
    std::string saveFeedback_;
    float combatFeedbackTimer_{0.0f};
    float jumpCharge_{0.0f};
    bool jumpCharging_{false};
    SceneMode sceneMode_{SceneMode::SectorMap};
    std::vector<std::string> eventOrder_;
    std::string activeEventId_;
    int activeEventChoice_{0};
    int sector_{0};
    int flagshipPhase_{0};
    int selectedBeacon_{0};
    int currentBeacon_{-1};
    int fleetRow_{-1};
    unsigned seed_{0x51f7a21u};
    int visitedBeacons_{0};
    int fuel_{16};
    int scrap_{0};
    int droneParts_{0};
    std::vector<std::string> shipChoices_;
    int shipSelection_{0};
    std::vector<std::string> activeQuestIds_;
    std::vector<std::string> augmentIds_;
    std::unordered_map<std::string, std::string> questTargets_;
    std::unordered_map<std::string, int> sectorEventUsage_;
    std::vector<StoreOffer> storeOffers_;
    int storeSelection_{0};
    bool storeOpen_{false};
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
