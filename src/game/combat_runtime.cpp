#include "game/combat_runtime.hpp"
#include <algorithm>

namespace wormhole {

bool CombatRuntime::load(ShipContent& contentSource, const LoadedShip& enemyShip) {
    player.reset();
    enemy.reset();
    targetRoom = -1;
    selectedWeapon = 0;
    enemyTargetRoom = 0;
    outcome = CombatOutcome::Ongoing;
    shots_.clear();
    impactResults_.clear();
    lastImpactResult_ = {};
    hasImpactResult_ = false;
    enemyFireDelay_ = 0.0f;

    const LoadedShip* playerShip = contentSource.playerShip();
    if (!playerShip) return false;
    if (!player.load(*playerShip)) return false;
    if (!enemy.load(enemyShip)) return false;

    if (!enemy.content.layout.rooms.empty())
        targetRoom = 0;

    // Enemy AI prototype: prefer the player weapon room, then engines, then room 0.
    enemyTargetRoom = 0;
    for (const auto& system : player.systems) {
        if (system.type == "weapons" && system.room >= 0) {
            enemyTargetRoom = system.room;
            break;
        }
    }
    if (enemyTargetRoom == 0) {
        for (const auto& system : player.systems) {
            if (system.type == "engines" && system.room >= 0) {
                enemyTargetRoom = system.room;
                break;
            }
        }
    }
    return true;
}

void CombatRuntime::enqueueWeapon(bool fromPlayer, int weaponIndex,
                                  const RuntimeWeapon& weapon, int room) {
    const int projectileCount = std::max(1, weapon.shots);
    for (int i = 0; i < projectileCount; ++i) {
        CombatShot shot;
        shot.fromPlayer = fromPlayer;
        shot.weaponIndex = weaponIndex;
        shot.weapon = weapon;
        shot.weapon.shots = 1;
        shot.targetRoom = room;
        // Convert FTL's projectile speed into a stable gameplay flight time until
        // the renderer has the exact ship/projectile coordinates available.
        const float speed = static_cast<float>(std::max(1, weapon.speed));
        const float baseFlight = std::clamp(2.5f / speed, 0.12f, 0.75f);
        shot.duration = baseFlight + static_cast<float>(i) * 0.03f;
        shots_.push_back(std::move(shot));
    }
}

void CombatRuntime::update(float dt) {
    if (dt <= 0.0f || outcome != CombatOutcome::Ongoing) return;

    enemyFireDelay_ = std::max(0.0f, enemyFireDelay_ - dt);
    player.updateWeapons(dt);
    enemy.updateWeapons(dt);
    player.updateDrones(dt);
    enemy.updateDrones(dt);
    player.updateShields(dt);
    enemy.updateShields(dt);
    player.updateEnvironment(dt);
    enemy.updateEnvironment(dt);

    // Enemy AI prototype: launch at the selected player room when a weapon is ready.
    if (enemy.valid && enemyTargetRoom >= 0 &&
        enemyTargetRoom < static_cast<int>(player.content.layout.rooms.size()) &&
        enemyFireDelay_ <= 0.0f &&
        std::none_of(shots_.begin(), shots_.end(), [](const CombatShot& shot) {
            return !shot.fromPlayer;
        })) {
        for (int i = 0; i < static_cast<int>(enemy.weapons.size()); ++i) {
            if (!enemy.weapons[i].ready) continue;
            RuntimeWeapon firedWeapon = enemy.weapons[i];
            if (!enemy.fireWeapon(i)) continue;
            enqueueWeapon(false, i, firedWeapon, enemyTargetRoom);
            enemyFireDelay_ = 0.25f;
            break;
        }
    }

    // Defense drones intercept at most one eligible incoming projectile per update.
    bool defenseInterceptedThisUpdate = false;
    for (auto it = shots_.begin(); it != shots_.end();) {
        ShipRuntime& defender = it->fromPlayer ? enemy : player;
        bool intercepted = false;
        for (auto& drone : defender.drones) {
            if (defenseInterceptedThisUpdate ||
                drone.type != DroneBlueprint::Type::Defense || !drone.powered || !drone.active)
                continue;
            const bool laserTarget = drone.defenceTarget.empty() || drone.defenceTarget == "LASERS";
            if (!laserTarget) continue;
            drone.active = false;
            drone.charge = 0;
            intercepted = true;
            defenseInterceptedThisUpdate = true;
            break;
        }
        if (intercepted)
            it = shots_.erase(it);
        else
            ++it;
    }

    // Charged combat drones launch their configured weapon at the selected room.
    auto launchCombatDrone = [&](ShipRuntime& owner, int targetRoom, bool fromPlayer) {
        for (auto& drone : owner.drones) {
            if (drone.type != DroneBlueprint::Type::Combat || !drone.powered || !drone.active)
                continue;
            RuntimeWeapon weapon;
            weapon.name = drone.name + "_DRONE_WEAPON";
            weapon.type = "LASER";
            weapon.power = 0;
            weapon.speed = drone.weaponSpeed;
            weapon.shots = drone.weaponShots;
            weapon.damage = drone.weaponDamage;
            weapon.systemDamage = drone.weaponSystemDamage;
            weapon.ionDamage = drone.weaponIonDamage;
            weapon.shieldPiercing = drone.weaponShieldPiercing;
            weapon.personnelDamage = drone.weaponPersonnelDamage;
            weapon.cooldown = drone.weaponCooldown;
            drone.active = false;
            drone.weaponCharge = 0.0f;
            enqueueWeapon(fromPlayer, -1, weapon, targetRoom);
            break;
        }
    };
    launchCombatDrone(player, targetRoom, true);
    launchCombatDrone(enemy, enemyTargetRoom, false);

    // Resolve projectiles only after their flight time has elapsed.
    for (auto it = shots_.begin(); it != shots_.end();) {
        it->elapsed += dt;
        if (it->elapsed + 0.00001f < it->duration) {
            ++it;
            continue;
        }

        ShipRuntime& attacker = it->fromPlayer ? player : enemy;
        ShipRuntime& target = it->fromPlayer ? enemy : player;
        CombatResult result = resolveWeapon(attacker, target, it->weapon, it->targetRoom);
        const bool targetDestroyed = result.targetDestroyed;
        const bool hitEnemy = it->fromPlayer;
        impactResults_.push_back(result);
        lastImpactResult_ = impactResults_.back();
        hasImpactResult_ = true;
        it = shots_.erase(it);

        if (targetDestroyed) {
            outcome = hitEnemy ? CombatOutcome::EnemyDestroyed
                               : CombatOutcome::PlayerDestroyed;
            break;
        }
    }

    if (outcome != CombatOutcome::Ongoing) return;


}

bool CombatRuntime::selectWeapon(int weaponIndex) {
    if (weaponIndex < 0 || weaponIndex >= static_cast<int>(player.weapons.size()))
        return false;
    selectedWeapon = weaponIndex;
    return true;
}

bool CombatRuntime::setEnemyTargetRoom(int roomId) {
    if (!player.valid || roomId < 0 ||
        roomId >= static_cast<int>(player.content.layout.rooms.size()))
        return false;
    enemyTargetRoom = roomId;
    return true;
}

bool CombatRuntime::setTargetRoom(int roomId) {
    if (!enemy.valid || roomId < 0 ||
        roomId >= static_cast<int>(enemy.content.layout.rooms.size()))
        return false;
    targetRoom = roomId;
    return true;
}

CombatResult CombatRuntime::resolveWeapon(ShipRuntime& attacker,
                                          ShipRuntime& target,
                                          const RuntimeWeapon& weapon, int room) {
    CombatResult result;
    if (!attacker.valid || !target.valid || room < 0)
        return result;
    if (room >= static_cast<int>(target.content.layout.rooms.size()))
        return result;

    result.fired = true;
    result.shotsFired = weapon.shots;

    const int shieldPiercing = std::max(0, weapon.shieldPiercing);
    for (int shot = 0; shot < weapon.shots; ++shot) {
        // Missile/bomb weapons bypass shields in FTL. Laser/beam/ion-style
        // weapons must first overcome the target's shield layers.
        const bool shieldBypass = weapon.missilesUsed > 0;
        if (!shieldBypass && target.shieldLayers > shieldPiercing) {
            --target.shieldLayers;
            target.shieldCharge = 0.0f;
            ++result.shieldsAbsorbed;
            continue;
        }

        int hullDamage = std::max(0, weapon.damage);
        // Hull-buster beams deal their bonus damage when striking a room
        // without a system installed.
        if (weapon.hullBust > 0) {
            const bool hasSystem = std::any_of(target.systems.begin(), target.systems.end(),
                [room](const RuntimeSystem& system) {
                    return system.room == room;
                });
            if (!hasSystem)
                hullDamage += weapon.hullBust;
        }
        if (hullDamage > 0) {
            target.damageRoom(room, hullDamage);
            result.hullDamage += hullDamage;
        }
        if (weapon.systemDamage > 0)
            result.systemDamage += target.damageSystemInRoom(room, weapon.systemDamage);
        if (weapon.ionDamage > 0)
            result.ionDamage += target.damageSystemInRoom(room, weapon.ionDamage);
        if (weapon.personnelDamage > 0)
            result.personnelDamage += target.damageCrewInRoom(room, weapon.personnelDamage);
        if (target.hull <= 0) break;
    }

    result.targetDestroyed = target.hull <= 0;
    return result;
}

CombatResult CombatRuntime::fireWeapon(int weaponIndex) {
    CombatResult result;
    if (!player.valid || weaponIndex < 0 ||
        weaponIndex >= static_cast<int>(player.weapons.size()))
        return result;

    RuntimeWeapon& weapon = player.weapons[weaponIndex];
    if (!player.fireWeapon(weaponIndex))
        return result;

    const RuntimeWeapon firedWeapon = weapon;
    result.fired = true;
    result.shotsFired = std::max(1, firedWeapon.shots);
    enqueueWeapon(true, weaponIndex, firedWeapon, targetRoom);
    return result;
}

CombatResult CombatRuntime::fireSelectedWeapon() {
    return fireWeapon(selectedWeapon);
}


bool CombatRuntime::consumeImpactResult(CombatResult& result) {
    if (impactResults_.empty()) {
        hasImpactResult_ = false;
        return false;
    }
    result = impactResults_.front();
    impactResults_.pop_front();
    hasImpactResult_ = !impactResults_.empty();
    lastImpactResult_ = hasImpactResult_ ? impactResults_.back() : CombatResult{};
    return true;
}

}
