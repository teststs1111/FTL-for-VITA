#include "data/sector_graph.hpp"
#include <algorithm>
#include <random>

namespace wormhole {

void SectorGraph::generate(int sector, std::uint32_t seed) {
    nodes_.clear();
    std::mt19937 rng(seed ^ (static_cast<std::uint32_t>(sector) * 0x9e3779b9u));
    nodes_.reserve(static_cast<std::size_t>(rows_ * columns_));
    for (int r=0;r<rows_;++r)
        for (int c=0;c<columns_;++c)
            nodes_.push_back({r,c,{},false});

    // Keep the three lanes connected while allowing branches and convergence.
    for (int r=0;r<rows_-1;++r) {
        for (int c=0;c<columns_;++c) {
            const int from=r*columns_+c;
            std::array<int,3> candidates{
                r*columns_ + std::max(0,c-1),
                r*columns_ + c,
                r*columns_ + std::min(columns_-1,c+1)
            };
            std::shuffle(candidates.begin(), candidates.end(), rng);
            const int count = (r==rows_-2) ? 2 : (2 + static_cast<int>(rng()%2));
            for (int i=0;i<count;++i) {
                const int to=(r+1)*columns_+(candidates[i]-r*columns_);
                if (std::find(nodes_[from].links.begin(),nodes_[from].links.end(),to)==nodes_[from].links.end())
                    nodes_[from].links.push_back(to);
            }
        }
    }
    startNode_=static_cast<int>(rng()%columns);
    nodes_[startNode_].visited=true;
}

const BeaconNode* SectorGraph::node(int index) const {
    if(index<0 || index>=static_cast<int>(nodes_.size())) return nullptr;
    return &nodes_[index];
}

std::vector<int> SectorGraph::selectable(int current) const {
    if(current<0) {
        std::vector<int> out;
        for(int c=0;c<columns_;++c) out.push_back(c);
        return out;
    }
    const auto* n=node(current);
    return n ? n->links : std::vector<int>{};
}

}
