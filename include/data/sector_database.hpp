#pragma once
#include "data/asset_store.hpp"
#include "data/bxml.hpp"
#include <string>
#include <vector>

namespace wormhole {

struct SectorEventPool {
    std::string name;
    int min{0};
    int max{0};
};

struct SectorDefinition {
    std::string name;
    int minSector{0};
    bool unique{false};
    std::string startEvent;
    std::vector<SectorEventPool> events;
};

class SectorDatabase {
public:
    explicit SectorDatabase(AssetStore& assets) : assets_(assets) {}
    bool load();
    const SectorDefinition* select(int sector, std::size_t variant) const;
    std::size_t size() const { return sectors_.size(); }

private:
    void collect(const bxml::Node& node);
    AssetStore& assets_;
    std::vector<SectorDefinition> sectors_;
};

}
