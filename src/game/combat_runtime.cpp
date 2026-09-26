#include "game/combat_runtime.hpp"
#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

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
    boardingTimer_ = 8.0f;
    boardingFightTimer_ = 0.0f;
    boarders.clear();
    flagshipPhase_ = 0;
    droneSurgeTimer_ = 0.0f;
    playerWeaponCooldownMultiplier_ = 1.0f;
    superShield_ = 0;

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


bool CombatRuntime::loadFlagshipPhase(ShipContent& contentSource, const LoadedShip& enemyShip,
                                      const std::vector<RuntimeCrew>& previousCrew) {
    if (!load(contentSource, enemyShip)) return false;

    // Flagship phases use separate blueprints, but crew casualties persist
    // between phases. Match by name/race and carry health/alive state forward.
    std::vector<bool> matched(previousCrew.size(), false);
    for (auto& nextCrew : enemy.crew) {
        std::size_t match = previousCrew.size();
        for (std::size_t i = 0; i < previousCrew.size(); ++i) {
            if (matched[i]) continue;
            const auto& oldCrew = previousCrew[i];
            if (oldCrew.name == nextCrew.name && oldCrew.race == nextCrew.race) {
                match = i;
                break;
            }
        }
        if (match == previousCrew.size()) {
            for (std::size_t i = 0; i < previousCrew.size(); ++i) {
                if (matched[i]) continue;
                if (previousCrew[i].race == nextCrew.race) {
                    match = i;
                    break;
                }
            }
        }
        if (match == previousCrew.size()) continue;
        matched[match] = true;
        const auto& oldCrew = previousCrew[match];
        nextCrew.alive = oldCrew.alive;
        nextCrew.health = oldCrew.alive ? std::clamp(oldCrew.health, 0, nextCrew.maxHealth) : 0;
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

std::uint32_t CombatRuntime::nextRandom() {
    randomState_ ^= randomState_ << 13;
    randomState_ ^= randomState_ >> 17;
    randomState_ ^= randomState_ << 5;
    return randomState_;
}

void CombatRuntime::setRandomSeed(std::uint32_t seed) {
    randomState_ = seed == 0 ? 0x6D2B79F5u : seed;
}

void CombatRuntime::configureFlagshipPhase(int phase) {
    flagshipPhase_ = std::clamp(phase, 0, 3);
    droneSurgeTimer_ = flagshipPhase_ == 2 ? 10.0f : 0.0f;
    superShield_ = flagshipPhase_ == 3 ? 10 : 0;
}

void CombatRuntime::updateEnvironmentHazard(float dt) {
    if (environment_ == CombatEnvironment::None || outcome != CombatOutcome::Ongoing) return;
    environmentTimer_ -= dt;
    if (environmentTimer_ > 0.0f) return;

    auto randomRoom = [](ShipRuntime& ship, std::uint32_t roll) {
        if (ship.content.layout.rooms.empty()) return -1;
        return ship.content.layout.rooms[roll % ship.content.layout.rooms.size()].id;
    };

    auto schedule = [&](float minimum, float maximum) {
        const std::uint32_t span = static_cast<std::uint32_t>((maximum - minimum) * 1000.0f);
        const float offset = span == 0 ? 0.0f : static_cast<float>(nextRandom() % (span + 1)) / 1000.0f;
        environmentTimer_ = minimum + offset;
    };

    if (environment_ == CombatEnvironment::Asteroid) {
        ShipRuntime* ships[2] = {&player, &enemy};
        for (ShipRuntime* ship : ships) {
            if (ship->shieldLayers > 0) {
                ship->damageShields(1);
            } else {
                const int room = randomRoom(*ship, nextRandom());
                if (room >= 0) {
                    ship->damageRoom(room, 1);
                    if ((nextRandom() % 100u) < 10u) ship->setRoomFire(room, true);
                    if ((nextRandom() % 100u) < 10u) ship->setRoomBreach(room, true);
                }
            }
        }
        schedule(4.0f, 8.0f);
        return;
    }

    if (environment_ == CombatEnvironment::Sun) {
        ShipRuntime* ships[2] = {&player, &enemy};
        for (ShipRuntime* ship : ships) {
            const int fires = ship->shieldLayers > 0
                ? 1 + static_cast<int>(nextRandom() % 2u)
                : 3 + static_cast<int>(nextRandom() % 4u);
            for (int i = 0; i < fires; ++i) {
                const int room = randomRoom(*ship, nextRandom());
                if (room < 0) continue;
                if (ship->setRoomFire(room, true) &&
                    (nextRandom() % 100u) < (fires > 2 ? 66u : 33u))
                    ship->damageRoom(room, 1);
            }
        }
        schedule(28.0f, 34.0f);
        return;
    }

    ShipRuntime* target = environment_ == CombatEnvironment::PDSEnemy ? &enemy : &player;
    const int room = randomRoom(*target, nextRandom());
    if (room >= 0) {
        int evade = 0;
        if (target == &player && !cloaked()) {
            for (const auto& system : player.systems)
                if (system.type == SystemType::Engines) evade += system.power * 5;
            for (const auto& crew : player.crew)
                if (crew.alive && crew.room == player.content.layout.rooms.front().id)
                    evade += std::min(5, crew.pilotSkill);
            evade = std::min(40, evade);
        }
        if ((nextRandom() % 100u) >= static_cast<std::uint32_t>(evade)) {
            target->damageRoom(room, 3);
            target->setRoomBreach(room, true);
        }
    }
    schedule(20.0f, 30.0f);
}

void CombatRuntime::update(float dt) {
    if (dt <= 0.0f || outcome != CombatOutcome::Ongoing) return;

    enemyFireDelay_ = std::max(0.0f, enemyFireDelay_ - dt);
    player.updateWeapons(dt, playerWeaponCooldownMultiplier_);
    enemy.updateWeapons(dt);
    player.updateDrones(dt);
    enemy.updateDrones(dt);
    player.updateShields(dt, playerShieldRechargeMultiplier_);
    enemy.updateShields(dt);
    player.updateEnvironment(dt);
    enemy.updateEnvironment(dt);

    // Rebel Flagship phase 2 periodically triggers the original-style
    // "drone power surge". Model the surge as several simultaneous drone
    // projectiles so it remains independent of the enemy ship's normal
    // reactor allocation and cannot be disabled by ordinary system damage.
    if (flagshipPhase_ == 2) {
        droneSurgeTimer_ -= dt;
        if (droneSurgeTimer_ <= 0.0f && !player.content.layout.rooms.empty()) {
            const int roomCount = static_cast<int>(player.content.layout.rooms.size());
            const int surgeCount = 3 + static_cast<int>(nextRandom() % 2u);
            for (int i = 0; i < surgeCount; ++i) {
                RuntimeWeapon surge;
                surge.name = "FLAGSHIP_DRONE_SURGE";
                surge.type = "LASER";
                surge.power = 0;
                surge.speed = 10;
                surge.shots = 1;
                surge.damage = 1;
                surge.systemDamage = 1;
                surge.cooldown = 1.0f;
                const int room = player.content.layout.rooms[
                    static_cast<std::size_t>(nextRandom() % static_cast<std::uint32_t>(roomCount))].id;
                enqueueWeapon(false, -1, surge, room);
            }
            droneSurgeTimer_ = 20.0f;
        }
    }

    // Boarding-party prototype: after a short delay, an enemy crew member
    // enters the player's weapons room. This uses the same RuntimeCrew model
    // so health/death and room interactions remain deterministic.
    boardingTimer_ -= dt;
    if (boardingTimer_ <= 0.0f && boarders.empty() && !enemy.crew.empty() && !player.content.layout.rooms.empty()) {
        for (const auto& enemyCrew : enemy.crew) {
            if (!enemyCrew.alive) continue;
            RuntimeCrew boarder = enemyCrew;
            boarder.room = enemyTargetRoom;
            if (boarder.room < 0 || boarder.room >= static_cast<int>(player.content.layout.rooms.size()))
                boarder.room = player.content.layout.rooms.front().id;
            boarder.health = boarder.maxHealth;
            boarder.alive = true;
            boarders.push_back(std::move(boarder));
            break;
        }
        boardingTimer_ = 20.0f;
    }

    boardingFightTimer_ = std::max(0.0f, boardingFightTimer_ - dt);
    if (!boarders.empty() && boardingFightTimer_ <= 0.0f) {
        // Boarding crews now move through open doors toward the nearest living
        // defender instead of remaining permanently in their insertion room.
        // This keeps boarding tied to the same ship layout/door state used by
        // player crew movement.
        for (auto& boarder : boarders) {
            if (!boarder.alive) continue;

            int targetRoom = boarder.room;
            int bestDistance = 1000000;
            for (const auto& defender : player.crew) {
                if (!defender.alive || defender.room < 0) continue;
                const int distance = std::abs(defender.room - boarder.room);
                if (distance < bestDistance) {
                    bestDistance = distance;
                    targetRoom = defender.room;
                }
            }

            if (targetRoom != boarder.room) {
                std::unordered_map<int, int> parent;
                std::vector<int> queue;
                queue.push_back(boarder.room);
                parent[boarder.room] = -1;
                for (std::size_t head = 0; head < queue.size(); ++head) {
                    const int current = queue[head];
                    if (current == targetRoom) break;
                    for (int doorIndex = 0;
                         doorIndex < static_cast<int>(player.content.layout.doors.size());
                         ++doorIndex) {
                        if (doorIndex >= static_cast<int>(player.doorOpen.size()) ||
                            !player.doorOpen[doorIndex])
                            continue;
                        const auto& door = player.content.layout.doors[doorIndex];
                        int next = -1;
                        if (door.leftRoom == current) next = door.rightRoom;
                        else if (door.rightRoom == current) next = door.leftRoom;
                        if (next < 0 || parent.find(next) != parent.end()) continue;
                        parent[next] = current;
                        queue.push_back(next);
                    }
                }
                if (parent.find(targetRoom) != parent.end()) {
                    int step = targetRoom;
                    while (parent[step] != -1 && parent[step] != boarder.room)
                        step = parent[step];
                    if (parent[step] == boarder.room)
                        boarder.room = step;
                }
            }

            int defenders = 0;
            for (auto& crew : player.crew) {
                if (crew.alive && crew.room == boarder.room) {
                    crew.health = std::max(0, crew.health - 12);
                    if (crew.health == 0) crew.alive = false;
                    ++defenders;
                }
            }
            if (defenders > 0) boarder.health = std::max(0, boarder.health - defenders * 18);
            if (boarder.health == 0) boarder.alive = false;
            if (defenders == 0)
                player.damageSystemInRoom(boarder.room, 1);
        }
        boardingFightTimer_ = 1.0f;
        boarders.erase(std::remove_if(boarders.begin(), boarders.end(),
            [](const RuntimeCrew& crew) { return !crew.alive; }), boarders.end());
    }

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
            if (fromPlayer) playerDeployedCombatDrone_ = true;
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

    if (&target == &player && cloaked()) {
        result.evaded = std::max(1, weapon.shots);
        return result;
    }

    const int shieldPiercing = std::max(0, weapon.shieldPiercing);
    for (int shot = 0; shot < weapon.shots; ++shot) {
        // Missile/bomb weapons bypass shields, and therefore do not use the
        // normal projectile evasion check here. Other projectiles can be
        // avoided based on the target's powered engines and manned piloting.
        const bool shieldBypass = weapon.missilesUsed > 0;
        if (!shieldBypass) {
            int dodgeChance = 0;
            for (const auto& system : target.systems) {
                if (system.type == "engines" && system.powered)
                    dodgeChance += system.power * 5;
            }
            for (const auto& system : target.systems) {
                if (system.type != "pilot" || !system.powered || system.room < 0)
                    continue;
                const bool piloted = std::any_of(target.crew.begin(), target.crew.end(),
                    [&system](const RuntimeCrew& crew) {
                        return crew.alive && crew.room == system.room;
                    });
                if (piloted) dodgeChance += 5;
            }
            dodgeChance = std::clamp(dodgeChance, 0, 95);
            if (dodgeChance > 0 && (nextRandom() % 100u) < static_cast<std::uint32_t>(dodgeChance)) {
                ++result.evaded;
                continue;
            }
        }

        // Phase 3 starts with a Zoltan-style super shield. Unlike ordinary
        // shields it is hit-count based and must be depleted before normal
        // shields or missile bypass rules can matter.
        if (!shieldBypass && superShield_ > 0) {
            --superShield_;
            target.shieldCharge = 0.0f;
            ++result.shieldsAbsorbed;
            continue;
        }
        if (shieldBypass && superShield_ > 0) {
            --superShield_;
            target.shieldCharge = 0.0f;
            ++result.shieldsAbsorbed;
            continue;
        }

        // Missile/bomb weapons bypass shields in FTL. Laser/beam/ion-style
        // weapons must first overcome the target's shield layers.
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
        if (weapon.ionDamage > 0) {
            const bool reverseIonNegated = (&target == &player &&
                (nextRandom() % 100u) < 20u);
            if (!reverseIonNegated)
                result.ionDamage += target.ionizeSystemInRoom(room, weapon.ionDamage);
        }
        if (weapon.personnelDamage > 0)
            result.personnelDamage += target.damageCrewInRoom(room, weapon.personnelDamage);

        // FTL weapons can apply secondary effects after a successful impact.
        // Keep these effects tied to the same projectile resolution so shields,
        // evasion and damage all use one deterministic hit outcome.
        if (weapon.fireChance > 0 && target.roomOxygen[room] > 0 &&
            (nextRandom() % 100u) < static_cast<std::uint32_t>(weapon.fireChance)) {
            if (target.setRoomFire(room, true))
                ++result.firesStarted;
        }
        if (weapon.breachChance > 0 &&
            (nextRandom() % 100u) < static_cast<std::uint32_t>(weapon.breachChance)) {
            if (target.setRoomBreach(room, true))
                ++result.breachesStarted;
        }
        if (weapon.stunChance > 0 && weapon.stunDuration > 0 &&
            (nextRandom() % 100u) < static_cast<std::uint32_t>(weapon.stunChance)) {
            result.systemsStunned += target.stunSystemsInRoom(
                room, static_cast<float>(weapon.stunDuration));
        }

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
    if (cloaked() && !stealthWeapons_)
        cloakTimer_ = 0.0f;
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


bool CombatRuntime::activateCloaking() {
    if (!player.valid) return false;
    bool hasCloaking = false;
    for (const auto& system : player.systems) {
        if (system.type == "cloaking" && system.maxPower > 0) { hasCloaking = true; break; }
    }
    if (!hasCloaking) return false;
    if (cloakTimer_ > 0.0f) return false;
    cloakTimer_ = 5.0f;
    return true;
}

}
