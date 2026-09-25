#include "game/ship_runtime.hpp"
#include <algorithm>
#include <queue>
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
        system.ionDamage = 0;
        system.ionTimer = 0.0f;
        system.ionDisabled = false;
        system.stunTimer = 0.0f;
        system.breached = false;
        system.powered = blueprint.availableByDefault && system.power > 0;
        systems.push_back(std::move(system));
    }

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

void ShipRuntime::updateWeapons(float dt) {
    if (!valid || dt <= 0.f) return;

    int weaponSystemPower = 0;
    for (const auto& system : systems) {
        if (system.type == "weapons" && system.powered && system.stunTimer <= 0.0f)
            weaponSystemPower = std::max(weaponSystemPower, system.power);
    }
    if (weaponSystemPower <= 0) return;

    for (auto& weapon : weapons) {
        if (weapon.power > weaponSystemPower || weapon.ready) continue;
        weapon.charge = std::min(weapon.cooldown, weapon.charge + dt);
        if (weapon.charge >= weapon.cooldown) weapon.ready = true;
    }
}

void ShipRuntime::updateShields(float dt) {
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
    if (!weapon.ready) return false;

    int weaponSystemPower = 0;
    for (const auto& system : systems) {
        if (system.type == "weapons" && system.powered)
            weaponSystemPower = std::max(weaponSystemPower, system.power);
    }
    if (weaponSystemPower < weapon.power) return false;
    if (weapon.missilesUsed > missiles) return false;

    missiles -= weapon.missilesUsed;
    weapon.charge = 0.0f;
    weapon.ready = false;
    return true;
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
        if (system.power == 0) system.powered = false;
        applied += hit;
        if (applied >= amount) break;
    }
    return applied;
}

int ShipRuntime::ionizeSystemInRoom(int roomId, int amount) {
    if (!valid || roomId < 0 || amount <= 0) return 0;

    int applied = 0;
    for (auto& system : systems) {
        if (system.room != roomId || system.maxPower <= 0) continue;

        const int remaining = std::max(0, system.maxPower - system.damage - system.ionDamage);
        const int hit = std::min(amount - applied, remaining);
        if (hit <= 0) continue;

        system.ionDamage += hit;
        system.ionTimer = 5.0f;
        system.power = std::min(system.power,
                                std::max(0, system.maxPower - system.damage - system.ionDamage));
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
    if (powered && availableReactorPower() < system.power) return false;
    system.powered = powered;
    return true;
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
    const int next = std::max(0, std::min(power, system.maxPower));
    if (next == system.power) return false;

    const int delta = next - system.power;
    if (delta > 0 && availableReactorPower() < delta) return false;

    system.power = next;
    if (system.power == 0) system.powered = false;
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

        const int restored = system.ionDamage;
        const bool restorePowered = system.ionDisabled;
        system.ionDamage = 0;
        system.ionDisabled = false;
        const int effectiveMax = std::max(0, system.maxPower - system.damage);
        system.power = std::min(effectiveMax, system.power + restored);
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

int ShipRuntime::usedReactorPower() const {
    int used = 0;
    for (const auto& system : systems)
        if (system.powered) used += std::max(0, system.power);
    return used;
}

int ShipRuntime::availableReactorPower() const {
    int used = 0;
    for (const auto& system : systems)
        if (system.powered) used += std::max(0, system.power);
    for (const auto& drone : drones)
        if (drone.powered) used += std::max(1, drone.power);
    return std::max(0, reactor - used);
}

}