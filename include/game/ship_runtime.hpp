#pragma once
#include "data/ship_content.hpp"
#include <vector>
#include <string>

namespace wormhole {

struct RuntimeSystem {
    std::string type;
    int room{-1};
    int level{0};
    int power{0};
    int maxPower{0};
    bool powered{false};
};

struct RuntimeCrew {
    std::string race;
    std::string name;
    int room{-1};
    bool alive{true};
};

struct ShipRuntime {
    LoadedShip content;
    int hull{0};
    int maxHull{0};
    int reactor{0};
    std::vector<int> roomDamage;
    std::vector<RuntimeSystem> systems;
    std::vector<RuntimeCrew> crew;
    std::vector<bool> doorOpen;
    std::vector<int> roomOxygen;
    std::vector<bool> roomFire;
    bool valid{false};

    bool load(ShipContent& source);
    void reset();
    bool damageRoom(int roomId, int amount);
    bool repairRoom(int roomId, int amount);
    bool setSystemPowered(int systemIndex, bool powered);
    bool setSystemPower(int systemIndex, int power);
    bool moveCrew(int crewIndex, int targetRoom);
    bool setDoorOpen(int doorIndex, bool open);
    bool setRoomFire(int roomId, bool fire);
    void updateEnvironment(float dt);
    int usedReactorPower() const;
    int availableReactorPower() const;
};

}