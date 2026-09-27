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

    // Vanilla FTL builds the map before applying sector event data.
    // The map uses a 6x4 logical grid and normally contains 19-24 beacons.
    constexpr int kRows = 6;
    constexpr int kColumns = 4;
    rows_ = kRows;
    columns_ = kColumns;

    std::mt19937 rng(seed ^ (static_cast<std::uint32_t>(sector) * 0x9e3779b9u));

    std::vector<int> counts(static_cast<std::size_t>(rows_), 3);
    const int targetTotal = 19 + static_cast<int>(rng() % 6u);
    int remaining = targetTotal - rows_ * 3;
    while (remaining > 0) {
        const int row = static_cast<int>(rng() % static_cast<std::uint32_t>(rows_));
        if (counts[static_cast<std::size_t>(row)] < kColumns) {
            ++counts[static_cast<std::size_t>(row)];
            --remaining;
        }
    }

    rowStarts_.resize(static_cast<std::size_t>(rows_), 0);
    rowCounts_ = counts;

    for (int row = 0; row < rows_; ++row) {
        std::vector<int> available{0, 1, 2, 3};
        std::shuffle(available.begin(), available.end(), rng);
        std::sort(available.begin(), available.begin() +
            counts[static_cast<std::size_t>(row)]);
        rowStarts_[static_cast<std::size_t>(row)] =
            static_cast<int>(nodes_.size());
        for (int i = 0; i < counts[static_cast<std::size_t>(row)]; ++i) {
            BeaconNode beacon;
            beacon.row = row;
            beacon.column = available[static_cast<std::size_t>(i)];
            beacon.x = 150.0f + beacon.column * 150.0f + 8.0f +
                static_cast<float>(rng() % 134u);
            beacon.y = 80.0f + row * 100.0f + 8.0f +
                static_cast<float>(rng() % 84u);
            nodes_.push_back(beacon);
        }
    }

    auto rowNode = [&](int row, int index) {
        return rowStarts_[static_cast<std::size_t>(row)] + index;
    };

    startNode_ = rowNode(0, static_cast<int>(
        rng() % static_cast<std::uint32_t>(rowCounts_[0])));
    exitNode_ = rowNode(rows_ - 1, static_cast<int>(
        rng() % static_cast<std::uint32_t>(rowCounts_[rows_ - 1])));
    nodes_[static_cast<std::size_t>(startNode_)].visited = true;

    auto nearest = [](const std::vector<int>& columns, int column) {
        int best = 0;
        int bestDistance = 1000000;
        for (int i = 0; i < static_cast<int>(columns.size()); ++i) {
            const int distance =
                std::abs(columns[static_cast<std::size_t>(i)] - column);
            if (distance < bestDistance) {
                bestDistance = distance;
                best = i;
            }
        }
        return best;
    };

    for (int row = 0; row < rows_ - 1; ++row) {
        const int fromCount = rowCounts_[static_cast<std::size_t>(row)];
        const int toCount = rowCounts_[static_cast<std::size_t>(row + 1)];

        std::vector<int> fromColumns;
        std::vector<int> toColumns;
        for (int i = 0; i < fromCount; ++i)
            fromColumns.push_back(
                nodes_[static_cast<std::size_t>(rowNode(row, i))].column);
        for (int i = 0; i < toCount; ++i)
            toColumns.push_back(
                nodes_[static_cast<std::size_t>(rowNode(row + 1, i))].column);

        // Every destination gets at least one incoming route.
        for (int targetIndex = 0; targetIndex < toCount; ++targetIndex) {
            const int parentIndex = nearest(
                fromColumns, toColumns[static_cast<std::size_t>(targetIndex)]);
            auto& links =
                nodes_[static_cast<std::size_t>(rowNode(row, parentIndex))].links;
            const int target = rowNode(row + 1, targetIndex);
            if (std::find(links.begin(), links.end(), target) == links.end())
                links.push_back(target);
        }

        // Every source gets at least one outgoing route.
        for (int sourceIndex = 0; sourceIndex < fromCount; ++sourceIndex) {
            auto& links =
                nodes_[static_cast<std::size_t>(rowNode(row, sourceIndex))].links;
            if (!links.empty()) continue;
            const int targetIndex = nearest(
                toColumns, fromColumns[static_cast<std::size_t>(sourceIndex)]);
            links.push_back(rowNode(row + 1, targetIndex));
        }

        // Add nearby alternatives to produce the branching/converging map
        // structure without permitting implausibly long row-to-row jumps.
        for (int sourceIndex = 0; sourceIndex < fromCount; ++sourceIndex) {
            auto& links =
                nodes_[static_cast<std::size_t>(rowNode(row, sourceIndex))].links;
            const int sourceColumn =
                fromColumns[static_cast<std::size_t>(sourceIndex)];
            std::vector<int> candidates;
            for (int targetIndex = 0; targetIndex < toCount; ++targetIndex) {
                const int target = rowNode(row + 1, targetIndex);
                if (std::find(links.begin(), links.end(), target) != links.end())
                    continue;
                if (std::abs(toColumns[static_cast<std::size_t>(targetIndex)] -
                             sourceColumn) <= 1)
                    candidates.push_back(targetIndex);
            }
            std::shuffle(candidates.begin(), candidates.end(), rng);
            if (!candidates.empty() && (rng() % 100u) < 55u)
                links.push_back(rowNode(row + 1, candidates.front()));
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
