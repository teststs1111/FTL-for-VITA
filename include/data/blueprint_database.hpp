#pragma once
#include "data/asset_store.hpp"
#include "data/ship_blueprint.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace wormhole {

struct AugmentBlueprint {
    std::string id;
    std::string title;
    std::string description;
    int cost{0};
    int rarity{0};
};

class BlueprintDatabase {
public:
    explicit BlueprintDatabase(AssetStore& assets) : assets_(assets) {}

    bool loadShipBlueprint(const std::string& assetPath);
    std::size_t loadShipBlueprints(const std::string& assetPath);
    std::size_t loadShipBlueprints(const std::vector<std::string>& assetPaths);
    const ShipBlueprint* findShip(const std::string& id) const;
    const WeaponBlueprint* findWeapon(const std::string& id) const;
    const DroneBlueprint* findDrone(const std::string& id) const;
    const std::unordered_map<std::string, WeaponBlueprint>& weapons() const { return weapons_; }
    const std::unordered_map<std::string, DroneBlueprint>& drones() const { return drones_; }
    const std::unordered_map<std::string, AugmentBlueprint>& augments() const { return augments_; }
    const AugmentBlueprint* findAugment(const std::string& id) const;
    const std::unordered_map<std::string, ShipBlueprint>& ships() const { return ships_; }
    void clear();

private:
    AssetStore& assets_;
    std::unordered_map<std::string, ShipBlueprint> ships_;
    std::unordered_map<std::string, WeaponBlueprint> weapons_;
    std::unordered_map<std::string, DroneBlueprint> drones_;
    std::unordered_map<std::string, AugmentBlueprint> augments_;
};

}
