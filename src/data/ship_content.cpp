#include "data/ship_content.hpp"
#include <algorithm>
#include <cstdint>
#include <functional>\n#include <set>\n#include <string>

namespace wormhole {

bool ShipContent::open(const std::string& archivePath) {
    loaded_ = false;
    database_.clear();
    return assets_.openArchive(archivePath);
}

bool ShipContent::openArchives(const std::vector<std::string>& archivePaths) {
    loaded_ = false;
    database_.clear();
    return assets_.openArchives(archivePaths);
}

bool ShipContent::loadShip(const std::string& shipId, LoadedShip& out,
                             const std::string& blueprintPath,
                             unsigned randomSeed) {
    database_.clear();
    out = {};

    // autoBlueprints.xml is part of the canonical base ftl.dat and contains
    // the generated/enemy ship definitions used by normal gameplay.
    std::vector<std::string> sources{blueprintPath, "data/autoBlueprints.xml"};
    if (advancedEdition_) {
        sources.push_back("data/dlcBlueprints.xml");
        sources.push_back("data/dlcBlueprintsOverwrite.xml");
        sources.push_back("data/dlcPirateBlueprints.xml");
    }
    if (database_.loadShipBlueprints(sources) == 0) return false;
    const ShipBlueprint* blueprint = database_.findShip(shipId);
    if (!blueprint || blueprint->layout.empty()) return false;

    const auto* layoutBytes = assets_.getBytes("data/" + blueprint->layout + ".txt");
    if (!layoutBytes) return false;

    LayoutBlueprint layout;
    const std::string text(layoutBytes->begin(), layoutBytes->end());
    if (!parseLayoutBlueprint(text, layout)) return false;

    out.blueprint = *blueprint;
    out.layout = std::move(layout);

    std::uint32_t rng = static_cast<std::uint32_t>(randomSeed);
    if (rng == 0) rng = static_cast<std::uint32_t>(std::hash<std::string>{}(shipId));
    auto nextRandom = [&]() {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return rng;
    };
    auto systemPower = [&](const char* name) {
        for (const auto& system : blueprint->systems)
            if (system.system == name && system.availableByDefault)
                return std::max(0, system.startingPower);
        return 0;
    };

    if (!out.blueprint.weaponLoadList.empty() && out.blueprint.initialWeapons.empty()) {
        if (const auto* list = database_.findBlueprintList(out.blueprint.weaponLoadList)) {
            int remaining = systemPower("weapons");
            const int slots = out.blueprint.weaponListCount >= 0
                ? out.blueprint.weaponListCount
                : (out.blueprint.weaponSlots > 0 ? out.blueprint.weaponSlots : 4);
            // Each generated slot samples the canonical pool independently.
            // Duplicate blueprint selections are valid; duplicate entries in
            // autoBlueprints.xml remain intentional additional weight.
            for (int slot = 0; slot < slots && remaining > 0; ++slot) {
                std::vector<std::string> candidates;
                for (const auto& id : *list) {
                    if (const auto* weapon = database_.findWeapon(id);
                        weapon && weapon->power > 0 && weapon->power <= remaining)
                        candidates.push_back(id);
                }
                if (candidates.empty()) break;
                const auto& selected = candidates[nextRandom() % candidates.size()];
                out.blueprint.initialWeapons.push_back(selected);
                remaining -= database_.findWeapon(selected)->power;
            }
        }
    }
    for (const auto& weaponId : out.blueprint.initialWeapons) {
        if (const auto* weapon = database_.findWeapon(weaponId))
            out.initialWeaponBlueprints.push_back(*weapon);
    }

    if (!out.blueprint.droneLoadList.empty() && out.blueprint.initialDrones.empty()) {
        if (const auto* list = database_.findBlueprintList(out.blueprint.droneLoadList)) {
            int remaining = systemPower("drones");
            const int slots = out.blueprint.droneListCount >= 0
                ? out.blueprint.droneListCount
                : (out.blueprint.droneSlots > 0 ? out.blueprint.droneSlots : 2);
            // Drone slots also sample independently; repeated blueprint
            // selections are not filtered here.
            for (int slot = 0; slot < slots && remaining > 0; ++slot) {
                std::vector<std::string> candidates;
                for (const auto& id : *list) {
                    if (const auto* drone = database_.findDrone(id);
                        drone && drone->power > 0 && drone->power <= remaining)
                        candidates.push_back(id);
                }
                if (candidates.empty()) break;
                const auto& selected = candidates[nextRandom() % candidates.size()];
                out.blueprint.initialDrones.push_back(selected);
                remaining -= database_.findDrone(selected)->power;
            }
        }
    }
    for (const auto& droneId : out.blueprint.initialDrones) {
        const auto* drone = database_.findDrone(droneId);
        if (drone) {
            DroneBlueprint resolved = *drone;
            if (!resolved.weaponBlueprint.empty()) {
                if (const auto* weapon = database_.findWeapon(resolved.weaponBlueprint)) {
                    resolved.weaponCooldown = weapon->cooldown;
                    resolved.weaponShots = weapon->shots;
                    resolved.weaponDamage = weapon->damage;
                    resolved.weaponSystemDamage = weapon->systemDamage;
                    resolved.weaponIonDamage = weapon->ionDamage;
                    resolved.weaponShieldPiercing = weapon->shieldPiercing;
                    resolved.weaponPersonnelDamage = weapon->personnelDamage;
                }
            }
            out.initialDroneBlueprints.push_back(std::move(resolved));
        }
    }
    return true;
}

bool ShipContent::loadEnemyShip(const std::string& shipId, LoadedShip& out, int sector,
                              int difficulty, unsigned randomSeed,
                              const std::string& blueprintPath) {
    if (!loadShip(shipId, out, blueprintPath, randomSeed))
        return false;

    std::uint32_t rng = static_cast<std::uint32_t>(randomSeed);
    if (rng == 0)
        rng = static_cast<std::uint32_t>(std::hash<std::string>{}(shipId));
    generateEnemySystems(out.blueprint, sector, difficulty, rng);

    // The system generator runs before enemy loadout generation in FTL.
    // Rebuild generated weapons/drones from the now-final system power.
    out.blueprint.initialWeapons.clear();
    out.blueprint.initialDrones.clear();
    out.initialWeaponBlueprints.clear();
    out.initialDroneBlueprints.clear();

    auto systemPower = [&](const char* name) {
        for (const auto& system : out.blueprint.systems)
            if (system.system == name && system.availableByDefault)
                return std::max(0, system.startingPower);
        return 0;
    };

    if (!out.blueprint.weaponLoadList.empty()) {
        if (const auto* list = database_.findBlueprintList(out.blueprint.weaponLoadList)) {
            int remaining = systemPower("weapons");
            const int slots = out.blueprint.weaponListCount >= 0
                ? out.blueprint.weaponListCount
                : (out.blueprint.weaponSlots > 0 ? out.blueprint.weaponSlots : 4);
                for (int slot = 0; slot < slots && remaining > 0; ++slot) {
                std::vector<std::string> candidates;
                for (const auto& id : *list) {
                    const auto* weapon = database_.findWeapon(id);
                    if (!weapon || weapon->power <= 0 || weapon->power > remaining) continue;
                    if (weapon->power != 1 && weapon->power >= systemPower("weapons")) continue;
                    if (weapon->power * 4 <= remaining) continue;
                    candidates.push_back(id);
                }
                if (candidates.empty()) break;
                const auto& selected = candidates[enemyNextRandom(rng) % candidates.size()];
                out.blueprint.initialWeapons.push_back(selected);
                remaining -= database_.findWeapon(selected)->power;
            }
        }
    }
    for (const auto& weaponId : out.blueprint.initialWeapons)
        if (const auto* weapon = database_.findWeapon(weaponId))
            out.initialWeaponBlueprints.push_back(*weapon);

    if (!out.blueprint.droneLoadList.empty()) {
        if (const auto* list = database_.findBlueprintList(out.blueprint.droneLoadList)) {
            int remaining = systemPower("drones");
            const int totalPower = remaining;
            const int slots = out.blueprint.droneListCount >= 0
                ? out.blueprint.droneListCount
                : (out.blueprint.droneSlots > 0 ? out.blueprint.droneSlots : 2);
            std::set<std::string> used;
            for (int slot = 0; slot < slots && remaining > 0; ++slot) {
                std::vector<std::string> candidates;
                for (const auto& id : *list) {
                    if (used.count(id)) continue;
                    const auto* drone = database_.findDrone(id);
                    if (!drone || drone->power <= 0 || drone->power > remaining) continue;
                    if (totalPower >= 4 && drone->power >= totalPower) continue;
                    candidates.push_back(id);
                }
                if (candidates.empty()) break;
                const auto& selected = candidates[enemyNextRandom(rng) % candidates.size()];
                used.insert(selected);
                out.blueprint.initialDrones.push_back(selected);
                remaining -= database_.findDrone(selected)->power;
            }
        }
    }
    for (const auto& droneId : out.blueprint.initialDrones) {
        const auto* drone = database_.findDrone(droneId);
        if (!drone) continue;
        DroneBlueprint resolved = *drone;
        if (!resolved.weaponBlueprint.empty()) {
            if (const auto* weapon = database_.findWeapon(resolved.weaponBlueprint)) {
                resolved.weaponCooldown = weapon->cooldown;
                resolved.weaponShots = weapon->shots;
                resolved.weaponDamage = weapon->damage;
                resolved.weaponSystemDamage = weapon->systemDamage;
                resolved.weaponIonDamage = weapon->ionDamage;
                resolved.weaponShieldPiercing = weapon->shieldPiercing;
                resolved.weaponPersonnelDamage = weapon->personnelDamage;
            }
        }
        out.initialDroneBlueprints.push_back(std::move(resolved));
    }
    return true;
}

bool ShipContent::loadPlayerShip(const std::string& blueprintPath,
                                  const std::string& shipId) {
    loaded_ = false;
    LoadedShip loaded;
    if (!loadShip(shipId, loaded, blueprintPath)) return false;
    ship_ = std::move(loaded);
    loaded_ = true;
    return true;
}

}