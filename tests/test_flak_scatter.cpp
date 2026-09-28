#include "game/combat_runtime.hpp"
#include <cassert>

using namespace wormhole;

int main() {
    CombatRuntime combat;
    combat.player.valid = true;
    combat.enemy.valid = true;

    RoomBlueprint targetRoom;
    targetRoom.id = 0;
    targetRoom.x = 0;
    targetRoom.y = 0;
    targetRoom.w = 30;
    targetRoom.h = 30;

    RoomBlueprint adjacentRoom;
    adjacentRoom.id = 1;
    adjacentRoom.x = 30;
    adjacentRoom.y = 0;
    adjacentRoom.w = 30;
    adjacentRoom.h = 30;

    combat.enemy.content.layout.rooms = {targetRoom, adjacentRoom};

    RuntimeSystem weapons;
    weapons.type = "weapons";
    weapons.power = 1;
    weapons.maxPower = 1;
    weapons.powered = true;
    combat.player.systems.push_back(weapons);

    RuntimeWeapon flak;
    flak.name = "Flak I";
    flak.type = "FLAK";
    flak.power = 1;
    flak.allocatedPower = 1;
    flak.shots = 3;
    flak.speed = 26;
    flak.cooldown = 10.0f;
    flak.ready = true;
    combat.player.weapons.push_back(flak);
    combat.player.weaponIonDisabled = std::vector<bool>{false};

    assert(combat.setTargetRoom(0));
    combat.setRandomSeed(4);
    const CombatResult fired = combat.fireWeapon(0);
    assert(fired.fired);
    // Flak I has three damaging projectiles plus three non-damaging fake
    // debris projectiles, which participate in Defense Drone distraction.
    assert(combat.pendingShotCount() == 6);

    bool sawScatter = false;
    for (const auto& shot : combat.pendingShots()) {
        assert(shot.targetRoom == -1 || shot.targetRoom == 0 || shot.targetRoom == 1);
        if (shot.targetRoom == 1) sawScatter = true;
    }
    assert(sawScatter);

    return 0;
}
