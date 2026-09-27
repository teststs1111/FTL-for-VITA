#pragma once
#include <vector>
#include <cstdint>

namespace wormhole {

struct BeaconNode {
    int row{0};
    int column{0};
    std::vector<int> links;
    bool visited{false};
    bool fleetCovered{false};
    bool nebula{false};
};

class SectorGraph {
public:
    void generate(int sector, std::uint32_t seed);
    const std::vector<BeaconNode>& nodes() const { return nodes_; }
    const BeaconNode* node(int index) const;
    std::vector<int> selectable(int current, int fleetRow = -1) const;
    void setFleetCoverageFromRow(int fleetRow);
    void advanceFleetCoverage(int steps);
    void setNebulaSector(bool enabled);
    void setNebulaIndices(const std::vector<int>& indices);
    void setFleetCoveredIndices(const std::vector<int>& indices);
    std::vector<int> fleetCoveredIndices() const;
    bool isFleetCovered(int index) const;
    int startNode() const { return startNode_; }
    int exitRow() const { return rows_ - 1; }
    int rows() const { return rows_; }
    int columns() const { return columns_; }
private:
    std::vector<BeaconNode> nodes_;
    std::vector<int> rowStarts_;
    std::vector<int> rowCounts_;
    int rows_{6};
    int columns_{4};
    int startNode_{0};
};

}
