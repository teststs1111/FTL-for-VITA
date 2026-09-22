#include "data/blueprint_database.hpp"

namespace wormhole {

bool BlueprintDatabase::loadShipBlueprint(const std::string& assetPath) {
    const auto* data = assets_.getBytes(assetPath);
    if (!data) return false;

    bxml::Node root;
    try {
        root = bxml::read(*data);
    } catch (...) {
        return false;
    }

    ShipBlueprint ship;
    if (!parseShipBlueprint(root, ship) || ship.id.empty()) return false;
    ships_[ship.id] = std::move(ship);
    return true;
}

const ShipBlueprint* BlueprintDatabase::findShip(const std::string& id) const {
    const auto it = ships_.find(id);
    return it == ships_.end() ? nullptr : &it->second;
}

void BlueprintDatabase::clear() {
    ships_.clear();
}

}
