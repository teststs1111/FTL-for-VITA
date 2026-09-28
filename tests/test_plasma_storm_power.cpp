#include "game/combat_runtime.hpp"
#include <cassert>

using namespace wormhole;

int main() {
    CombatRuntime combat;
    combat.player.valid = true;
    combat.player.reactor = 10;

    RuntimeSystem weapons;
    weapons.type = "weapons";
    weapons.room = 1;
    weapons.power = 4;
    weapons.maxPower = 4;
    weapons.powered = true;
    weapons.zoltanPower = 2;
    weapons.batteryPower = 1;
    combat.player.systems.push_back(weapons);

    RuntimeSystem shields;
    shields.type = "shields";
    shields.room = 2;
    shields.power = 4;
    shields.maxPower = 4;
    shields.powered = true;
    combat.player.systems.push_back(shields);

    RuntimeCrew z1;
    z1.race = "zoltan";
    z1.room = 1;
    z1.alive = true;
    combat.player.crew.push_back(z1);

    RuntimeCrew z2 = z1;
    combat.player.crew.push_back(z2);

    // The mixed allocation is 2 Zoltan + 1 Backup Battery + 1 reactor bar.
    // The second system adds four reactor bars, so reactor usage is five.
    assert(combat.player.reactorFundedPowerForSystem(combat.player.systems[0]) == 1);
    assert(combat.player.reactorFundedPowerForSystem(combat.player.systems[1]) == 4);
    assert(combat.player.usedReactorPower() == 5);

    // Add one reactor-funded bar, then enter a 10-power Plasma Storm.
    // The storm cap is ceil(10 / 2) = 5, so exactly one reactor-funded
    // bar must be removed. Zoltan and Backup Battery allocations are preserved.
    combat.player.systems[1].power = 5;
    assert(combat.player.usedReactorPower() == 6);

    combat.setRandomSeed(1);
    combat.setEnvironment(CombatEnvironment::PlasmaStorm);

    assert(combat.player.reactorPowerCap == 5);
    assert(combat.player.usedReactorPower() == 5);
    assert(combat.player.systems[0].zoltanPower == 2);
    assert(combat.player.systems[0].batteryPower == 1);
    assert(combat.player.systems[0].power == 4);
    assert(combat.player.systems[1].power == 4);

    // Leaving the storm restores the reactor's full allocation ceiling, but
    // the bars removed on entry are not automatically restored.
    combat.setEnvironment(CombatEnvironment::None);
    assert(combat.player.reactorPowerCap == -1);
    assert(combat.player.usedReactorPower() == 5);
    assert(combat.player.systems[0].zoltanPower == 2);
    assert(combat.player.systems[0].batteryPower == 1);
    assert(combat.player.systems[1].power == 4);

    return 0;
}
