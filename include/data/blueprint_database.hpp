#pragma once
#include "data/asset_store.hpp"
#include "data/ship_blueprint.hpp"
#include <string>
#include <unordered_map>

namespace wormhole {

class BlueprintDatabase {
public:
    explicit BlueprintDatabase(AssetStore& assets) : assets_(assets) {}

    bool loadShipBlueprint(const std::string& assetPath);
    std::size_t loadShipBlueprints(const std::string& assetPath);
    const ShipBlueprint* findShip(const std::string& id) const;
    const WeaponBlueprint* findWeapon(const std::string& id) const;
    const DroneBlueprint* findDrone(const std::string& id) const;
    const std::unordered_map<std::string, WeaponBlueprint>& weapons() const { return weapons_; }
    const std::unordered_map<std::string, DroneBlueprint>& drones() const { return drones_; }
    void clear();

private:
    AssetStore& assets_;
    std::unordered_map<std::string, ShipBlueprint> ships_;
    std::unordered_map<std::string, WeaponBlueprint> weapons_;
    std::unordered_map<std::string, DroneBlueprint> drones_;
};

}
