#pragma once
#include "game/ship_runtime.hpp"

namespace wormhole {

struct CombatResult {
    bool fired{false};
    int shotsFired{0};
    int shieldsAbsorbed{0};
    int hullDamage{0};
    int systemDamage{0};
    bool targetDestroyed{false};
};

class CombatRuntime {
public:
    ShipRuntime player;
    ShipRuntime enemy;
    int targetRoom{-1};
    int selectedWeapon{0};

    bool load(ShipContent& content, const LoadedShip& enemyShip);
    void update(float dt);
    bool setTargetRoom(int roomId);
    CombatResult fireSelectedWeapon();
    CombatResult fireWeapon(int weaponIndex);

private:
    CombatResult resolveWeapon(ShipRuntime& attacker, ShipRuntime& target,
                               RuntimeWeapon& weapon);
};

} 
