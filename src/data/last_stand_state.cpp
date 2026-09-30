#include "data/last_stand_state.hpp"

namespace wormhole {

LastStandAdvanceResult advanceLastStandState(
    int routeSize,
    int& routeIndex,
    int& jumpCounter,
    int& baseTurns,
    int& waitTurns,
    bool atBase) {
    LastStandAdvanceResult result;
    if (routeSize <= 0 || routeIndex < 0 || routeIndex >= routeSize)
        return result;

    if (waitTurns > 0) {
        --waitTurns;
        return result;
    }

    if (atBase) {
        ++baseTurns;
        result.gameOver = baseTurns >= 3;
        return result;
    }

    ++jumpCounter;
    if (jumpCounter < 2)
        return result;

    jumpCounter = 0;
    if (routeIndex + 1 < routeSize) {
        ++routeIndex;
        baseTurns = 0;
        result.moved = true;
    } else {
        ++baseTurns;
        result.gameOver = baseTurns >= 3;
    }
    return result;
}

void retreatLastStandAfterPhase(
    int routeSize,
    int& routeIndex,
    int& jumpCounter,
    int& baseTurns,
    int& waitTurns,
    bool atBase) {
    // After Phase 1/2, the Flagship retreats one beacon away from the
    // player's position. With the route ordered from spawn toward the Base,
    // that means moving toward the Base; if already on the Base, move back
    // one beacon instead. Then it waits for one player map action.
    if (atBase) {
        if (routeIndex > 0)
            --routeIndex;
    } else if (routeIndex + 1 < routeSize) {
        ++routeIndex;
    }

    jumpCounter = 0;
    baseTurns = 0;
    waitTurns = 1;
}

} // namespace wormhole
