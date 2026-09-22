#pragma once
#include "data/ship_content.hpp"
#include <vector>

namespace wormhole {
struct ShipRuntime {
    LoadedShip content;
    int hull{0};
    int maxHull{0};
    int reactor{0};
    std::vector<int> roomDamage;
    bool valid{false};
    bool load(ShipContent& source);
    void reset();
};
}
