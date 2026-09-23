#include "game/combat_runtime.hpp"
#include <algorithm>

namespace wormhole {

bool CombatRuntime::load(ShipContent& contentSource, const LoadedShip& enemyShip) {
    player.reset();
    enemy.reset();
    targetRoom = -1;
    selectedWeapon = 0;
    enemyTargetRoom = 0;

    const LoadedShip* playerShip = contentSource.playerShip();
    if (!playerShip) return false;
    if (!player.load(*playerShip)) return false;
    if (!enemy.load(enemyShip)) return false;

    if (!enemy.content.layout.rooms.empty())
        targetRoom = 0;
    return true;
}

void CombatRuntime::update(float dt) {
    if (dt <= 0.0f) return;
    player.updateWeapons(dt);
    enemy.updateWeapons(dt);
    player.updateShields(dt);
    enemy.updateShields(dt);
    player.updateEnvironment(dt);
    enemy.updateEnvironment(dt);

    if (enemy.valid && enemyTargetRoom >= 0 &&
        enemyTargetRoom < static_cast<int>(player.content.layout.rooms.size())) {
        for (int i = 0; i < static_cast<int>(enemy.weapons.size()); ++i) {
            if (!enemy.weapons[i].ready) continue;
            auto result = resolveWeapon(enemy, player, enemy.weapons[i], enemyTargetRoom);
            if (result.fired) break;
        }
    }
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
                                          RuntimeWeapon& weapon, int targetRoom) {
    CombatResult result;
    if (!attacker.valid || !target.valid || targetRoom < 0)
        return result;
    if (targetRoom >= static_cast<int>(target.content.layout.rooms.size()))
        return result;

    result.fired = true;
    result.shotsFired = weapon.shots;

    int shieldPiercing = std::max(0, weapon.shieldPiercing);
    for (int shot = 0; shot < weapon.shots; ++shot) {
        if (target.shieldLayers > shieldPiercing) {
            --target.shieldLayers;
            target.shieldCharge = 0.0f;
            ++result.shieldsAbsorbed;
            continue;
        }

        if (weapon.damage > 0) {
            target.damageRoom(targetRoom, weapon.damage);
            result.hullDamage += weapon.damage;
        }
        if (weapon.systemDamage > 0)
            result.systemDamage += target.damageSystemInRoom(targetRoom, weapon.systemDamage);
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

    result = resolveWeapon(player, enemy, weapon, targetRoom);
    if (!result.fired) {
        // The weapon was consumed/reset before resolution. This path is only
        // reachable for an invalid target, so leave the shot spent rather than
        // attempting to rewind combat state.
    }
    return result;
}

CombatResult CombatRuntime::fireSelectedWeapon() {
    return fireWeapon(selectedWeapon);
}

}
