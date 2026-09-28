#include "data/ship_content.hpp"
#include <algorithm>
#include <cstdint>
#include <functional>

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
                             const std::string& blueprintPath) {
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

    if (!out.blueprint.weaponLoadList.empty() && out.initialWeapons.empty()) {
        if (const auto* list = database_.findBlueprintList(out.blueprint.weaponLoadList)) {
            int remaining = systemPower("weapons");
            const int slots = out.blueprint.weaponSlots > 0 ? out.blueprint.weaponSlots : 4;
            // Vanilla chooses each random weapon from the list without
            // repeating an already selected blueprint. Duplicate entries in
            // autoBlueprints.xml remain intentional weights.
            std::vector<std::string> selectedWeapons;
            for (int slot = 0; slot < slots && remaining > 0; ++slot) {
                std::vector<std::string> candidates;
                for (const auto& id : *list) {
                    if (std::find(selectedWeapons.begin(), selectedWeapons.end(), id) != selectedWeapons.end())
                        continue;
                    if (const auto* weapon = database_.findWeapon(id);
                        weapon && weapon->power > 0 && weapon->power <= remaining)
                        candidates.push_back(id);
                }
                if (candidates.empty()) break;
                const auto& selected = candidates[nextRandom() % candidates.size()];
                selectedWeapons.push_back(selected);
                out.blueprint.initialWeapons.push_back(selected);
                remaining -= database_.findWeapon(selected)->power;
            }
        }
    }
    for (const auto& weaponId : out.blueprint.initialWeapons) {
        if (const auto* weapon = database_.findWeapon(weaponId))
            out.initialWeaponBlueprints.push_back(*weapon);
    }

    if (!out.blueprint.droneLoadList.empty() && out.initialDrones.empty()) {
        if (const auto* list = database_.findBlueprintList(out.blueprint.droneLoadList)) {
            int remaining = systemPower("drones");
            const int slots = out.blueprint.droneSlots > 0 ? out.blueprint.droneSlots : 2;
            // Drone loadouts follow the same no-repeat selection rule.
            std::vector<std::string> selectedDrones;
            for (int slot = 0; slot < slots && remaining > 0; ++slot) {
                std::vector<std::string> candidates;
                for (const auto& id : *list) {
                    if (std::find(selectedDrones.begin(), selectedDrones.end(), id) != selectedDrones.end())
                        continue;
                    if (const auto* drone = database_.findDrone(id);
                        drone && drone->power > 0 && drone->power <= remaining)
                        candidates.push_back(id);
                }
                if (candidates.empty()) break;
                const auto& selected = candidates[nextRandom() % candidates.size()];
                selectedDrones.push_back(selected);
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