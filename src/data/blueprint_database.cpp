#include "data/blueprint_database.hpp"

#include <functional>

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

std::size_t BlueprintDatabase::loadShipBlueprints(const std::string& assetPath) {
    const auto* data = assets_.getBytes(assetPath);
    if (!data) return 0;

    bxml::Node root;
    try {
        root = bxml::read(*data);
    } catch (...) {
        return 0;
    }

    std::size_t loaded = 0;
    std::function<void(const bxml::Node&)> visit = [&](const bxml::Node& node) {
        if (node.name == "shipBlueprint" || node.name == "ship") {
            ShipBlueprint ship;
            if (parseShipBlueprint(node, ship) && !ship.id.empty()) {
                ships_[ship.id] = std::move(ship);
                ++loaded;
            }
        }
        for (const auto& child : node.children) visit(child);
    };
    visit(root);
    return loaded;
}

const ShipBlueprint* BlueprintDatabase::findShip(const std::string& id) const {
    const auto it = ships_.find(id);
    return it == ships_.end() ? nullptr : &it->second;
}

void BlueprintDatabase::clear() {
    ships_.clear();
}

}
