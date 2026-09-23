#pragma once
#include "data/bxml.hpp"
#include <string>
#include <vector>

namespace wormhole {

struct RoomBlueprint {
    int id{-1}; int x{0}; int y{0}; int w{1}; int h{1};
};

struct DoorBlueprint {
    int x{0}; int y{0}; int leftRoom{-1}; int rightRoom{-1}; bool vertical{false};
};

struct LayoutBlueprint {
    int xOffset{0}; int yOffset{0}; int horizontal{0}; int vertical{0};
    int ellipseX{0}; int ellipseY{0}; int ellipseW{0}; int ellipseH{0};
    std::vector<RoomBlueprint> rooms;
    std::vector<DoorBlueprint> doors;
};

struct SystemSlotBlueprint { std::string system; int room{-1}; int level{0}; int startingPower{0}; int maxPower{0}; bool availableByDefault{true}; };
struct CrewBlueprint { std::string race; std::string name; int room{-1}; };

struct ShipBlueprint {
    std::string id;
    std::string name;
    std::string layout;
    int maxHealth{0};
    int startingReactorPower{0};
    int weaponSlots{0};
    int droneSlots{0};
    int startingMissiles{0};
    std::vector<std::string> initialWeapons;
    std::vector<std::string> initialDrones;
    std::vector<RoomBlueprint> rooms;
    std::vector<DoorBlueprint> doors;
    std::vector<SystemSlotBlueprint> systems;
    std::vector<CrewBlueprint> crew;
};

bool parseShipBlueprint(const bxml::Node& node, ShipBlueprint& out);
bool parseLayoutBlueprint(const std::string& text, LayoutBlueprint& out);

}
