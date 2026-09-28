#include "game/ship_runtime.hpp"
#include <algorithm>
#include <cctype>
#include <queue>
#include <random>
#include <vector>

namespace wormhole {

bool ShipRuntime::load(ShipContent& source) {
    const LoadedShip* loaded = source.playerShip();
    return loaded ? load(*loaded) : false;
}

bool ShipRuntime::load(const LoadedShip& loaded) {
    reset();

    content = loaded;
    maxHull = std::max(0, content.blueprint.maxHealth);
    hull = maxHull;
    reactor = std::max(0, content.blueprint.startingReactorPower);

    roomDamage.assign(content.layout.rooms.size(), 0);
    roomOxygen.assign(content.layout.rooms.size(), 100);
    roomFire.assign(content.layout.rooms.size(), false);
    roomBreach.assign(content.layout.rooms.size(), false);
    roomFireDamageTimer.assign(content.layout.rooms.size(), 0.0f);

    systems.clear();
    for (const auto& blueprint : content.blueprint.systems) {
        RuntimeSystem system;
        system.type = blueprint.system;
        system.room = blueprint.room;
        system.level = std::max(0, blueprint.level);
        system.power = std::max(0, blueprint.startingPower);
        if (system.power == 0) system.power = system.level;
        system.maxPower = std::max(system.power, blueprint.maxPower);
        system.damage = 0;
        system.batteryPower = 0;
        system.zoltanPower = 0;
        system.ionDamage = 0;
        system.ionRemovedPower = 0;
        system.ionTimer = 0.0f;
        system.ionDisabled = false;
        system.stunTimer = 0.0f;
        system.breached = false;
        system.powered = blueprint.availableByDefault && system.power > 0;
        systems.push_back(std::move(system));
    }

    // Doors start closed in the runtime; the player can open them with the door control.
    doorOpen.assign(content.layout.doors.size(), false);

    missiles = std::max(0, content.blueprint.startingMissiles);
    shieldLayers = 0;
    maxShieldLayers = 0;
    shieldCharge = 0.0f;
    for (const auto& system : systems) {
        if (system.type == "shields") {
            maxShieldLayers = std::max(maxShieldLayers, system.power);
            if (system.powered) shieldLayers = maxShieldLayers;
        }
    }

    drones.clear();
    for (const auto& blueprint : content.initialDroneBlueprints) {
        RuntimeDrone drone;
        drone.type = blueprint.type;
        drone.name = blueprint.name;
        drone.power = std::max(1, blueprint.power);
        drone.speed = std::max(0, blueprint.speed);
        drone.cooldown = std::max(0, blueprint.cooldown);
        drone.dodge = std::clamp(blueprint.dodge, 0, 100);
        drone.defenceTarget = blueprint.defenceTarget;
        drone.weaponCooldown = std::max(0.1f, blueprint.weaponCooldown);
        drone.weaponShots = std::max(1, blueprint.weaponShots);
        drone.weaponDamage = std::max(0, blueprint.weaponDamage);
        drone.weaponSystemDamage = std::max(0, blueprint.weaponSystemDamage);
        drone.weaponIonDamage = std::max(0, blueprint.weaponIonDamage);
        drone.weaponShieldPiercing = std::max(0, blueprint.weaponShieldPiercing);
        drone.weaponPersonnelDamage = std::max(0, blueprint.weaponPersonnelDamage);
        drone.weaponSpeed = std::max(0, blueprint.speed);
        drone.weaponCharge = 0.0f;
        drone.charge = 0;
        drone.powered = false;
        drone.active = false;
        drones.push_back(std::move(drone));
    }

    weapons.clear();
    for (const auto& blueprint : content.initialWeaponBlueprints) {
        RuntimeWeapon weapon;
        weapon.name = blueprint.name;
        weapon.type = blueprint.type;
        weapon.power = std::max(1, blueprint.power);
        weapon.cooldown = std::max(0.1f, blueprint.cooldown);
        weapon.speed = std::max(0, blueprint.speed);
        weapon.shots = std::max(1, blueprint.shots);
        weapon.damage = std::max(0, blueprint.damage);
        weapon.systemDamage = std::max(0, blueprint.systemDamage);
        weapon.ionDamage = std::max(0, blueprint.ionDamage);
        weapon.shieldPiercing = std::max(0, blueprint.shieldPiercing);
        weapon.missilesUsed = std::max(0, blueprint.missilesUsed);
        weapon.personnelDamage = std::max(0, blueprint.personnelDamage);
        weapon.hullBust = std::max(0, blueprint.hullBust);
        weapon.fireChance = std::clamp(blueprint.fireChance, 0, 100);
        weapon.breachChance = std::clamp(blueprint.breachChance, 0, 100);
        weapon.stunChance = std::clamp(blueprint.stunChance, 0, 100);
        weapon.stunDuration = std::max(0, blueprint.stunDuration);
        weapons.push_back(std::move(weapon));
    }
    weaponIonDisabled.assign(weapons.size(), false);

    crew.clear();
    for (const auto& blueprint : content.blueprint.crew) {
        RuntimeCrew member;
        member.race = blueprint.race;
        member.name = blueprint.name;
        member.room = blueprint.room;
        member.maxHealth = 100;
        member.health = member.maxHealth;
        member.alive = true;
        crew.push_back(std::move(member));
    }

    valid = true;
    return true;
}

void ShipRuntime::updateWeapons(float dt, float cooldownMultiplier) {
    if (!valid || dt <= 0.f) return;

    int weaponSystemPower = 0;
    for (const auto& system : systems) {
        if (system.type == "weapons" && system.powered && system.stunTimer <= 0.0f)
            weaponSystemPower = std::max(weaponSystemPower, system.power);
    }
    if (weaponSystemPower <= 0) {
        for (auto& weapon : weapons) weapon.allocatedPower = 0;
        return;
    }

    // Weapon slots consume the shared Weapons-system power cumulatively.
    // Keep the loaded slot order as the runtime's current weapon ordering.
    int remainingPower = weaponSystemPower;
    for (int i = 0; i < static_cast<int>(weapons.size()); ++i) {
        auto& weapon = weapons[i];
        weapon.allocatedPower = 0;
        const bool ionDisabled = i < static_cast<int>(weaponIonDisabled.size()) &&
                                 weaponIonDisabled[i];
        if (ionDisabled || remainingPower < weapon.power)
            continue;

        weapon.allocatedPower = weapon.power;
        remainingPower -= weapon.power;
        if (weapon.ready) continue;

        const float rate = std::max(0.0f, cooldownMultiplier);
        weapon.charge = std::min(weapon.cooldown,
            weapon.charge + dt * rate);
        if (weapon.charge >= weapon.cooldown)
            weapon.ready = true;
    }
}

void ShipRuntime::updateShields(float dt, float rechargeMultiplier) {
    if (!valid || dt <= 0.0f) return;

    int targetLayers = 0;
    bool powered = false;
    for (const auto& system : systems) {
        if (system.type != "shields") continue;
        targetLayers = std::max(targetLayers, system.power);
        powered = powered || system.powered;
    }
    maxShieldLayers = std::max(0, targetLayers);
    if (!powered || maxShieldLayers <= 0) {
        shieldLayers = 0;
        shieldCharge = 0.0f;
        return;
    }
    if (shieldLayers > maxShieldLayers) shieldLayers = maxShieldLayers;
    if (shieldLayers >= maxShieldLayers) {
        shieldCharge = 0.0f;
        return;
    }

    shieldCharge += dt;
    constexpr float rechargeSeconds = 2.0f;
    while (shieldCharge >= rechargeSeconds && shieldLayers < maxShieldLayers) {
        shieldCharge -= rechargeSeconds;
        ++shieldLayers;
    }
}

bool ShipRuntime::damageShields(int amount) {
    if (!valid || amount <= 0 || shieldLayers <= 0) return false;
    const int absorbed = std::min(amount, shieldLayers);
    shieldLayers -= absorbed;
    shieldCharge = 0.0f;
    return absorbed > 0;
}

bool ShipRuntime::fireWeapon(int weaponIndex) {
    if (!valid || weaponIndex < 0 || weaponIndex >= static_cast<int>(weapons.size())) return false;
    RuntimeWeapon& weapon = weapons[weaponIndex];
    if (weaponIndex < static_cast<int>(weaponIonDisabled.size()) && weaponIonDisabled[weaponIndex]) return false;
    if (!weapon.ready) return false;

    int weaponSystemPower = 0;
    for (const auto& system : systems) {
        if (system.type == "weapons" && system.powered && system.stunTimer <= 0.0f)
            weaponSystemPower = std::max(weaponSystemPower, system.power);
    }
    // A weapon can only fire when its own slot is actually funded by the
    // shared Weapons-system allocation. Checking only total system power would
    // incorrectly allow a later weapon to fire while an earlier slot consumes
    // the available bars.
    if (weapon.allocatedPower < weapon.power) return false;

    // missilesUsed is the weapon's per-volley ammunition cost. Multi-shot
    // weapons still consume that single volley cost; shots describes how many
    // projectiles the volley produces rather than multiplying missile usage.
    if (weapon.missilesUsed > missiles) return false;

    missiles -= weapon.missilesUsed;
    weapon.charge = 0.0f;
    weapon.ready = false;
    weapon.volleyPending = true;
    return true;
}


int ShipRuntime::resolveWeaponVolley(int weaponIndex, int targetRoom, int targetEvasion) {
    if (targetRoom < 0) return 0;
    return resolveWeaponVolley(weaponIndex, std::vector<int>{targetRoom}, targetEvasion);
}

int ShipRuntime::resolveWeaponVolley(int weaponIndex, const std::vector<int>& projectileTargets, int targetEvasion) {
    if (!valid || weaponIndex < 0 || weaponIndex >= static_cast<int>(weapons.size()) ||
        projectileTargets.empty()) return 0;

    const int fallbackTargetRoom = projectileTargets.front();
    RuntimeWeapon& weapon = weapons[weaponIndex];
    if (!weapon.volleyPending || weapon.allocatedPower < weapon.power) return 0;
    if (weaponIndex < static_cast<int>(weaponIonDisabled.size()) && weaponIonDisabled[weaponIndex])
        return 0;

    // A volley contains the configured number of projectiles. Missile/bomb
    // ammunition was already consumed by fireWeapon(), so resolving the volley
    // never charges ammo again.
    const int shots = std::max(1, weapon.shots);
    const std::string type = weapon.type;
    int resolved = 0;

    auto lower = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    };
    const std::string kind = lower(type);
    const bool missileLike = kind.find("missile") != std::string::npos ||
                             kind.find("bomb") != std::string::npos;
    const bool ionLike = kind.find("ion") != std::string::npos;
    const bool beamLike = kind.find("beam") != std::string::npos;
    const bool bypassShields = missileLike;
    const int evasion = std::clamp(targetEvasion, 0, 100);
    // FTL resolves each projectile independently against the target's evasion.
    // Keep the RNG local to volley resolution so the result is not tied to frame timing.
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> roll(0, 99);

    for (int shot = 0; shot < shots; ++shot) {
        const int targetRoom = shot < static_cast<int>(projectileTargets.size())
            ? projectileTargets[shot]
            : fallbackTargetRoom;
        if (targetRoom < 0) {
            ++resolved;
            continue;
        }

        // Beams do not use the normal projectile hit/miss roll; they always connect.
        // Other weapon projectiles have a per-shot chance to miss equal to target evasion.
        if (!beamLike && roll(rng) < evasion) {
            ++resolved;
            continue;
        }
        if (ionLike) {
            // Ion shots that meet a normal shield apply their ion damage to the
            // shield system itself; otherwise they ionize the selected room.
            int shieldIndex = -1;
            for (int i = 0; i < static_cast<int>(systems.size()); ++i) {
                if (systems[i].type == "shields" && systems[i].maxPower > 0) {
                    shieldIndex = i;
                    break;
                }
            }
            if (shieldLayers > 0 && shieldIndex >= 0) {
                ionizeSystemInRoom(systems[shieldIndex].room, std::max(1, weapon.ionDamage));
            } else if (weapon.ionDamage > 0) {
                ionizeSystemInRoom(targetRoom, weapon.ionDamage);
            }
            ++resolved;
            continue;
        }

        if (!beamLike && !bypassShields && shieldLayers > 0) {
            // Lasers, flak and crystal-style projectiles remove one shield
            // layer per projectile. A crystal piercing value describes its
            // special ability, but the vanilla multi-layer behavior still
            // consumes the encountered shield layer before room damage.
            --shieldLayers;
            shieldCharge = 0.0f;
            ++resolved;
            continue;
        }

        if (beamLike) {
            // Beams never consume shield layers. Their damage is reduced by one
            // for every regular shield layer currently present.
            const int effectiveDamage = std::max(0, weapon.damage - shieldLayers);
            if (effectiveDamage > 0) {
                damageRoom(targetRoom, effectiveDamage);
                if (weapon.systemDamage > 0)
                    damageSystemInRoom(targetRoom, std::min(weapon.systemDamage, effectiveDamage));
                if (weapon.personnelDamage > 0)
                    damageCrewInRoom(targetRoom, weapon.personnelDamage);
            }
            ++resolved;
            continue;
        }

        // Shield-bypassing missiles/bombs and unblocked projectiles apply both
        // hull and system effects to the targeted room. A weapon's damage and
        // systemDamage are separate effects in the blueprint data.
        if (weapon.damage > 0)
            damageRoom(targetRoom, weapon.damage);
        if (weapon.systemDamage > 0)
            damageSystemInRoom(targetRoom, weapon.systemDamage);
        if (weapon.personnelDamage > 0)
            damageCrewInRoom(targetRoom, weapon.personnelDamage);
        if (weapon.ionDamage > 0)
            ionizeSystemInRoom(targetRoom, weapon.ionDamage);
        if (weapon.stunDuration > 0)
            stunSystemsInRoom(targetRoom, static_cast<float>(weapon.stunDuration));

        ++resolved;
    }

    // The charge was spent by fireWeapon(); this consumes the pending volley
    // when its projectiles have resolved in the combat runtime.
    weapon.volleyPending = false;
    return resolved;
}


bool ShipRuntime::interceptWeaponWithDefenseDrone(int weaponIndex) {
    if (!valid || weaponIndex < 0 || weaponIndex >= static_cast<int>(weapons.size())) return false;
    const std::string kind = [&]() {
        std::string value = weapons[weaponIndex].type;
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }();

    const bool bombLike = kind.find("bomb") != std::string::npos;
    if (bombLike) return false; // Bombs teleport directly into rooms and bypass Defense Drones.

    const bool missileLike = kind.find("missile") != std::string::npos;
    const bool flakLike = kind.find("flak") != std::string::npos;
    const bool crystalLike = kind.find("crystal") != std::string::npos;
    const bool laserLike = kind.find("laser") != std::string::npos;
    const bool ionLike = kind.find("ion") != std::string::npos;
    const bool markIEligible = missileLike || flakLike || crystalLike;
    const bool markIIEligible = markIEligible || laserLike || ionLike;
    if (!markIIEligible) return false;

    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> roll(0, 99);
    for (auto& drone : drones) {
        if (!drone.powered || !drone.active) continue;
        std::string name = drone.name;
        std::transform(name.begin(), name.end(), name.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (name.find("defense") == std::string::npos && name.find("defence") == std::string::npos)
            continue;

        const bool markII = name.find("mark ii") != std::string::npos ||
                            name.find("mk ii") != std::string::npos ||
                            name.find("defense ii") != std::string::npos ||
                            name.find("defence ii") != std::string::npos;
        if ((markII ? markIIEligible : markIEligible) && roll(rng) < 90) {
            // One defense-drone shot is consumed by one incoming projectile.
            drone.active = false;
            drone.weaponCharge = 0.0f;
            return true;
        }
    }
    return false;
}

void ShipRuntime::updateDrones(float dt) {
    if (!valid || dt <= 0.f) return;
    for (auto& drone : drones) {
        if (!drone.powered) {
            drone.active = false;
            continue;
        }
        const bool combatDrone = drone.type == DroneBlueprint::Type::Combat;
        const float cooldownSeconds = combatDrone ? drone.weaponCooldown
                                                  : static_cast<float>(std::max(0, drone.cooldown)) / 1000.0f;
        if (cooldownSeconds <= 0.0f) {
            drone.active = true;
            continue;
        }
        if (combatDrone) {
            drone.weaponCharge = std::min(cooldownSeconds, drone.weaponCharge + dt);
            drone.active = drone.weaponCharge >= cooldownSeconds;
        } else {
            drone.charge = std::min(drone.cooldown, drone.charge + static_cast<int>(dt * 1000.0f));
            drone.active = drone.charge >= drone.cooldown; 
        }
    }
}

bool ShipRuntime::setDronePowered(int droneIndex, bool powered) {
    if (!valid || droneIndex < 0 || droneIndex >= static_cast<int>(drones.size())) return false;
    RuntimeDrone& drone = drones[droneIndex];
    if (drone.powered == powered) return false;
    if (powered) {
        const int used = usedReactorPower();
        if (reactor - used < std::max(1, drone.power)) return false;
    }
    drone.powered = powered;
    if (!powered) { drone.active = false; drone.weaponCharge = 0.0f; }
    return true;
}

void ShipRuntime::reset() {
    content = {};
    hull = maxHull = reactor = 0;
    reactorPowerCap = -1;
    backupBatteryActivePower = 0;
    backupBatteryTimer = 0.0f;
    backupBatteryCooldownTimer = 0.0f;
    roomDamage.clear();
    roomOxygen.clear();
    roomFire.clear();
    roomBreach.clear();
    roomFireDamageTimer.clear();
    systems.clear();
    crew.clear();
    drones.clear();
    doorOpen.clear();
    weapons.clear();
    weaponIonDisabled.clear();
    missiles = 0;
    shieldLayers = 0;
    maxShieldLayers = 0;
    shieldCharge = 0.0f;
    valid = false;
}

bool ShipRuntime::damageRoom(int roomId, int amount) {
    if (!valid || roomId < 0 || roomId >= static_cast<int>(roomDamage.size()) || amount <= 0) return false;
    roomDamage[roomId] += amount;
    hull = std::max(0, hull - amount);
    return true;
}

int ShipRuntime::damageSystemInRoom(int roomId, int amount) {
    if (!valid || roomId < 0 || amount <= 0) return 0;
    int applied = 0;
    for (auto& system : systems) {
        if (system.room != roomId || system.maxPower <= 0) continue;
        const int remaining = std::max(0, system.maxPower - system.damage);
        const int hit = std::min(amount - applied, remaining);
        if (hit <= 0) continue;
        system.damage += hit;
        system.power = std::min(system.power, std::max(0, system.maxPower - system.damage - system.ionDamage));
        system.zoltanPower = std::min(system.zoltanPower, system.power);
        system.batteryPower = std::min(system.batteryPower, std::max(0, system.power - system.zoltanPower));

        // Losing Weapons-system power deallocates whole weapon slots from the
        // rightmost side. A partially funded weapon cannot remain active.
        if (system.type == "weapons") {
            int remainingPower = system.power;
            for (int weaponIndex = 0; weaponIndex < static_cast<int>(weapons.size()); ++weaponIndex) {
                auto& weapon = weapons[weaponIndex];
                const bool ionDisabled = weaponIndex < static_cast<int>(weaponIonDisabled.size()) &&
                                         weaponIonDisabled[weaponIndex];
                if (ionDisabled || remainingPower < weapon.power) {
                    weapon.allocatedPower = 0;
                    continue;
                }
                weapon.allocatedPower = weapon.power;
                remainingPower -= weapon.power;
            }
            for (int weaponIndex = static_cast<int>(weapons.size()) - 1; weaponIndex >= 0; --weaponIndex) {
                auto& weapon = weapons[weaponIndex];
                if (weapon.allocatedPower == 0) {
                    weapon.ready = false;
                    weapon.charge = 0.0f;
                }
            }
        }

        if (system.power == 0) system.powered = false;
        applied += hit;
        if (applied >= amount) break;
    }
    return applied;
}

int ShipRuntime::repairSystemInRoom(int roomId, int amount) {
    if (!valid || roomId < 0 || amount <= 0) return 0;
    int repaired = 0;
    for (auto& system : systems) {
        if (system.room != roomId || system.damage <= 0) continue;
        const int restored = std::min(amount - repaired, system.damage);
        if (restored <= 0) continue;
        system.damage -= restored;
        const int effectiveMax = std::max(0, system.maxPower - system.damage - system.ionDamage);
        system.power = std::min(effectiveMax, system.power + restored);
        repaired += restored;
        if (repaired >= amount) break;
    }
    return repaired;
}

int ShipRuntime::ionizeSystemInRoom(int roomId, int amount) {
    if (!valid || roomId < 0 || amount <= 0) return 0;

    int applied = 0;
    for (auto& system : systems) {
        if (system.room != roomId || system.maxPower <= 0) continue;

        // Ion damage stacks up to 5 points / 25 seconds.
        // Additional ion damage can still be recorded even after all normal
        // power has been forced out, but the lock itself caps at 25 seconds.
        const int hit = std::min(amount - applied, 5 - system.ionDamage);
        if (hit <= 0) continue;

        system.ionDamage += hit;
        system.ionTimer = std::min(25.0f, system.ionTimer + 5.0f * hit);

        // Ion damage can force out only normal power. Zoltan power is ion-proof.
        // Weapons are special: each ion point shuts down one whole active weapon,
        // starting from the rightmost slot. The weapon's normal (non-Zoltan)
        // power is removed, while any Zoltan-funded portion remains available.
        int removed = 0;
        if (system.type == "weapons" && !weapons.empty()) {
            int zoltanRemaining = std::max(0, system.zoltanPower);
            int poweredBudget = std::max(0, system.power);
            std::vector<int> zoltanForWeapon(weapons.size(), 0);
            for (std::size_t i = 0; i < weapons.size(); ++i) {
                const int allocated = std::min(weapons[i].power, poweredBudget);
                const int zoltanAllocated = std::min(allocated, zoltanRemaining);
                zoltanForWeapon[i] = zoltanAllocated;
                zoltanRemaining -= zoltanAllocated;
                poweredBudget -= allocated;
                if (allocated < weapons[i].power) break;
            }

            for (int i = static_cast<int>(weapons.size()) - 1; i >= 0; --i) {
                if (i < static_cast<int>(weaponIonDisabled.size()) && weaponIonDisabled[i]) continue;
                const int allocated = std::min(weapons[i].power, std::max(0, system.power));
                if (allocated < weapons[i].power) continue;
                const int normalWeaponPower = std::max(0, weapons[i].power - zoltanForWeapon[i]);
                if (normalWeaponPower <= 0) continue;
                if (i < static_cast<int>(weaponIonDisabled.size())) {
                    weaponIonDisabled[i] = true;
                    weapons[i].charge = 0.0f;
                    weapons[i].ready = false;
                }
                removed = std::min(normalWeaponPower,
                    std::max(0, system.power - system.zoltanPower));
                break;
            }
        } else {
            const int normalPower = std::max(0, system.power - system.zoltanPower);
            removed = std::min(hit, normalPower);
        }

        if (removed > 0) {
            const int batteryRemoved = std::min(removed, system.batteryPower);
            system.batteryPower -= batteryRemoved;
            const int reactorRemoved = removed - batteryRemoved;
            system.power = std::max(0, system.power - removed);
            system.ionRemovedPower += reactorRemoved + batteryRemoved;
        }

        if (system.power == 0) {
            system.ionDisabled = system.powered;
            system.powered = false;
        }

        applied += hit;
        if (applied >= amount) break;
    }
    return applied;
}

bool ShipRuntime::repairRoom(int roomId, int amount) {
    if (!valid || roomId < 0 || roomId >= static_cast<int>(roomDamage.size()) || amount <= 0) return false;
    const int restored = std::min(amount, roomDamage[roomId]);
    roomDamage[roomId] -= restored;
    hull = std::min(maxHull, hull + restored);
    return restored > 0;
}

bool ShipRuntime::setSystemPowered(int systemIndex, bool powered) {
    if (!valid || systemIndex < 0 || systemIndex >= static_cast<int>(systems.size())) return false;
    RuntimeSystem& system = systems[systemIndex];
    if (system.ionDamage > 0) return false;
    if (system.powered == powered) return true;
    if (powered) {
        // Existing Zoltan/battery allocations already belong to this system.
        // Only newly uncovered reactor-funded bars need capacity.
        const int allocatedFree = std::min(system.power,
            std::max(0, system.zoltanPower) + std::max(0, system.batteryPower));
        const int reactorNeeded = std::max(0, system.power - allocatedFree);
        const reactorCapacity = reactorPowerCap >= 0
            ? std::min(reactor, reactorPowerCap)
            : reactor;
        if (std::max(0, reactorCapacity - usedReactorPower()) < reactorNeeded)
            return false;
    }
    system.powered = powered;
    return true;
}

int ShipRuntime::addCrew(const RuntimeCrew& input) {
    if (!valid) return -1;
    int aliveCount = 0;
    for (const auto& member : crew) if (member.alive) ++aliveCount;
    if (aliveCount >= 8) return -1;

    RuntimeCrew member = input;
    member.alive = true;
    member.maxHealth = std::max(1, member.maxHealth);
    member.health = std::clamp(member.health, 1, member.maxHealth);
    if (member.room < 0 && !content.layout.rooms.empty())
        member.room = content.layout.rooms.front().id;

    for (std::size_t i = 0; i < crew.size(); ++i) {
        if (crew[i].alive) continue;
        crew[i] = member;
        rebalanceZoltanPowerSources();
        return static_cast<int>(i);
    }
    crew.push_back(std::move(member));
    rebalanceZoltanPowerSources();
    return static_cast<int>(crew.size() - 1);
}

int ShipRuntime::removeCrewByRace(const std::string& race, bool cloneIfPossible) {
    if (!valid) return 0;
    std::string wanted = race;
    std::transform(wanted.begin(), wanted.end(), wanted.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    auto hasPoweredClonebay = [&]() {
        for (const auto& system : systems) {
            if (system.type == "clonebay" && system.powered && system.damage < system.maxPower)
                return true;
        }
        return false;
    };

    for (auto& member : crew) {
        if (!member.alive) continue;
        std::string current = member.race;
        std::transform(current.begin(), current.end(), current.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (!wanted.empty() && current != wanted) continue;

        if (cloneIfPossible && hasPoweredClonebay()) {
            member.health = member.maxHealth;
            member.alive = true;
            if (member.room < 0 && !content.layout.rooms.empty())
                member.room = content.layout.rooms.front().id;
        } else {
            member.health = 0;
            member.alive = false;
            member.room = -1;
        }
        rebalanceZoltanPowerSources();
        return 1;
    }
    return 0;
}

int ShipRuntime::removeCrew(bool cloneIfPossible) {
    return removeCrewByRace({}, cloneIfPossible);
}

bool ShipRuntime::moveCrew(int crewIndex, int targetRoom) {
    if (!valid || crewIndex < 0 || crewIndex >= static_cast<int>(crew.size())) return false;
    if (targetRoom < 0 || targetRoom >= static_cast<int>(content.layout.rooms.size())) return false;

    RuntimeCrew& member = crew[crewIndex];
    if (!member.alive || member.room == targetRoom) return false;
    if (member.room < 0) {
        member.room = targetRoom;
        return true;
    }

    const int roomCount = static_cast<int>(content.layout.rooms.size());
    std::vector<bool> visited(roomCount, false);
    std::vector<int> queue;
    queue.reserve(roomCount);
    queue.push_back(member.room);
    if (member.room >= 0 && member.room < roomCount) visited[member.room] = true;

    for (std::size_t head = 0; head < queue.size(); ++head) {
        const int current = queue[head];
        for (int i = 0; i < static_cast<int>(content.layout.doors.size()); ++i) {
            const auto& door = content.layout.doors[i];
            if (i >= static_cast<int>(doorOpen.size()) || !doorOpen[i]) continue;

            int next = -1;
            if (door.leftRoom == current) next = door.rightRoom;
            else if (door.rightRoom == current) next = door.leftRoom;
            if (next < 0 || next >= roomCount || visited[next]) continue;

            visited[next] = true;
            if (next == targetRoom) {
                member.room = targetRoom;
                rebalanceZoltanPowerSources();
                return true;
            }
            queue.push_back(next);
        }
    }
    return false;
}

int ShipRuntime::damageCrewInRoom(int roomId, int amount) {
    if (!valid || roomId < 0 || amount <= 0) return 0;

    int applied = 0;
    for (auto& member : crew) {
        if (!member.alive || member.room != roomId) continue;
        const int damage = std::min(amount, member.health);
        member.health -= damage;
        applied += damage;
        if (member.health <= 0) {
            member.health = 0;
            member.alive = false;
        }
    }
    rebalanceZoltanPowerSources();
    return applied;
}

int ShipRuntime::stunSystemsInRoom(int roomId, float seconds) {
    if (!valid || roomId < 0 || seconds <= 0.0f) return 0;
    int affected = 0;
    for (auto& system : systems) {
        if (system.room != roomId) continue;
        system.stunTimer = std::max(system.stunTimer, seconds);
        ++affected;
    }
    return affected;
}

int ShipRuntime::healCrew(int crewIndex, int amount) {
    if (!valid || crewIndex < 0 || crewIndex >= static_cast<int>(crew.size()) || amount <= 0)
        return 0;

    RuntimeCrew& member = crew[crewIndex];
    if (!member.alive || member.health >= member.maxHealth) return 0;

    const int healed = std::min(amount, member.maxHealth - member.health);
    member.health += healed;
    return healed;
}

bool ShipRuntime::extinguishFire(int crewIndex) {
    if (!valid || crewIndex < 0 || crewIndex >= static_cast<int>(crew.size())) return false;
    const RuntimeCrew& member = crew[crewIndex];
    if (!member.alive || member.room < 0 || member.room >= static_cast<int>(roomFire.size())) return false;
    if (!roomFire[member.room]) return false;
    roomFire[member.room] = false;
    roomOxygen[member.room] = std::max(roomOxygen[member.room], 20);
    return true;
}

bool ShipRuntime::setDoorOpen(int doorIndex, bool open) {
    if (!valid || doorIndex < 0 || doorIndex >= static_cast<int>(doorOpen.size())) return false;
    if (doorOpen[doorIndex] == open) return false;
    doorOpen[doorIndex] = open;
    return true;
}

bool ShipRuntime::setSystemPower(int systemIndex, int power) {
    if (!valid || systemIndex < 0 || systemIndex >= static_cast<int>(systems.size())) return false;
    RuntimeSystem& system = systems[systemIndex];
    if (system.ionDamage > 0) return false;
    const int next = std::max(0, std::min(power, system.maxPower));
    if (next == system.power) return false;

    const int delta = next - system.power;
    if (delta > 0) {
        const int zoltanFree = availableZoltanPowerForSystem(system);
        const int zoltanAdded = std::min(delta, zoltanFree);
        int remaining = delta - zoltanAdded;
        system.zoltanPower += zoltanAdded;

        const int capacity = reactorPowerCap >= 0
            ? std::min(reactor, reactorPowerCap)
            : reactor;
        const int regularReactorFree = std::max(0, capacity - usedReactorPower());
        const int reactorAdded = std::min(remaining, regularReactorFree);
        remaining -= reactorAdded;

        int allocatedBattery = 0;
        if (remaining > 0) {
            int usedBattery = 0;
            for (const auto& other : systems)
                usedBattery += std::max(0, other.batteryPower);
            const int batteryFree = std::max(0, backupBatteryActivePower - usedBattery);
            if (remaining > batteryFree) {
                system.zoltanPower -= zoltanAdded;
                return false;
            }
            allocatedBattery = remaining;
        }
        system.batteryPower += allocatedBattery;
    } else {
        int removed = -delta;
        const int reactorFunded = reactorFundedPowerForSystem(system);
        const int reactorRemoved = std::min(removed, reactorFunded);
        removed -= reactorRemoved;

        const int batteryRemoved = std::min(removed, system.batteryPower);
        system.batteryPower -= batteryRemoved;
        removed -= batteryRemoved;

        const int zoltanRemoved = std::min(removed, system.zoltanPower);
        system.zoltanPower -= zoltanRemoved;
    }

    system.power = next;
    system.batteryPower = std::min(system.batteryPower,
                                   std::max(0, system.power - system.zoltanPower));
    system.zoltanPower = std::min(system.zoltanPower, std::max(0, system.power));
    if (system.power == 0) {
        system.powered = false;
        system.batteryPower = 0;
        system.zoltanPower = 0;
    } else {
        system.powered = true;
    }
    return true;
}

bool ShipRuntime::setRoomBreach(int roomId, bool breached) {
    if (!valid || roomId < 0 || roomId >= static_cast<int>(roomBreach.size())) return false;
    if (roomBreach[roomId] == breached) return false;
    roomBreach[roomId] = breached;
    for (auto& system : systems)
        if (system.room == roomId) system.breached = breached;
    return true;
}

bool ShipRuntime::setRoomFire(int roomId, bool fire) {
    if (!valid || roomId < 0 || roomId >= static_cast<int>(roomFire.size())) return false;
    if (roomFire[roomId] == fire) return false;
    roomFire[roomId] = fire;
    return true;
}

void ShipRuntime::updateEnvironment(float dt) {
    if (!valid || dt <= 0.f) return;

    // Ion damage temporarily removes system power for five seconds.
    for (auto& system : systems) {
        if (system.stunTimer > 0.0f)
            system.stunTimer = std::max(0.0f, system.stunTimer - dt);
        if (system.ionDamage <= 0 || system.ionTimer <= 0.0f) continue;
        system.ionTimer = std::max(0.0f, system.ionTimer - dt);
        if (system.ionTimer > 0.0f) continue;

        const int restoreTarget = system.ionRemovedPower;
        const bool restorePowered = system.ionDisabled;
        system.ionDamage = 0;
        system.ionRemovedPower = 0;
        system.ionDisabled = false;
        if (system.type == "weapons") {
            for (int weaponIndex = 0; weaponIndex < static_cast<int>(weaponIonDisabled.size()); ++weaponIndex)
                weaponIonDisabled[weaponIndex] = false;
        }

        // The lost normal bars return to the reactor while ionized. When the
        // lock expires, FTL attempts to restore the system's previous power,
        // subject to the reactor/Battery power currently available.
        int remaining = std::min(restoreTarget,
            std::max(0, system.maxPower - system.damage - system.power));
        const int capacity = reactorPowerCap >= 0
            ? std::min(reactor, reactorPowerCap)
            : reactor;
        const int reactorFree = std::max(0, capacity - usedReactorPower());
        const int reactorAdded = std::min(remaining, reactorFree);
        system.power += reactorAdded;
        remaining -= reactorAdded;

        if (remaining > 0 && backupBatteryActivePower > 0) {
            int usedBattery = 0;
            for (const auto& other : systems)
                usedBattery += std::max(0, other.batteryPower);
            const int batteryFree = std::max(0, backupBatteryActivePower - usedBattery);
            const int batteryAdded = std::min(remaining, batteryFree);
            system.power += batteryAdded;
            system.batteryPower += batteryAdded;
            remaining -= batteryAdded;
        }

        if (restorePowered && system.power > 0)
            system.powered = true;
    }

    for (int i = 0; i < static_cast<int>(roomFire.size()); ++i) {
        if (roomFire[i]) {
            roomOxygen[i] = std::max(0, roomOxygen[i] - static_cast<int>(dt * 8.f));
            roomFireDamageTimer[i] += dt;
            while (roomFireDamageTimer[i] >= 1.0f) {
                damageCrewInRoom(i, 10);
                roomFireDamageTimer[i] -= 1.0f;
            }
            if (roomOxygen[i] == 0) {
                roomFire[i] = false;
                roomFireDamageTimer[i] = 0.0f;
                damageRoom(i, 1);
            }
        } else {
            roomFireDamageTimer[i] = 0.0f;
        }

        if (roomBreach[i])
            roomOxygen[i] = std::max(0, roomOxygen[i] - static_cast<int>(dt * 12.f));
    }

    // Crew inside a powered, undamaged medbay recover health over time.
    // This runs for both normal ship management and combat because the combat
    // runtime advances the same ShipRuntime environment each frame.
    for (const auto& system : systems) {
        if (system.type != "medbay" || !system.powered || system.power <= 0 || system.damage >= system.maxPower)
            continue;
        const int roomId = system.room;
        if (roomId < 0 || roomId >= static_cast<int>(roomFire.size()) ||
            roomFire[roomId] || roomBreach[roomId] || roomOxygen[roomId] <= 0)
            continue;
        constexpr float healPerSecondPerPower = 4.0f;
        const int healAmount = std::max(1, static_cast<int>(healPerSecondPerPower * system.power * dt));
        for (int crewIndex = 0; crewIndex < static_cast<int>(crew.size()); ++crewIndex) {
            if (crew[crewIndex].alive && crew[crewIndex].room == roomId)
                healCrew(crewIndex, healAmount);
        }
    }

    // Fire can spread through open doors into oxygenated rooms.
    const std::vector<bool> fireBefore = roomFire;
    for (int i = 0; i < static_cast<int>(content.layout.doors.size()); ++i) {
        if (i >= static_cast<int>(doorOpen.size()) || !doorOpen[i]) continue;
        const auto& door = content.layout.doors[i];
        if (door.leftRoom < 0 || door.rightRoom < 0) continue;
        if (door.leftRoom >= static_cast<int>(roomFire.size()) || door.rightRoom >= static_cast<int>(roomFire.size())) continue;

        const bool leftFire = fireBefore[door.leftRoom];
        const bool rightFire = fireBefore[door.rightRoom];
        if (leftFire && roomOxygen[door.rightRoom] > 0)
            roomFire[door.rightRoom] = true;
        if (rightFire && roomOxygen[door.leftRoom] > 0)
            roomFire[door.leftRoom] = true;
    }
}

void ShipRuntime::rebalanceZoltanPowerSources() {
    if (!valid) return;

    const auto isZoltan = [](const RuntimeCrew& member) {
        if (!member.alive || member.room < 0) return false;
        std::string race = member.race;
        std::transform(race.begin(), race.end(), race.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return race == "zoltan" || race == "energy";
    };

    // A Zoltan leaving a room removes only its own free bar. The system does
    // not automatically reclaim reactor power for that missing bar.
    for (auto& system : systems) {
        if (system.zoltanPower <= 0 || system.room < 0) continue;
        int roomZoltans = 0;
        for (const auto& member : crew)
            if (isZoltan(member) && member.room == system.room) ++roomZoltans;

        int usedElsewhere = 0;
        for (const auto& other : systems) {
            if (&other != &system && other.room == system.room)
                usedElsewhere += std::max(0, other.zoltanPower);
        }
        const int allowed = std::max(0, roomZoltans - usedElsewhere);
        if (system.zoltanPower > allowed) {
            const int lost = system.zoltanPower - allowed;
            system.zoltanPower = allowed;
            system.power = std::max(0, system.power - lost);
            system.batteryPower = std::min(system.batteryPower,
                                           std::max(0, system.power - system.zoltanPower));
            if (system.power == 0) system.powered = false;
        }
    }

    // Newly available Zoltan power is assigned to eligible powered main
    // systems. If the target is already full, the game pushes Battery-funded
    // bars out first, then reactor-funded bars, without lowering total power.
    for (auto& system : systems) {
        if (!system.powered || system.power <= 0 || system.room < 0 ||
            system.type == "pilot" || system.type == "engines" || system.type == "oxygen" ||
            system.type == "doors" || system.type == "sensors" || system.type == "battery")
            continue;

        const int free = availableZoltanPowerForSystem(system);
        if (free <= 0) continue;

        const int add = std::min(free, system.maxPower - system.zoltanPower);
        if (add <= 0) continue;

        // If the system is not full, the Zoltan adds a new power bar.
        // If it is full, it replaces Battery bars first, then reactor bars.
        const int newBars = std::min(add, std::max(0, system.maxPower - system.power));
        int replacement = add - newBars;
        if (replacement > 0) {
            const int pushedBattery = std::min(replacement, system.batteryPower);
            system.batteryPower -= pushedBattery;
            replacement -= pushedBattery;
        }
        if (replacement > 0) {
            const int reactorBars = reactorFundedPowerForSystem(system);
            replacement -= std::min(replacement, reactorBars);
        }

        system.power += newBars;
        system.zoltanPower += add;

        system.batteryPower = std::min(system.batteryPower,
                                       std::max(0, system.power - system.zoltanPower));
    }
}

int ShipRuntime::availableZoltanPowerForSystem(const RuntimeSystem& system) const {
    if (system.room < 0 ||
        system.type == "pilot" || system.type == "engines" || system.type == "oxygen" ||
        system.type == "doors" || system.type == "sensors" || system.type == "battery")
        return 0;

    const auto isZoltan = [](const RuntimeCrew& member) {
        if (!member.alive || member.room < 0)
            return false;
        std::string race = member.race;
        std::transform(race.begin(), race.end(), race.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return race == "zoltan" || race == "energy";
    };

    int total = 0;
    for (const auto& member : crew)
        if (isZoltan(member) && member.room == system.room)
            ++total;

    int allocated = 0;
    for (const auto& other : systems) {
        if (&other == &system || other.room != system.room)
            continue;
        allocated += std::max(0, other.zoltanPower);
    }
    allocated += std::max(0, system.zoltanPower);
    return std::max(0, total - allocated);
}

int ShipRuntime::zoltanPowerForSystem(const RuntimeSystem& system) const {
    // Explicit source allocation: do not infer the source of each bar from
    // current crew placement. This keeps Zoltan-funded bars stable while
    // Backup Battery and reactor capacity change.
    if (!system.powered || system.power <= 0 || system.type == "pilot" ||
        system.type == "engines" || system.type == "oxygen" ||
        system.type == "doors" || system.type == "sensors" ||
        system.type == "battery")
        return 0;
    return std::min(system.power, std::max(0, system.zoltanPower));
}

int ShipRuntime::reactorFundedPowerForSystem(const RuntimeSystem& system) const {
    if (!system.powered || system.power <= 0 || system.type == "battery")
        return 0;
    return std::max(0, system.power - std::max(0, system.zoltanPower) -
                       std::max(0, system.batteryPower));
}

int ShipRuntime::usedReactorPower() const {
    int used = 0;
    for (const auto& system : systems)
        used += reactorFundedPowerForSystem(system);
    for (const auto& drone : drones)
        if (drone.powered) used += std::max(1, drone.power);
    return used;
}

void ShipRuntime::setReactorPowerCap(int cap) {
    reactorPowerCap = cap < 0 ? -1 : std::min(cap, reactor);
}

bool ShipRuntime::activateBackupBattery() {
    if (!valid || backupBatteryActivePower > 0 || backupBatteryCooldownTimer > 0.0f)
        return false;

    const RuntimeSystem* battery = nullptr;
    for (const auto& system : systems) {
        if (system.type == "battery" && system.damage < system.maxPower &&
            system.maxPower > 0) {
            battery = &system;
            break;
        }
    }
    if (!battery)
        return false;

    // Backup Battery I supplies +2 power; level II supplies +4.
    // The subsystem itself is not reactor-funded.
    backupBatteryActivePower = battery->level >= 2 ? 4 : 2;
    backupBatteryTimer = 30.0f;
    return true;
}

void ShipRuntime::updateBackupBattery(float dt) {
    if (!valid || dt <= 0.0f)
        return;

    if (backupBatteryCooldownTimer > 0.0f) {
        backupBatteryCooldownTimer = std::max(0.0f, backupBatteryCooldownTimer - dt);
    }

    if (backupBatteryActivePower <= 0)
        return;

    backupBatteryTimer = std::max(0.0f, backupBatteryTimer - dt);
    if (backupBatteryTimer > 0.0f)
        return;

    // Temporary battery-funded bars disappear when the work cycle ends.
    for (auto& system : systems) {
        if (system.batteryPower <= 0) continue;
        system.power = std::max(0, system.power - system.batteryPower);
        system.batteryPower = 0;
        if (system.power == 0) system.powered = false;
    }
    backupBatteryActivePower = 0;
    backupBatteryCooldownTimer = 20.0f;
}

int ShipRuntime::availableReactorPower() const {
    const int capacity = reactorPowerCap >= 0
        ? std::min(reactor, reactorPowerCap)
        : reactor;
    return std::max(0, capacity + backupBatteryActivePower - usedReactorPower());
}

}