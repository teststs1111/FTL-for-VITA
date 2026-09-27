#include "data/sector_graph.hpp"
#include <algorithm>
#include <random>

namespace wormhole {

void SectorGraph::generate(int sector, std::uint32_t seed) {
    nodes_.clear();
    rowStarts_.clear();
    rowCounts_.clear();

    std::mt19937 rng(seed ^ (static_cast<std::uint32_t>(sector) * 0x9e3779b9u));
    rowStarts_.resize(static_cast<std::size_t>(rows_), 0);
    rowCounts_.resize(static_cast<std::size_t>(rows_), 0);

    // Real FTL sectors are not a rigid 8x3 grid. Keep the familiar eight
    // progression rows, but vary the number of beacons in each row and place
    // them on a wider logical map. This gives the renderer sparse branches
    // while keeping the graph deterministic and compact for Vita.
    for (int r = 0; r < rows_; ++r) {
        const int count = 3 + static_cast<int>(rng() % 3); // 3..5 beacons
        rowStarts_[static_cast<std::size_t>(r)] = static_cast<int>(nodes_.size());
        rowCounts_[static_cast<std::size_t>(r)] = count;
        for (int i = 0; i < count; ++i) {
            const int column = (i * (columns_ - 1)) / std::max(1, count - 1);
            nodes_.push_back({r, column, {}, false, false, false});
        }
    }

    auto rowNode = [&](int row, int index) {
        return rowStarts_[static_cast<std::size_t>(row)] + index;
    };

    // Build a connected forward graph. First establish one guaranteed spine
    // from the random starting beacon to the exit, then give every node in
    // each later row at least one parent and add deterministic nearby branches.
    startNode_ = rowNode(0, static_cast<int>(rng() % rowCounts_[0]));
    int spine = startNode_;
    nodes_[spine].visited = true;

    for (int r = 0; r < rows_ - 1; ++r) {
        const int fromRowCount = rowCounts_[static_cast<std::size_t>(r)];
        const int toRowCount = rowCounts_[static_cast<std::size_t>(r + 1)];
        const int fromIndex = spine - rowStarts_[static_cast<std::size_t>(r)];
        const int spineTarget = std::min(toRowCount - 1,
            std::max(0, fromIndex + static_cast<int>(rng() % 3) - 1));
        const int guaranteedTarget = rowNode(r + 1, spineTarget);
        nodes_[spine].links.push_back(guaranteedTarget);
        spine = guaranteedTarget;

        // Every node in the next row gets at least one incoming edge. Prefer
        // nearby columns to avoid implausible long jumps across the map.
        for (int targetIndex = 0; targetIndex < toRowCount; ++targetIndex) {
            const int target = rowNode(r + 1, targetIndex);
            bool hasParent = false;
            const int preferred = std::min(fromRowCount - 1,
                std::max(0, targetIndex + static_cast<int>(rng() % 3) - 1));
            const int parent = rowNode(r, preferred);
            for (const int link : nodes_[parent].links)
                if (link == target) hasParent = true;
            if (!hasParent) nodes_[parent].links.push_back(target);
        }

        // Add one or two nearby alternatives from each node, with the final
        // row receiving slightly fewer branches to keep the exit leg readable.
        for (int sourceIndex = 0; sourceIndex < fromRowCount; ++sourceIndex) {
            auto& links = nodes_[rowNode(r, sourceIndex)].links;
            const int extras = (r == rows_ - 2) ? 1 : 1 + static_cast<int>(rng() % 2);
            for (int extra = 0; extra < extras; ++extra) {
                const int offset = static_cast<int>(rng() % 3) - 1;
                const int targetIndex = std::clamp(sourceIndex + offset, 0, toRowCount - 1);
                const int target = rowNode(r + 1, targetIndex);
                if (std::find(links.begin(), links.end(), target) == links.end())
                    links.push_back(target);
            }
        }
    }
}

const BeaconNode* SectorGraph::node(int index) const {
    if (index < 0 || index >= static_cast<int>(nodes_.size())) return nullptr;
    return &nodes_[static_cast<std::size_t>(index)];
}

std::vector<int> SectorGraph::selectable(int current, int fleetRow) const {
    if (current < 0) {
        std::vector<int> out;
        if (nodes_.empty()) return out;
        const int firstStart = rowStarts_.front();
        const int firstCount = rowCounts_.front();
        for (int i = 0; i < firstCount; ++i) {
            const int index = firstStart + i;
            const auto* target = node(index);
            if (!target) continue;
            if (target->fleetCovered || (fleetRow >= 0 && target->row <= fleetRow)) continue;
            out.push_back(index);
        }
        return out;
    }

    const auto* n = node(current);
    if (!n) return {};

    std::vector<int> out;
    for (const int link : n->links) {
        const auto* target = node(link);
        if (!target) continue;
        // Fleet-controlled beacons remain navigable; arrival triggers the
        // Rebel fleet encounter in MainGame. Removing them here would make
        // the fleet state unreachable from the player map.
        out.push_back(link);
    }

    // Per-beacon fleet flags are authoritative; fleetRow remains a compatibility fallback.
    return out;
}

void SectorGraph::setNebulaSector(bool enabled) {
    for (auto& beacon : nodes_) beacon.nebula = enabled;
}

void SectorGraph::setNebulaIndices(const std::vector<int>& indices) {
    for (auto& beacon : nodes_) beacon.nebula = false;
    for (const int index : indices) {
        if (index >= 0 && index < static_cast<int>(nodes_.size()))
            nodes_[static_cast<std::size_t>(index)].nebula = true;
    }
}

void SectorGraph::setFleetCoverageFromRow(int fleetRow) {
    for (auto& beacon : nodes_)
        beacon.fleetCovered = fleetRow >= 0 && beacon.row <= fleetRow;
}

void SectorGraph::setFleetCoveredIndices(const std::vector<int>& indices) {
    for (auto& beacon : nodes_) beacon.fleetCovered = false;
    for (const int index : indices) {
        if (index >= 0 && index < static_cast<int>(nodes_.size()))
            nodes_[static_cast<std::size_t>(index)].fleetCovered = true;
    }
}

std::vector<int> SectorGraph::fleetCoveredIndices() const {
    std::vector<int> result;
    for (std::size_t i = 0; i < nodes_.size(); ++i)
        if (nodes_[i].fleetCovered) result.push_back(static_cast<int>(i));
    return result;
}

bool SectorGraph::isFleetCovered(int index) const {
    const auto* beacon = node(index);
    return beacon && beacon->fleetCovered;
}

}
