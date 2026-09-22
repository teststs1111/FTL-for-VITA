#include "game/ship_runtime.hpp"
#include <algorithm>
namespace wormhole {
bool ShipRuntime::load(ShipContent& source) {
    const LoadedShip* loaded = source.playerShip();
    if (!loaded) return false;
    content = *loaded;
    maxHull = std::max(0, content.blueprint.maxHealth);
    hull = maxHull;
    reactor = std::max(0, content.blueprint.startingReactorPower);
    roomDamage.assign(content.layout.rooms.size(), 0);
    valid = true;
    return true;
}
void ShipRuntime::reset() {
    content = {};
    hull = maxHull = reactor = 0;
    roomDamage.clear();
    valid = false;
}
}
