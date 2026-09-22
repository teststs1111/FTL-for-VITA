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

    systems.clear();
    for (const auto& blueprint : content.blueprint.systems) {
        RuntimeSystem system;
        system.type = blueprint.system;
        system.room = blueprint.room;
        system.level = std::max(0, blueprint.level);
        system.power = system.level;
        system.powered = system.power > 0;
        systems.push_back(std::move(system));
    }

    crew.clear();
    for (const auto& blueprint : content.blueprint.crew) {
        RuntimeCrew member;
        member.race = blueprint.race;
        member.name = blueprint.name;
        member.room = blueprint.room;
        crew.push_back(std::move(member));
    }

    valid = true;
    return true;
}

void ShipRuntime::reset() {
    content = {};
    hull = maxHull = reactor = 0;
    roomDamage.clear();
    systems.clear();
    crew.clear();
    valid = false;
}

bool ShipRuntime::damageRoom(int roomId, int amount) {
    if (!valid || roomId < 0 || roomId >= static_cast<int>(roomDamage.size()) || amount <= 0) return false;
    roomDamage[roomId] += amount;
    hull = std::max(0, hull - amount);
    return true;
}

bool ShipRuntime::repairRoom(int roomId, int amount) {
    if (!valid || roomId < 0 || roomId >= static_cast<int>(roomDamage.size()) || amount <= 0) return false;
    const int restored = std::min(amount, roomDamage[roomId]);
    roomDamage[roomId] -= restored;
    hull = std::min(maxHull, hull + restored);
    return restored > 0;
}

bool ShipRuntime::setSystemPowered(int systemIndex, bool powered) {
    if (!valid || systemIndex < 0 || systemIndex >= static_cast<int>(systems.size())) return false;
    RuntimeSystem& system = systems[systemIndex];
    if (powered && availableReactorPower() < system.power) return false;
    system.powered = powered;
    return true;
}

int ShipRuntime::usedReactorPower() const {
    int used = 0;
    for (const auto& system : systems)
        if (system.powered) used += std::max(0, system.power);
    return used;
}

int ShipRuntime::availableReactorPower() const {
    return std::max(0, reactor - usedReactorPower());
}

}