#include "game/combat_runtime.hpp"
#include <cassert>

using namespace wormhole;

int main() {
    // Normal fleet-controlled beacon: Rebel ASB.
    assert(selectRebelFleetEnvironment(false, false, false) == CombatEnvironment::PDSPlayer);

    // Fleet-controlled nebula beacon: ion/plasma storm, not ASB.
    assert(selectRebelFleetEnvironment(true, false, false) == CombatEnvironment::PlasmaStorm);

    // Nebula exit is the special non-ASB case.
    assert(selectRebelFleetEnvironment(true, true, false) == CombatEnvironment::None);

    // Easy-mode exit also has no ASB.
    assert(selectRebelFleetEnvironment(false, true, true) == CombatEnvironment::None);

    // Normal/hard exit still receives ASB.
    assert(selectRebelFleetEnvironment(false, true, false) == CombatEnvironment::PDSPlayer);

    // A zero-fuel arrival at a captured non-exit nebula follows the same
    // environment branch; fuel is a separate MainGame transition state.
    assert(selectRebelFleetEnvironment(true, false, false) == CombatEnvironment::PlasmaStorm);

    return 0;
}
