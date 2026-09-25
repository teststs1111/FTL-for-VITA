#pragma once
#include <array>
#include <vector>
#include <cstdint>

namespace wormhole {

struct BeaconNode {
    int row{0};
    int column{0};
    std::vector<int> links;
    bool visited{false};
};

class SectorGraph {
public:
    void generate(int sector, std::uint32_t seed);
    const std::vector<BeaconNode>& nodes() const { return nodes_; }
    const BeaconNode* node(int index) const;
    std::vector<int> selectable(int current, int fleetRow = -1) const;
    int startNode() const { return startNode_; }
    int exitRow() const { return rows_ - 1; }
    int rows() const { return rows_; }
    int columns() const { return columns_; }
private:
    std::vector<BeaconNode> nodes_;
    int rows_{8};
    int columns_{3};
    int startNode_{0};
};

}
