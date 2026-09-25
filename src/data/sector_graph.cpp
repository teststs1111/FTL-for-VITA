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
    startNode_=static_cast<int>(rng()%columns_);
    nodes_[startNode_].visited=true;
}

const BeaconNode* SectorGraph::node(int index) const {
    if(index<0 || index>=static_cast<int>(nodes_.size())) return nullptr;
    return &nodes_[index];
}

std::vector<int> SectorGraph::selectable(int current, int fleetRow) const {
    if(current<0) {
        // The first map selection is always the first graph row. The fleet
        // boundary is applied only after the player has entered the sector;
        // fleetRow is a row index, never a column index.
        std::vector<int> out;
        for(int c=0;c<columns_;++c) {
            const int index = c;
            const auto* target = node(index);
            if (!target) continue;
            if (fleetRow >= 0 && target->row <= fleetRow) continue;
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
        if (fleetRow >= 0 && target->row <= fleetRow) continue;
        out.push_back(link);
    }

    // The fleet must never make a sector mathematically unwinnable. If every
    // connected destination is behind the simplified boundary, retain the
    // furthest-forward destination as an emergency escape route.
    if (out.empty() && !n->links.empty()) {
        int fallback = n->links.front();
        for (const int link : n->links) {
            const auto* candidate = node(link);
            const auto* best = node(fallback);
            if (candidate && best && candidate->row > best->row)
                fallback = link;
        }
        out.push_back(fallback);
    }
    return out;
}

}
