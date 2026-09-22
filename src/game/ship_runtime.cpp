#include "game/ship_runtime.hpp"
#include <algorithm>
namespace wormhole {
bool ShipRuntime::load(ShipContent& source) {
    const LoadedShip* loaded = source.playerShip();
    if (!loaded) return false;
    content = *loaded; maxHull = std::max(0, content.blueprint.maxHealth);
    hull = maxHull; reactor = std::max(0, content.blueprint.startingReactorPower);
    roomDamage.assign(content.layout.rooms.size(), 0); valid = true; return true;
}
void ShipRuntime::reset() { content = {}; hull = maxHull = reactor = 0; roomDamage.clear(); valid = false; }
bool ShipRuntime::damageRoom(int roomId, int amount) {
    if (!valid || roomId < 0 || roomId >= (int)roomDamage.size() || amount <= 0) return false;
    roomDamage[roomId] += amount; hull = std::max(0, hull - amount); return true;
}
bool ShipRuntime::repairRoom(int roomId, int amount) {
    if (!valid || roomId < 0 || roomId >= (int)roomDamage.size() || amount <= 0) return false;
    int restored = std::min(amount, roomDamage[roomId]); roomDamage[roomId] -= restored;
    hull = std::min(maxHull, hull + restored); return restored > 0;
}
}