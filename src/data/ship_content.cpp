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