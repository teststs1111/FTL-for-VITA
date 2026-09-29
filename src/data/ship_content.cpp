#include "data/ship_content.hpp"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <set>
#include <string>


namespace wormhole {
namespace {

unsigned enemyNextRandom(unsigned& rng) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

int enemyEffectiveSector(int sector, int difficulty) {
    const int s = std::clamp(sector, 1, 8);
    return std::clamp(s - (difficulty == 0 ? 1 : 0), 1, 8);
}

int enemyOptionalChance(int effectiveSector, int difficulty) {
    int chance = 20 + (effectiveSector - 1) * 10;
    if (difficulty == 0) chance -= 10;
    if (difficulty == 2) chance += 10;
    return std::clamp(chance, 10, 100);
}

struct EnemyBudget { int offensive; int defensive; int general; };

EnemyBudget enemyBudget(int sector, int difficulty) {
    static constexpr int table[8][3][3] = {
        {{1,1,1},{1,2,2},{1,1,2}},
        {{1,2,2},{2,3,3},{1,1,2}},
        {{2,3,3},{3,4,4},{1,1,2}},
        {{3,4,4},{4,5,5},{1,2,3}},
        {{4,5,5},{5,6,6},{2,2,3}},
        {{5,6,6},{6,7,7},{2,2,3}},
        {{6,7,7},{7,8,8},{2,3,4}},
        {{7,8,8},{8,9,9},{3,3,4}}
    };
    const int s = std::clamp(sector, 1, 8) - 1;
    const int d = std::clamp(difficulty, 0, 2);
    return {table[s][0][d], table[s][1][d], table[s][2][d]};
}

bool enemyOffensive(const std::string& type) {
    return type == "weapons" || type == "drones" || type == "teleporter";
}

bool enemyDefensive(const std::string& type) {
    return type == "shields" || type == "engines";
}

void generateEnemySystems(ShipBlueprint& ship, int sector, int difficulty, unsigned& rng) {
    const int effectiveSector = enemyEffectiveSector(sector, difficulty);
    const int optionalChance = enemyOptionalChance(effectiveSector, difficulty);
    EnemyBudget budget = enemyBudget(effectiveSector, difficulty);

    // Keep the rolled maximum separate from the blueprint's canonical hard cap.
    // Runtime maxPower remains the actual installed-system capacity.
    std::vector<int> rolledMax(ship.systems.size(), 0);
    for (std::size_t i = 0; i < ship.systems.size(); ++i) {
        auto& system = ship.systems[i];
        const int minimum = std::max(0, system.minPower);
        const int maximum = std::max(minimum, system.maxPower);
        const int span = maximum - minimum;
        const int progress = (span * (effectiveSector - 1)) / 7;
        const int bonusMax = difficulty == 0
            ? (effectiveSector <= 1 ? 0 : 1)
            : (effectiveSector <= 2 ? 1 : 2);
        const int bonus = static_cast<int>(enemyNextRandom(rng) %
            static_cast<unsigned>(bonusMax + 1));
        rolledMax[i] = std::min(maximum, minimum + progress + bonus);

        bool installed = system.availableByDefault;
        if (system.optional)
            installed = static_cast<int>(enemyNextRandom(rng) % 100u) < optionalChance;

        if (!installed) {
            system.startingPower = 0;
            system.level = 0;
            system.availableByDefault = false;
            continue;
        }

        int initial = system.startingPower;
        if (system.optional && initial <= 0)
            initial = std::max(1, system.minPower);
        initial = std::clamp(initial, 0, rolledMax[i]);
        system.startingPower = initial;
        system.level = initial;
        system.availableByDefault = true;

        if (system.optional) {
            if (enemyOffensive(system.system)) --budget.offensive;
            else if (difficulty == 2) --budget.general;
            else budget.general -= 2;
        }
    }

    std::vector<int> installed;
    for (std::size_t i = 0; i < ship.systems.size(); ++i)
        if (ship.systems[i].availableByDefault) installed.push_back(static_cast<int>(i));

    auto spendClass = [&](int amount, int classId) {
        int remaining = amount;
        while (remaining > 0) {
            std::vector<int> candidates;
            for (int index : installed) {
                const auto& system = ship.systems[index];
                const bool matches = classId == 0 ? enemyOffensive(system.system)
                    : classId == 1 ? enemyDefensive(system.system) : true;
                if (matches && system.level < rolledMax[index])
                    candidates.push_back(index);
            }
            if (candidates.empty()) break;
            const int index = candidates[enemyNextRandom(rng) % candidates.size()];
            ++ship.systems[index].level;
            ++ship.systems[index].startingPower;
            --remaining;
        }
        return remaining;
    };

    const int offensiveLeft = spendClass(std::max(0, budget.offensive), 0);
    const int defensiveLeft = spendClass(std::max(0, budget.defensive), 1);
    budget.general += offensiveLeft + defensiveLeft;
    spendClass(std::max(0, budget.general), 2);

    int reactor = 0;
    for (const auto& system : ship.systems)
        if (system.availableByDefault)
            reactor += std::max(0, system.startingPower);
    ship.startingReactorPower = reactor;
}

} // namespace

bool ShipContent::open(const std::string& archivePath) {
    loaded_ = false;
    database_.clear();
    return assets_.openArchive(archivePath);
}

bool ShipContent::openArchives(const std::vector<std::string>& archivePaths) {
    loaded_ = false;
    database_.clear();
    return assets_.openArchives(archivePaths);
}

bool ShipContent::loadShip(const std::string& shipId, LoadedShip& out,
                             const std::string& blueprintPath,
                             unsigned randomSeed) {
    database_.clear();
    out = {};

    // autoBlueprints.xml is part of the canonical base ftl.dat and contains
    // the generated/enemy ship definitions used by normal gameplay.
    std::vector<std::string> sources{blueprintPath, "data/autoBlueprints.xml"};
    if (advancedEdition_) {
        sources.push_back("data/dlcBlueprints.xml");
        sources.push_back("data/dlcBlueprintsOverwrite.xml");
        sources.push_back("data/dlcPirateBlueprints.xml");
    }
    if (database_.loadShipBlueprints(sources) == 0) return false;
    const ShipBlueprint* blueprint = database_.findShip(shipId);
    if (!blueprint || blueprint->layout.empty()) return false;

    const auto* layoutBytes = assets_.getBytes("data/" + blueprint->layout + ".txt");
    if (!layoutBytes) return false;

    LayoutBlueprint layout;
    const std::string text(layoutBytes->begin(), layoutBytes->end());
    if (!parseLayoutBlueprint(text, layout)) return false;

    out.blueprint = *blueprint;
    out.layout = std::move(layout);

    std::uint32_t rng = static_cast<std::uint32_t>(randomSeed);
    if (rng == 0) rng = static_cast<std::uint32_t>(std::hash<std::string>{}(shipId));
    auto nextRandom = [&]() {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return rng;
    };
    auto systemPower = [&](const char* name) {
        for (const auto& system : blueprint->systems)
            if (system.system == name && system.availableByDefault)
                return std::max(0, system.startingPower);
        return 0;
    };

    if (!out.blueprint.weaponLoadList.empty() && out.blueprint.initialWeapons.empty()) {
        if (const auto* list = database_.findBlueprintList(out.blueprint.weaponLoadList)) {
            int remaining = systemPower("weapons");
            const int slots = out.blueprint.weaponListCount >= 0
                ? out.blueprint.weaponListCount
                : (out.blueprint.weaponSlots > 0 ? out.blueprint.weaponSlots : 4);
            // Each generated slot samples the canonical pool independently.
            // Duplicate blueprint selections are valid; duplicate entries in
            // autoBlueprints.xml remain intentional additional weight.
            for (int slot = 0; slot < slots && remaining > 0; ++slot) {
                std::vector<std::string> candidates;
                for (const auto& id : *list) {
                    if (const auto* weapon = database_.findWeapon(id);
                        weapon && weapon->power > 0 && weapon->power <= remaining)
                        candidates.push_back(id);
                }
                if (candidates.empty()) break;
                const auto& selected = candidates[nextRandom() % candidates.size()];
                out.blueprint.initialWeapons.push_back(selected);
                remaining -= database_.findWeapon(selected)->power;
            }
        }
    }
    for (const auto& weaponId : out.blueprint.initialWeapons) {
        if (const auto* weapon = database_.findWeapon(weaponId))
            out.initialWeaponBlueprints.push_back(*weapon);
    }

    if (!out.blueprint.droneLoadList.empty() && out.blueprint.initialDrones.empty()) {
        if (const auto* list = database_.findBlueprintList(out.blueprint.droneLoadList)) {
            int remaining = systemPower("drones");
            const int slots = out.blueprint.droneListCount >= 0
                ? out.blueprint.droneListCount
                : (out.blueprint.droneSlots > 0 ? out.blueprint.droneSlots : 2);
            // Drone slots also sample independently; repeated blueprint
            // selections are not filtered here.
            for (int slot = 0; slot < slots && remaining > 0; ++slot) {
                std::vector<std::string> candidates;
                for (const auto& id : *list) {
                    if (const auto* drone = database_.findDrone(id);
                        drone && drone->power > 0 && drone->power <= remaining)
                        candidates.push_back(id);
                }
                if (candidates.empty()) break;
                const auto& selected = candidates[nextRandom() % candidates.size()];
                out.blueprint.initialDrones.push_back(selected);
                remaining -= database_.findDrone(selected)->power;
            }
        }
    }
    for (const auto& droneId : out.blueprint.initialDrones) {
        const auto* drone = database_.findDrone(droneId);
        if (drone) {
            DroneBlueprint resolved = *drone;
            if (!resolved.weaponBlueprint.empty()) {
                if (const auto* weapon = database_.findWeapon(resolved.weaponBlueprint)) {
                    resolved.weaponCooldown = weapon->cooldown;
                    resolved.weaponShots = weapon->shots;
                    resolved.weaponDamage = weapon->damage;
                    resolved.weaponSystemDamage = weapon->systemDamage;
                    resolved.weaponIonDamage = weapon->ionDamage;
                    resolved.weaponShieldPiercing = weapon->shieldPiercing;
                    resolved.weaponPersonnelDamage = weapon->personnelDamage;
                }
            }
            out.initialDroneBlueprints.push_back(std::move(resolved));
        }
    }
    return true;
}

bool ShipContent::loadEnemyShip(const std::string& shipId, LoadedShip& out, int sector,
                              int difficulty, unsigned randomSeed,
                              const std::string& blueprintPath) {
    if (!loadShip(shipId, out, blueprintPath, randomSeed))
        return false;

    std::uint32_t rng = static_cast<std::uint32_t>(randomSeed);
    if (rng == 0)
        rng = static_cast<std::uint32_t>(std::hash<std::string>{}(shipId));
    generateEnemySystems(out.blueprint, sector, difficulty, rng);

    // The system generator runs before enemy loadout generation in FTL.
    // Rebuild generated weapons/drones from the now-final system power.
    out.blueprint.initialWeapons.clear();
    out.blueprint.initialDrones.clear();
    out.initialWeaponBlueprints.clear();
    out.initialDroneBlueprints.clear();

    auto systemPower = [&](const char* name) {
        for (const auto& system : out.blueprint.systems)
            if (system.system == name && system.availableByDefault)
                return std::max(0, system.startingPower);
        return 0;
    };

    if (!out.blueprint.weaponLoadList.empty()) {
        if (const auto* list = database_.findBlueprintList(out.blueprint.weaponLoadList)) {
            int remaining = systemPower("weapons");
            const int slots = out.blueprint.weaponListCount >= 0
                ? out.blueprint.weaponListCount
                : (out.blueprint.weaponSlots > 0 ? out.blueprint.weaponSlots : 4);
                bool needsShieldBreaker = true;
            bool needsHullDamage = true;
            for (int slot = 0; slot < slots && remaining > 0; ++slot) {
                std::vector<std::string> candidates;
                for (const auto& id : *list) {
                    const auto* weapon = database_.findWeapon(id);
                    if (!weapon || weapon->power <= 0 || weapon->power > remaining) continue;
                    if (weapon->power != 1 && weapon->power >= systemPower("weapons")) continue;
                    if (weapon->power * 4 <= remaining) continue;
                    if (out.blueprint.initialWeapons.empty() && systemPower("weapons") >= 3 &&
                        weapon->power < 2) continue;
                    const bool shieldBreaker = weapon->type == "LASER" ||
                        (weapon->type == "MISSILE" && weapon->shieldPiercing <= 3);
                    const bool hullDamage = weapon->damage > 0;
                    // While either required capability is still missing, the
                    // generated weapon must satisfy at least one remaining flag.
                    // A single weapon can satisfy both and therefore clear both.
                    if ((needsShieldBreaker || needsHullDamage) &&
                        !shieldBreaker && !hullDamage) continue;
                    candidates.push_back(id);
                }
                if (candidates.empty()) break;
                const auto& selected = candidates[enemyNextRandom(rng) % candidates.size()];
                out.blueprint.initialWeapons.push_back(selected);
                const auto* weapon = database_.findWeapon(selected);
                remaining -= weapon->power;
                if (weapon->type == "LASER" ||
                    (weapon->type == "MISSILE" && weapon->shieldPiercing <= 3))
                    needsShieldBreaker = false;
                if (weapon->damage > 0)
                    needsHullDamage = false;
            }
        }
    }
    for (const auto& weaponId : out.blueprint.initialWeapons)
        if (const auto* weapon = database_.findWeapon(weaponId))
            out.initialWeaponBlueprints.push_back(*weapon);

    if (!out.blueprint.droneLoadList.empty()) {
        if (const auto* list = database_.findBlueprintList(out.blueprint.droneLoadList)) {
            int remaining = systemPower("drones");
            const int totalPower = remaining;
            const int slots = out.blueprint.droneListCount >= 0
                ? out.blueprint.droneListCount
                : (out.blueprint.droneSlots > 0 ? out.blueprint.droneSlots : 2);
            std::set<std::string> usedDrones;
            bool needsShieldBreaker = true;
            bool needsHullDamage = true;
            for (const auto& weaponId : out.blueprint.initialWeapons) {
                const auto* weapon = database_.findWeapon(weaponId);
                if (!weapon) continue;
                if (weapon->type == "LASER" ||
                    (weapon->type == "MISSILE" && weapon->shieldPiercing <= 3))
                    needsShieldBreaker = false;
                if (weapon->damage > 0)
                    needsHullDamage = false;
            }
            for (int slot = 0; slot < slots && remaining > 0; ++slot) {
                std::vector<std::string> candidates;
                for (const auto& id : *list) {
                    if (usedDrones.count(id)) continue;
                    const auto* drone = database_.findDrone(id);
                    if (!drone || drone->power <= 0 || drone->power > remaining) continue;
                    if (totalPower >= 4 && drone->power >= totalPower) continue;
                    if ((needsShieldBreaker || needsHullDamage) &&
                        drone->type != DroneBlueprint::Type::Combat) continue;
                    candidates.push_back(id);
                }
                if (candidates.empty()) {
                    for (const auto& id : *list) {
                        if (usedDrones.count(id)) continue;
                        const auto* drone = database_.findDrone(id);
                        if (!drone || drone->power <= 0 || drone->power > remaining) continue;
                        if (totalPower >= 4 && drone->power >= totalPower) continue;
                        candidates.push_back(id);
                    }
                }
                if (candidates.empty()) break;
                const auto& selected = candidates[enemyNextRandom(rng) % candidates.size()];
                usedDrones.insert(selected);
                out.blueprint.initialDrones.push_back(selected);
                const auto* drone = database_.findDrone(selected);
                remaining -= drone->power;
                if (drone->type == DroneBlueprint::Type::Combat) {
                    needsShieldBreaker = false;
                    needsHullDamage = false;
                }
            }
        }
    }
    for (const auto& droneId : out.blueprint.initialDrones) {
        const auto* drone = database_.findDrone(droneId);
        if (!drone) continue;
        DroneBlueprint resolved = *drone;
        if (!resolved.weaponBlueprint.empty()) {
            if (const auto* weapon = database_.findWeapon(resolved.weaponBlueprint)) {
                resolved.weaponCooldown = weapon->cooldown;
                resolved.weaponShots = weapon->shots;
                resolved.weaponDamage = weapon->damage;
                resolved.weaponSystemDamage = weapon->systemDamage;
                resolved.weaponIonDamage = weapon->ionDamage;
                resolved.weaponShieldPiercing = weapon->shieldPiercing;
                resolved.weaponPersonnelDamage = weapon->personnelDamage;
            }
        }
        out.initialDroneBlueprints.push_back(std::move(resolved));
    }
    return true;
}

bool ShipContent::loadPlayerShip(const std::string& blueprintPath,
                                  const std::string& shipId) {
    loaded_ = false;
    LoadedShip loaded;
    if (!loadShip(shipId, loaded, blueprintPath)) return false;
    ship_ = std::move(loaded);
    loaded_ = true;
    return true;
}

}