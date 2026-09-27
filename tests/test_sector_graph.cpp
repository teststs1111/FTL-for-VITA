#include "data/sector_graph.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
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
            assert(next > current || graph.node(next)->row > node->row);
            assert(graph.node(next)->row == node->row + 1);
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
    for (const int index : blocked)
        assert(graph.node(index)->row > 0);
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
    return 0;
}
