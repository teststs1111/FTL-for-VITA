#pragma once
#include "data/ship_content.hpp"
#include <vector>
namespace wormhole {
struct ShipRuntime {
    LoadedShip content;
    int hull{0}, maxHull{0}, reactor{0};
    std::vector<int> roomDamage;
    bool valid{false};
    bool load(ShipContent& source);
    void reset();
    bool damageRoom(int roomId, int amount);
    bool repairRoom(int roomId, int amount);
};
}