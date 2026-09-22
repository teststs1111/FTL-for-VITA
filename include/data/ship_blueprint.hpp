#pragma once
#include "data/bxml.hpp"
#include <string>
#include <vector>

namespace wormhole {

struct RoomBlueprint {
    int id{-1};
    int x{0};
    int y{0};
    int w{1};
    int h{1};
};

struct SystemSlotBlueprint {
    std::string system;
    int room{-1};
    int level{0};
};

struct CrewBlueprint {
    std::string race;
    std::string name;
    int room{-1};
};

struct ShipBlueprint {
    std::string id;
    std::string name;
    std::string layout;
    std::vector<RoomBlueprint> rooms;
    std::vector<SystemSlotBlueprint> systems;
    std::vector<CrewBlueprint> crew;
};

bool parseShipBlueprint(const bxml::Node& node, ShipBlueprint& out);

}
