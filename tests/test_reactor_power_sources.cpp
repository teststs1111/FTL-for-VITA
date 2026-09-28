#include "game/ship_runtime.hpp"
#include <cassert>

using namespace wormhole;

int main() {
    ShipRuntime ship;
    ship.valid = true;
    ship.reactor = 10;

    RuntimeSystem weapons;
    weapons.type = "weapons";
    weapons.room = 1;
    weapons.power = 3;
    weapons.maxPower = 4;
    weapons.powered = true;
    ship.systems.push_back(weapons);

    RuntimeSystem pilot;
    pilot.type = "pilot";
    pilot.room = 1;
    pilot.power = 1;
    pilot.maxPower = 1;
    pilot.powered = true;
    ship.systems.push_back(pilot);

    RuntimeCrew z1;
    z1.race = "zoltan";
    z1.room = 1;
    z1.alive = true;
    ship.crew.push_back(z1);

    RuntimeCrew z2 = z1;
    ship.crew.push_back(z2);

    // Two Zoltans cover two of the three weapon power bars.
    assert(ship.zoltanPowerForSystem(ship.systems[0]) == 2);
    assert(ship.reactorFundedPowerForSystem(ship.systems[0]) == 1);

    // Subsystems do not receive Zoltan power.
    assert(ship.zoltanPowerForSystem(ship.systems[1]) == 0);
    assert(ship.reactorFundedPowerForSystem(ship.systems[1]) == 1);

    // Total reactor consumption excludes the two free Zoltan bars.
    assert(ship.usedReactorPower() == 2);
    assert(ship.availableReactorPower() == 8);

    ship.setReactorPowerCap(5);
    assert(ship.availableReactorPower() == 3);

    // A Zoltan can supply the missing bar even when the reactor is capped.
    ship.setReactorPowerCap(0);
    assert(ship.setSystemPower(0, 4));
    assert(ship.systems[0].power == 4);
    assert(ship.zoltanPowerForSystem(ship.systems[0]) == 2);
    assert(ship.reactorFundedPowerForSystem(ship.systems[0]) == 2);
    assert(ship.availableReactorPower() == 0);

    // Turning on a preallocated system can likewise use a Zoltan bar.
    ship.systems[0].powered = false;
    assert(ship.setSystemPowered(0, true));

    return 0;
}
