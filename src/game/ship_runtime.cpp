#include "game/ship_runtime.hpp"
#include <algorithm>
#include <queue>
#include <vector>

namespace wormhole {

bool ShipRuntime::load(ShipContent& source) {
    const LoadedShip* loaded = source.playerShip();
    if (!loaded) return false;

    content = *loaded;
    maxHull = std::max(0, content.blueprint.maxHealth);
    hull = maxHull;
    reactor = std::max(0, content.blueprint.startingReactorPower);

    roomDamage.assign(content.layout.rooms.size(), 0);
    roomOxygen.assign(content.layout.rooms.size(), 100);
    roomFire.assign(content.layout.rooms.size(), false);

    systems.clear();
    for (const auto& blueprint : content.blueprint.systems) {
        RuntimeSystem system;
        system.type = blueprint.system;
        system.room = blueprint.room;
        system.level = std::max(0, blueprint.level);
        system.power = std::max(0, blueprint.startingPower);
        if (system.power == 0) system.power = system.level;
        system.maxPower = std::max(system.power, blueprint.maxPower);
        system.powered = blueprint.availableByDefault && system.power > 0;
        systems.push_back(std::move(system));
    }

    doorOpen.assign(content.layout.doors.size(), false);

    crew.clear();
    for (const auto& blueprint : content.blueprint.crew) {
        RuntimeCrew member;
        member.race = blueprint.race;
        member.name = blueprint.name;
        member.room = blueprint.room;
        crew.push_back(std::move(member));
    }

    valid = true;
    return true;
}

void ShipRuntime::reset() {
    content = {};
    hull = maxHull = reactor = 0;
    roomDamage.clear();
    roomOxygen.clear();
    roomFire.clear();
    systems.clear();
    crew.clear();
    doorOpen.clear();
    valid = false;
}

bool ShipRuntime::damageRoom(int roomId, int amount) {
    if (!valid || roomId < 0 || roomId >= static_cast<int>(roomDamage.size()) || amount <= 0) return false;
    roomDamage[roomId] += amount;
    hull = std::max(0, hull - amount);
    return true;
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

bool ShipRuntime::setRoomFire(int roomId, bool fire) {
    if (!valid || roomId < 0 || roomId >= static_cast<int>(roomFire.size())) return false;
    if (roomFire[roomId] == fire) return false;
    roomFire[roomId] = fire;
    return true;
}

void ShipRuntime::updateEnvironment(float dt) {
    if (!valid || dt <= 0.f) return;

    for (int i = 0; i < static_cast<int>(roomFire.size()); ++i) {
        if (!roomFire[i]) continue;
        roomOxygen[i] = std::max(0, roomOxygen[i] - static_cast<int>(dt * 8.f));
        if (roomOxygen[i] == 0) {
            roomFire[i] = false;
            damageRoom(i, 1);
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

int ShipRuntime::usedReactorPower() const {
    int used = 0;
    for (const auto& system : systems)
        if (system.powered) used += std::max(0, system.power);
    return used;
}

int ShipRuntime::availableReactorPower() const {
    return std::max(0, reactor - usedReactorPower());
}

}