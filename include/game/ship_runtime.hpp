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

struct RuntimeWeapon {
    std::string name;
    std::string type;
    int power{1};
    float cooldown{5.0f};
    float charge{0.0f};
    int shots{1};
    int damage{0};
    int systemDamage{0};
    int ionDamage{0};
    int shieldPiercing{0};
    int missilesUsed{0};
    bool ready{false};
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
    std::vector<RuntimeWeapon> weapons;
    int missiles{0};
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
    bool extinguishFire(int crewIndex);
    bool setDoorOpen(int doorIndex, bool open);
    bool setRoomFire(int roomId, bool fire);
    void updateEnvironment(float dt);
    int usedReactorPower() const;
    int availableReactorPower() const;
    void updateWeapons(float dt);
    bool fireWeapon(int weaponIndex);
};

}