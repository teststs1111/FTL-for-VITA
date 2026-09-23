#pragma once
#include "game/ship_runtime.hpp"
#include <vector>

namespace wormhole {

enum class CombatOutcome { Ongoing, PlayerDestroyed, EnemyDestroyed };

struct CombatResult {
    bool fired{false};
    int shotsFired{0};
    int shieldsAbsorbed{0};
    int hullDamage{0};
    int systemDamage{0};
    bool targetDestroyed{false};
};

struct CombatShot {
    bool fromPlayer{true};
    int weaponIndex{-1};
    RuntimeWeapon weapon;
    int targetRoom{-1};
    float elapsed{0.0f};
    float duration{0.25f};
};

class CombatRuntime {
public:
    ShipRuntime player;
    ShipRuntime enemy;
    int targetRoom{-1};
    int selectedWeapon{0};
    int enemyTargetRoom{0};
    CombatOutcome outcome{CombatOutcome::Ongoing};

    bool load(ShipContent& content, const LoadedShip& enemyShip);
    void update(float dt);
    bool setTargetRoom(int roomId);
    CombatResult fireSelectedWeapon();
    CombatResult fireWeapon(int weaponIndex);
    std::size_t pendingShotCount() const { return shots_.size(); }

private:
    CombatResult resolveWeapon(ShipRuntime& attacker, ShipRuntime& target,
                               const RuntimeWeapon& weapon, int targetRoom);

    void enqueueWeapon(bool fromPlayer, int weaponIndex, const RuntimeWeapon& weapon,
                       int targetRoom);
    std::vector<CombatShot> shots_;
};

} 
