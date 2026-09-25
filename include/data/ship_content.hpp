#pragma once
#include "data/asset_store.hpp"
#include "data/blueprint_database.hpp"
#include "data/ship_blueprint.hpp"
#include <string>
#include <vector>

namespace wormhole {

struct LoadedShip {
    ShipBlueprint blueprint;
    LayoutBlueprint layout;
    std::vector<WeaponBlueprint> initialWeaponBlueprints;
    std::vector<DroneBlueprint> initialDroneBlueprints;
};

class ShipContent {
public:
    bool open(const std::string& archivePath);
    bool openArchives(const std::vector<std::string>& archivePaths);
    bool loadShip(const std::string& shipId, LoadedShip& out,
                  const std::string& blueprintPath = "data/blueprints.xml");
    bool loadPlayerShip(const std::string& blueprintPath = "data/blueprints.xml",
                        const std::string& shipId = "PLAYER_SHIP_HARD");

    const LoadedShip* playerShip() const { return loaded_ ? &ship_ : nullptr; }
    AssetStore& assets() { return assets_; }
    const AssetStore& assets() const { return assets_; }
    BlueprintDatabase& blueprints() { return database_; }
    const BlueprintDatabase& blueprints() const { return database_; }

private:
    AssetStore assets_;
    BlueprintDatabase database_{assets_};
    LoadedShip ship_;
    bool loaded_{false};
};

}