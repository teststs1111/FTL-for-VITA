#include "data/sector_graph.hpp"
#include "data/last_stand_state.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cmath>
#include <functional>
#include <queue>
#include <vector>

static void testVanillaGridBounds() {
    wormhole::SectorGraph graph;
    graph.generate(1, 0x12345678u);

    assert(graph.rows() == 6);
    assert(graph.nodes().size() >= 19);
    assert(graph.nodes().size() <= 24);
    assert(graph.startNode() >= 0);
    assert(graph.node(graph.startNode()));
    assert(graph.node(graph.startNode())->row == 0);
    assert(graph.node(graph.exitNode())->row >= 4);
    assert(graph.node(graph.exitNode())->row <= 5);

    std::vector<int> reachable(graph.nodes().size(), 0);
    std::queue<int> queue;
    for (const auto& beacon : graph.nodes()) {
        if (beacon.row == 0) {
            const int index = static_cast<int>(&beacon - graph.nodes().data());
            reachable[static_cast<std::size_t>(index)] = 1;
            queue.push(index);
        }
    }
    assert(!queue.empty());

    while (!queue.empty()) {
        const int current = queue.front();
        queue.pop();
        const auto* node = graph.node(current);
        assert(node);
        for (const int next : node->links) {
            const auto* target = graph.node(next);
            assert(target);
            assert(std::abs(target->row - node->row) <= 1);
            assert(std::abs(target->column - node->column) <= 1);
            const float dx = target->x - node->x;
            const float dy = target->y - node->y;
            assert((dx * dx + dy * dy) <= (165.0f * 165.0f));
            if (!reachable[static_cast<std::size_t>(next)]) {
                reachable[static_cast<std::size_t>(next)] = 1;
                queue.push(next);
            }
        }
    }

    bool exitReachable = false;
    for (std::size_t i = 0; i < graph.nodes().size(); ++i) {
        if (static_cast<int>(i) == graph.exitNode() && reachable[i]) {
            exitReachable = true;
            break;
        }
    }
    assert(exitReachable);

    const auto first = graph.selectable(-1, -1);
    assert(first.size() == 1);
    assert(first.front() == graph.startNode());
    assert(graph.node(first.front())->row == 0);

    const auto blocked = graph.selectable(graph.startNode(), 0);
    for (const int index : blocked) {
        const auto* node = graph.node(index);
        assert(node);
        assert(std::abs(node->row - graph.node(graph.startNode())->row) <= 1);
        assert(std::abs(node->column - graph.node(graph.startNode())->column) <= 1);
    }
}

static void testDeterminism() {
    wormhole::SectorGraph a;
    wormhole::SectorGraph b;
    a.generate(4, 0xCAFEBABEu);
    b.generate(4, 0xCAFEBABEu);
    assert(a.startNode() == b.startNode());
    assert(a.nodes().size() == b.nodes().size());
    for (std::size_t i = 0; i < a.nodes().size(); ++i) {
        assert(a.nodes()[i].row == b.nodes()[i].row);
        assert(a.nodes()[i].column == b.nodes()[i].column);
        assert(a.nodes()[i].x == b.nodes()[i].x);
        assert(a.nodes()[i].y == b.nodes()[i].y);
        assert(a.nodes()[i].links == b.nodes()[i].links);
    }
}

static void testFleetCoverage() {
    wormhole::SectorGraph graph;
    graph.generate(2, 0xABCDEF01u);

    graph.setFleetCoverageFromRow(0);
    const auto covered = graph.fleetCoveredIndices();
    assert(!covered.empty());
    for (const int index : covered) {
        const auto* node = graph.node(index);
        assert(node && node->fleetCovered);
        assert(node->row == 0);
        assert(graph.isFleetCovered(index));
    }

    graph.setFleetCoveredIndices({covered.front()});
    assert(graph.isFleetCovered(covered.front()));

    graph.setFleetCoveredIndices({});
    graph.advanceFleetCoverage(1);
    assert(graph.fleetCoveredIndices().size() == 1);

    graph.setFleetCoveredIndices({});
    graph.setFleetCoverageFromPosition(320.0f);
    for (std::size_t i = 0; i < graph.nodes().size(); ++i) {
        const auto* node = graph.node(static_cast<int>(i));
        assert(node);
        if (static_cast<int>(i) == graph.exitNode())
            assert(!node->fleetCovered);
        else
            assert(node->fleetCovered == (node->x <= 320.0f));
    }
    assert(!graph.fleetCoveredIndices().empty());

    const auto positionalCovered = graph.fleetCoveredIndices();
    graph.advanceFleetCoverage(1);
    const auto expanded = graph.fleetCoveredIndices();
    // Positional Fleet coverage is authoritative. The legacy row helper may
    // add a row when needed, but must never clear or duplicate existing flags.
    assert(expanded.size() >= positionalCovered.size());
    for (const int index : positionalCovered)
        assert(std::find(expanded.begin(), expanded.end(), index) != expanded.end());

    // A fleet-covered beacon must remain navigable so arrival can trigger
    // the Rebel fleet encounter rather than silently removing the route.
    const auto links = graph.selectable(graph.startNode(), -1);
    assert(!links.empty());
    graph.setFleetCoveredIndices({links.front()});
    const auto coveredSelectable = graph.selectable(graph.startNode(), 0);
    assert(std::find(coveredSelectable.begin(), coveredSelectable.end(), links.front()) != coveredSelectable.end());
}

static void testLastStandNavigation() {
    wormhole::SectorGraph graph;
    graph.generate(8, 0x13579BDFu);

    assert(graph.rows() == 6);
    assert(graph.nodes().size() >= 19);
    assert(graph.nodes().size() <= 24);

    bool hasReverseOrSameRowLink = false;
    for (std::size_t i = 0; i < graph.nodes().size(); ++i) {
        const auto& source = graph.nodes()[i];
        for (const int link : source.links) {
            const auto* target = graph.node(link);
            assert(target);
            assert(std::abs(source.row - target->row) <= 1);
            assert(std::abs(source.column - target->column) <= 1);
            const float dx = source.x - target->x;
            const float dy = source.y - target->y;
            assert((dx * dx + dy * dy) <= (165.0f * 165.0f));
            assert(std::find(source.links.begin(), source.links.end(), link) != source.links.end());
            const auto* reverseSource = target;
            assert(std::find(reverseSource->links.begin(), reverseSource->links.end(), static_cast<int>(i)) != reverseSource->links.end());
            assert(std::count(source.links.begin(), source.links.end(), link) == 1);
            if (target->row <= source.row)
                hasReverseOrSameRowLink = true;
        }
    }

    // Links are bidirectional in the vanilla map graph, so at least one
    // connection must point backward or remain on the same grid row.
    assert(hasReverseOrSameRowLink);
}


static void testLastStandRouteShapes() {
    wormhole::SectorGraph graph;
    graph.generate(8, 0x2468ACE0u);

    bool foundThreeToFive = false;
    for (std::size_t start = 0; start < graph.nodes().size(); ++start) {
        const auto& s = graph.nodes()[start];
        if (s.row != 4 && s.row != 5) continue;
        for (std::size_t goal = 0; goal < graph.nodes().size(); ++goal) {
            const auto& g = graph.nodes()[goal];
            if (g.row != 2 && g.row != 3) continue;
            for (int length = 3; length <= 5; ++length) {
                std::vector<int> seen(graph.nodes().size(), 0);
                std::function<bool(int,int)> dfs = [&](int current, int depth) {
                    if (depth == length) return current == static_cast<int>(goal);
                    seen[static_cast<std::size_t>(current)] = 1;
                    for (const int next : graph.node(current)->links) {
                        if (next < 0 || next >= static_cast<int>(seen.size())) continue;
                        if (seen[static_cast<std::size_t>(next)]) continue;
                        if (dfs(next, depth + 1)) return true;
                    }
                    seen[static_cast<std::size_t>(current)] = 0;
                    return false;
                };
                if (dfs(static_cast<int>(start), 0)) {
                    foundThreeToFive = true;
                    break;
                }
            }
            if (foundThreeToFive) break;
        }
        if (foundThreeToFive) break;
    }
    assert(foundThreeToFive);
}


static void testLastStandStateTransitions() {
    int routeIndex = 0;
    int jumpCounter = 0;
    int baseTurns = 0;
    int waitTurns = 0;

    auto r = wormhole::advanceLastStandState(4, routeIndex, jumpCounter, baseTurns, waitTurns, false);
    assert(!r.moved && !r.gameOver);
    assert(routeIndex == 0 && jumpCounter == 1);

    r = wormhole::advanceLastStandState(4, routeIndex, jumpCounter, baseTurns, waitTurns, false);
    assert(r.moved && !r.gameOver);
    assert(routeIndex == 1 && jumpCounter == 0);

    wormhole::retreatLastStandAfterPhase(4, routeIndex, jumpCounter, baseTurns, waitTurns, false);
    assert(routeIndex == 2);
    assert(jumpCounter == 0 && baseTurns == 0 && waitTurns == 1);

    r = wormhole::advanceLastStandState(4, routeIndex, jumpCounter, baseTurns, waitTurns, false);
    assert(!r.moved && !r.gameOver && waitTurns == 0 && jumpCounter == 0);

    r = wormhole::advanceLastStandState(4, routeIndex, jumpCounter, baseTurns, waitTurns, false);
    assert(!r.moved && jumpCounter == 1);
    r = wormhole::advanceLastStandState(4, routeIndex, jumpCounter, baseTurns, waitTurns, false);
    assert(r.moved && routeIndex == 3);

    routeIndex = 3;
    jumpCounter = 0;
    baseTurns = 0;
    waitTurns = 0;
    r = wormhole::advanceLastStandState(4, routeIndex, jumpCounter, baseTurns, waitTurns, true);
    assert(!r.gameOver && baseTurns == 1);
    r = wormhole::advanceLastStandState(4, routeIndex, jumpCounter, baseTurns, waitTurns, true);
    assert(!r.gameOver && baseTurns == 2);
    r = wormhole::advanceLastStandState(4, routeIndex, jumpCounter, baseTurns, waitTurns, true);
    assert(r.gameOver && baseTurns == 3);

    routeIndex = 2;
    jumpCounter = 0;
    baseTurns = 0;
    waitTurns = 0;
    r = wormhole::advanceLastStandState(3, routeIndex, jumpCounter, baseTurns, waitTurns, false);
    r = wormhole::advanceLastStandState(3, routeIndex, jumpCounter, baseTurns, waitTurns, false);
    assert(!r.moved && !r.gameOver && baseTurns == 1);
}
static void testNebulaSector() {
    wormhole::SectorGraph graph;
    graph.generate(4, 0x1234u);
    graph.setNebulaSector(true);
    for (const auto& beacon : graph.nodes()) assert(beacon.nebula);
    graph.setNebulaSector(false);
    for (const auto& beacon : graph.nodes()) assert(!beacon.nebula);
}

int main() {
    testVanillaGridBounds();
    testDeterminism();
    testFleetCoverage();
    testNebulaSector();
    testLastStandNavigation();
    testLastStandRouteShapes();
    testLastStandStateTransitions();
    return 0;
}
