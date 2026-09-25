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
    int damage{0};
    int ionDamage{0};
    float ionTimer{0.0f};
    bool ionDisabled{false};
    bool powered{false};
    float stunTimer{0.0f};
    bool breached{false};
};

struct RuntimeWeapon {
    std::string name;
    std::string type;
    int power{1};
    float cooldown{5.0f};
    float charge{0.0f};
    int speed{0};
    int shots{1};
    int damage{0};
    int systemDamage{0};
    int ionDamage{0};
    int shieldPiercing{0};
    int missilesUsed{0};
    int personnelDamage{0};
    int hullBust{0};
    int fireChance{0};
    int breachChance{0};
    int stunChance{0};
    int stunDuration{0};
    bool ready{false};
};

struct RuntimeDrone {
    DroneBlueprint::Type type{DroneBlueprint::Type::Unknown};
    std::string name;
    int power{1};
    int speed{0};
    int cooldown{0};
    int charge{0};
    int dodge{0};
    std::string defenceTarget;
    float weaponCooldown{5.0f};
    int weaponShots{1};
    int weaponDamage{0};
    int weaponSystemDamage{0};
    int weaponIonDamage{0};
    int weaponShieldPiercing{0};
    int weaponPersonnelDamage{0};
    int weaponSpeed{0};
    float weaponCharge{0.0f};
    bool powered{false};
    bool active{false};
};

struct RuntimeCrew {
    std::string race;
    std::string name;
    int room{-1};
    int health{100};
    int maxHealth{100};
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
    std::vector<RuntimeDrone> drones;
    std::vector<RuntimeWeapon> weapons;
    int missiles{0};
    int shieldLayers{0};
    int maxShieldLayers{0};
    float shieldCharge{0.0f};
    std::vector<bool> doorOpen;
    std::vector<int> roomOxygen;
    std::vector<bool> roomFire;
    std::vector<bool> roomBreach;
    bool valid{false};

    bool load(ShipContent& source);
    bool load(const LoadedShip& loaded);
    void reset();
    bool damageRoom(int roomId, int amount);
    int damageSystemInRoom(int roomId, int amount);
    int ionizeSystemInRoom(int roomId, int amount);
    bool repairRoom(int roomId, int amount);
    bool setSystemPowered(int systemIndex, bool powered);
    bool setSystemPower(int systemIndex, int power);
    bool moveCrew(int crewIndex, int targetRoom);
    int damageCrewInRoom(int roomId, int amount);
    int stunSystemsInRoom(int roomId, float seconds);
    int healCrew(int crewIndex, int amount);
    bool extinguishFire(int crewIndex);
    bool setDoorOpen(int doorIndex, bool open);
    bool setRoomFire(int roomId, bool fire);
    bool setRoomBreach(int roomId, bool breached);
    void updateEnvironment(float dt);
    int usedReactorPower() const;
    int availableReactorPower() const;
    void updateWeapons(float dt);
    void updateShields(float dt);
    bool damageShields(int amount);
    bool fireWeapon(int weaponIndex);
    bool setDronePowered(int droneIndex, bool powered);
    void updateDrones(float dt);
};

}