#include "data/sector_graph.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <queue>
#include <vector>

static void testVariableReachableGraph() {
    wormhole::SectorGraph graph;
    graph.generate(1, 0x12345678u);

    assert(graph.rows() == 8);
    assert(graph.nodes().size() >= 24);
    assert(graph.nodes().size() <= 40);
    assert(graph.startNode() >= 0);
    assert(graph.node(graph.startNode()));
    assert(graph.node(graph.startNode())->row == 0);

    std::vector<int> reachable(graph.nodes().size(), 0);
    std::queue<int> queue;
    queue.push(graph.startNode());
    reachable[static_cast<std::size_t>(graph.startNode())] = 1;

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
        if (graph.nodes()[i].row == graph.exitRow() && reachable[i]) {
            exitReachable = true;
            break;
        }
    }
    assert(exitReachable);

    const auto first = graph.selectable(-1, -1);
    assert(!first.empty());
    for (const int index : first) assert(graph.node(index)->row == 0);

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
    assert(graph.fleetCoveredIndices().size() == 1);

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
    testVariableReachableGraph();
    testDeterminism();
    testFleetCoverage();
    testNebulaSector();
    return 0;
}
