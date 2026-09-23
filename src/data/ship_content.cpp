#include "data/ship_content.hpp"

namespace wormhole {

bool ShipContent::open(const std::string& archivePath) {
    loaded_ = false;
    database_.clear();
    return assets_.openArchive(archivePath);
}

bool ShipContent::loadPlayerShip(const std::string& blueprintPath,
                                  const std::string& shipId) {
    loaded_ = false;
    database_.clear();

    if (database_.loadShipBlueprints(blueprintPath) == 0) return false;
    const ShipBlueprint* blueprint = database_.findShip(shipId);
    if (!blueprint || blueprint->layout.empty()) return false;

    const auto* layoutBytes = assets_.getBytes("data/" + blueprint->layout + ".txt");
    if (!layoutBytes) return false;

    LayoutBlueprint layout;
    const std::string text(layoutBytes->begin(), layoutBytes->end());
    if (!parseLayoutBlueprint(text, layout)) return false;

    ship_.blueprint = *blueprint;
    ship_.layout = std::move(layout);
    ship_.initialWeaponBlueprints.clear();
    for (const auto& weaponId : ship_.blueprint.initialWeapons) {
        if (const auto* weapon = database_.findWeapon(weaponId))
            ship_.initialWeaponBlueprints.push_back(*weapon);
    }
    loaded_ = true;
    return true;
}

}