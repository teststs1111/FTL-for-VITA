#include "data/ship_content.hpp"

namespace wormhole {

bool ShipContent::open(const std::string& archivePath) {
    loaded_ = false;
    database_.clear();
    return assets_.openArchive(archivePath);
}

bool ShipContent::loadShip(const std::string& shipId, LoadedShip& out,
                             const std::string& blueprintPath) {
    database_.clear();
    out = {};

    if (database_.loadShipBlueprints(blueprintPath) == 0) return false;
    const ShipBlueprint* blueprint = database_.findShip(shipId);
    if (!blueprint || blueprint->layout.empty()) return false;

    const auto* layoutBytes = assets_.getBytes("data/" + blueprint->layout + ".txt");
    if (!layoutBytes) return false;

    LayoutBlueprint layout;
    const std::string text(layoutBytes->begin(), layoutBytes->end());
    if (!parseLayoutBlueprint(text, layout)) return false;

    out.blueprint = *blueprint;
    out.layout = std::move(layout);
    for (const auto& weaponId : out.blueprint.initialWeapons) {
        if (const auto* weapon = database_.findWeapon(weaponId))
            out.initialWeaponBlueprints.push_back(*weapon);
    }
    for (const auto& droneId : out.blueprint.initialDrones) {
        // Drone blueprints are loaded from the same database and copied into
        // the runtime ship just like starting weapons.
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