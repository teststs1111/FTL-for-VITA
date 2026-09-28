#include "game/combat_runtime.hpp"
#include <cassert>

using namespace wormhole;

int main() {
    CombatRuntime combat;
    combat.player.valid = true;
    combat.enemy.valid = true;
    combat.enemy.hull = 100;
    combat.enemy.maxHull = 100;

    RoomBlueprint room;
    room.id = 0;
    room.x = 0;
    room.y = 0;
    room.w = 30;
    room.h = 30;
    combat.enemy.content.layout.rooms.push_back(room);

    RuntimeWeapon flak;
    flak.name = "Flak I";
    flak.type = "FLAK";
    flak.shots = 3;
    flak.speed = 26;
    flak.ready = true;
    flak.allocatedPower = 1;
    combat.player.weapons.push_back(flak);
    combat.player.weaponIonDisabled = std::vector<bool>{false};

    assert(combat.setTargetRoom(0));
    combat.setRandomSeed(1);
    const CombatResult fired = combat.fireWeapon(0);
    assert(fired.fired);
    assert(combat.pendingShotCount() == 6);

    int fakeCount = 0;
    for (const auto& shot : combat.pendingShots()) {
        if (shot.fakeFlak) {
            ++fakeCount;
            assert(shot.weapon.shots == 1);
        }
    }
    assert(fakeCount == 3);

    combat.update(1.0f);
    CombatResult impact;
    int missCount = 0;
    while (combat.consumeImpactResult(impact)) {
        if (impact.evaded > 0) ++missCount;
    }
    assert(missCount >= 3);
    return 0;
}
