#include "data/last_stand_state.hpp"
#include <cassert>

using namespace wormhole;

int main() {
    int routeIndex = 0;
    int jumpCounter = 0;
    int baseTurns = 0;
    int waitTurns = 0;

    auto r = advanceLastStandState(6, routeIndex, jumpCounter, baseTurns, waitTurns, false);
    assert(!r.moved && routeIndex == 0 && jumpCounter == 1);
    r = advanceLastStandState(6, routeIndex, jumpCounter, baseTurns, waitTurns, false);
    assert(r.moved && routeIndex == 1 && jumpCounter == 0);

    // A completed phase makes the Flagship retreat one beacon farther toward
    // the Base, then wait for one player map action.
    retreatLastStandAfterPhase(6, routeIndex, jumpCounter, baseTurns, waitTurns, false);
    assert(routeIndex == 2 && jumpCounter == 0 && baseTurns == 0 && waitTurns == 1);
    r = advanceLastStandState(6, routeIndex, jumpCounter, baseTurns, waitTurns, false);
    assert(!r.moved && routeIndex == 2 && waitTurns == 0);

    // If the Flagship is already at the Base, retreat one beacon back out.
    routeIndex = 5;
    jumpCounter = 0;
    baseTurns = 0;
    waitTurns = 0;
    retreatLastStandAfterPhase(6, routeIndex, jumpCounter, baseTurns, waitTurns, true);
    assert(routeIndex == 4 && jumpCounter == 0 && baseTurns == 0 && waitTurns == 1);

    routeIndex = 5;
    jumpCounter = 0;
    baseTurns = 0;
    waitTurns = 0;
    r = advanceLastStandState(6, routeIndex, jumpCounter, baseTurns, waitTurns, true);
    assert(!r.gameOver && baseTurns == 1);
    r = advanceLastStandState(6, routeIndex, jumpCounter, baseTurns, waitTurns, true);
    assert(!r.gameOver && baseTurns == 2);
    r = advanceLastStandState(6, routeIndex, jumpCounter, baseTurns, waitTurns, true);
    assert(r.gameOver && baseTurns == 3);

    return 0;
}
