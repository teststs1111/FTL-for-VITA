#include "data/sector_graph.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <random>

namespace wormhole {

void SectorGraph::generate(int sector, std::uint32_t seed) {
    nodes_.clear();
    rowStarts_.clear();
    rowCounts_.clear();

    // Vanilla FTL creates the map before sector events are assigned.
    // The layout is a 6x4 grid. Each cell normally has an 80% chance to
    // contain a beacon, with the generator enforcing the 19-beacon minimum.
    constexpr int kRows = 6;
    constexpr int kColumns = 4;
    constexpr float kLinkDistance = 165.0f;
    rows_ = kRows;
    columns_ = kColumns;

    std::mt19937 rng(seed ^ (static_cast<std::uint32_t>(sector) * 0x9e3779b9u));

    std::vector<std::pair<int, int>> occupied;
    occupied.reserve(kRows * kColumns);
    int remainingCells = kRows * kColumns;
    int requiredBeacons = 19 + static_cast<int>(rng() % 6u);

    for (int row = 0; row < kRows; ++row) {
        for (int column = 0; column < kColumns; ++column) {
            --remainingCells;
            const int currentBeacons = static_cast<int>(occupied.size());

            // Keep enough cells occupied to reach the vanilla 19-beacon
            // minimum. The first column must also have a start beacon, and
            // normal sectors must have an exit candidate in the final two
            // columns of the 6-column map.
            bool force = currentBeacons + remainingCells < requiredBeacons;
            if (row == 0 && column == kColumns - 1) {
                bool hasStart = false;
                for (const auto& cell : occupied)
                    if (cell.first == 0) hasStart = true;
                if (!hasStart) force = true;
            }
            if (row >= kRows - 2 && column == kColumns - 1 && sector < 7) {
                bool hasExit = false;
                for (const auto& cell : occupied)
                    if (cell.first >= kRows - 2) hasExit = true;
                if (!hasExit) force = true;
            }

            if (force || (rng() % 100u) < 80u)
                occupied.emplace_back(row, column);
        }
    }

    rowStarts_.assign(static_cast<std::size_t>(rows_), 0);
    rowCounts_.assign(static_cast<std::size_t>(rows_), 0);

    for (int row = 0; row < rows_; ++row) {
        rowStarts_[static_cast<std::size_t>(row)] =
            static_cast<int>(nodes_.size());
        for (const auto& cell : occupied) {
            if (cell.first != row) continue;
            BeaconNode beacon;
            beacon.row = row;
            beacon.column = cell.second;
            beacon.x = 150.0f + row * 150.0f + 8.0f +
                static_cast<float>(rng() % 134u);
            beacon.y = 80.0f + cell.second * 100.0f + 8.0f +
                static_cast<float>(rng() % 84u);
            nodes_.push_back(beacon);
            ++rowCounts_[static_cast<std::size_t>(row)];
        }
    }

    auto rowNode = [&](int row, int index) {
        return rowStarts_[static_cast<std::size_t>(row)] + index;
    };

    startNode_ = rowNode(0, static_cast<int>(
        rng() % static_cast<std::uint32_t>(rowCounts_[0])));

    // Exit selection is finalized after the graph is built so a random
    // disconnected component can never become the required sector exit.
    // The vanilla exit is chosen from the far side of the map; for normal
    // sectors the final two grid columns are the candidate area.
    exitNode_ = -1;
    nodes_[static_cast<std::size_t>(startNode_)].visited = true;

    // Beacons connect to every beacon in an adjacent grid cell when the
    // actual map-space distance is <=165 pixels. Links are bidirectional,
    // which preserves the vanilla ability to revisit connected beacons.
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        for (std::size_t j = i + 1; j < nodes_.size(); ++j) {
            const auto& a = nodes_[i];
            const auto& b = nodes_[j];
            if (std::abs(a.row - b.row) > 1 ||
                std::abs(a.column - b.column) > 1)
                continue;

            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            if ((dx * dx + dy * dy) > (kLinkDistance * kLinkDistance))
                continue;

            nodes_[i].links.push_back(static_cast<int>(j));
            nodes_[j].links.push_back(static_cast<int>(i));
        }
    }

    // A valid FTL map must provide a route from the start to the exit.
    // If a sparse random layout disconnects them, preserve the generated
    // placement but add only the nearest adjacent-grid bridge needed to make
    // the route traversable. This keeps the map shape random while preventing
    // an unwinnable sector graph.
    std::vector<int> reachable(nodes_.size(), 0);
    std::vector<int> stack{startNode_};
    while (!stack.empty()) {
        const int current = stack.back();
        stack.pop_back();
        if (reachable[static_cast<std::size_t>(current)]) continue;
        reachable[static_cast<std::size_t>(current)] = 1;
        for (const int next : nodes_[static_cast<std::size_t>(current)].links)
            if (!reachable[static_cast<std::size_t>(next)]) stack.push_back(next);
    }

    std::vector<int> exitCandidates;
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        const auto& beacon = nodes_[i];
        const bool inExitArea = sector >= 7
            ? beacon.row == rows_ - 1
            : beacon.row >= rows_ - 2;
        if (inExitArea && reachable[i])
            exitCandidates.push_back(static_cast<int>(i));
    }

    if (exitCandidates.empty()) {
        // Vanilla generation guarantees at least one route to the exit.
        // Retry the random map layout instead of introducing a synthetic
        // long-distance link or moving the exit into the wrong column.
        generate(sector, seed + 0x9e3779b9u);
        return;
    }

    exitNode_ = exitCandidates[static_cast<std::size_t>(
        rng() % static_cast<std::uint32_t>(exitCandidates.size()))];
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
        const int index = startNode_;
        const auto* target = node(index);
        if (target && !target->fleetCovered && !(fleetRow >= 0 && target->row <= fleetRow))
            out.push_back(index);
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

void SectorGraph::advanceFleetCoverage(int steps) {
    if (steps <= 0 || nodes_.empty()) return;

    // In normal FTL sectors the Rebel Fleet advances from the sector entrance
    // toward the exit, following the same left-to-right progression as the
    // sector map. The final sector has separate flagship behavior handled by
    // MainGame; this graph-level frontier is for the normal pursuit model.
    for (int step = 0; step < steps; ++step) {
        int frontierRow = -1;
        for (const auto& beacon : nodes_) {
            if (beacon.fleetCovered)
                frontierRow = std::max(frontierRow, beacon.row);
        }

        const int nextRow = frontierRow + 1;
        if (nextRow >= exitRow()) break;

        std::vector<int> candidates;
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            const auto& beacon = nodes_[i];
            if (beacon.row != nextRow || beacon.fleetCovered) continue;

            bool adjacent = frontierRow < 0;
            if (!adjacent) {
                for (std::size_t sourceIndex = 0; sourceIndex < nodes_.size(); ++sourceIndex) {
                    const auto& source = nodes_[sourceIndex];
                    if (!source.fleetCovered || source.row != frontierRow) continue;
                    if (std::find(source.links.begin(), source.links.end(),
                                  static_cast<int>(i)) != source.links.end()) {
                        adjacent = true;
                        break;
                    }
                }
            }
            if (adjacent) candidates.push_back(static_cast<int>(i));
        }

        if (candidates.empty()) break;

        nodes_[static_cast<std::size_t>(candidates.front())].fleetCovered = true;
    }
}

void SectorGraph::setFleetCoverageFromPosition(float x) {
    // Vanilla FTL advances the Rebel pursuit counter continuously from the
    // left edge. A beacon is under fleet control once its map position lies
    // behind the current frontier. Keep the exit beacon outside this normal
    // frontier so the player can still transition through it.
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        auto& beacon = nodes_[i];
        beacon.fleetCovered =
            static_cast<int>(i) != exitNode_ &&
            beacon.x <= x;
    }
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
