#pragma once

namespace wormhole {

struct LastStandAdvanceResult {
    bool moved{false};
    bool gameOver{false};
};

// Advance the Flagship state by one player map tick.
// Returns whether the route index moved and whether the Base countdown ended the game.
LastStandAdvanceResult advanceLastStandState(
    int routeSize,
    int& routeIndex,
    int& jumpCounter,
    int& baseTurns,
    int& waitTurns,
    bool atBase);

// Model the one-turn retreat/wait after a non-final Flagship phase is defeated.
void retreatLastStandAfterPhase(
    int routeSize,
    int& routeIndex,
    int& jumpCounter,
    int& baseTurns,
    int& waitTurns,
    bool atBase);

} // namespace wormhole
