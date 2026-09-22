#pragma once
#include "data/asset_store.hpp"
#include "data/blueprint_database.hpp"
#include "data/ship_blueprint.hpp"
#include <string>

namespace wormhole {

struct LoadedShip {
    ShipBlueprint blueprint;
    LayoutBlueprint layout;
};

class ShipContent {
public:
    bool open(const std::string& archivePath);
    bool loadPlayerShip(const std::string& blueprintPath = "data/blueprints.xml",
                        const std::string& shipId = "PLAYER_SHIP_HARD");

    const LoadedShip* playerShip() const { return loaded_ ? &ship_ : nullptr; }
    AssetStore& assets() { return assets_; }
    const AssetStore& assets() const { return assets_; }

private:
    AssetStore assets_;
    BlueprintDatabase database_{assets_};
    LoadedShip ship_;
    bool loaded_{false};
};

}