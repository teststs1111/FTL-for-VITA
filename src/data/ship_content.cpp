#include "data/ship_content.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <set>
#include <string>

namespace wormhole {

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
            // The load list is sampled with replacement: the same weapon
            // blueprint may be selected more than once. The canonical data
            // therefore acts as the candidate pool, including intentional
            // duplicate entries as additional weight.
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
            // Drone load lists are also sampled with replacement; duplicate
            // drone blueprints are therefore allowed when the power budget and
            // slot count permit them.
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

namespace {

std::uint32_t enemyRandom(std::uint32_t& rng) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

int enemyProgressionSector(int sector, int difficulty) {
    return std::clamp(sector - (difficulty == 0 ? 1 : 0), 0, 8);
}

struct EnemyBudget {
    int offensive;
    int defensive;
    int general;
};

EnemyBudget enemyBudgetFor(int progression, int difficulty) {
    static constexpr int table[9][3][3] = {
        {{1,1,1},{1,1,1},{1,1,1}},
        {{1,1,1},{1,2,2},{1,1,2}},
        {{1,2,2},{2,3,3},{1,1,2}},
        {{2,3,3},{3,4,4},{1,1,2}},
        {{3,4,4},{4,5,5},{1,2,3}},
        {{4,5,5},{5,6,6},{2,2,3}},
        {{5,6,6},{6,7,7},{2,2,3}},
        {{6,7,7},{7,8,8},{2,3,4}},
        {{7,8,8},{8,9,9},{3,3,4}}
    };
    const int p = std::clamp(progression, 0, 8);
    const int d = std::clamp(difficulty, 0, 2);
    return {table[p][d][0], table[p][d][1], table[p][d][2]};
}

bool enemyOffensive(const std::string& system) {
    return system == "weapons" || system == "drones" || system == "teleporter";
}

bool enemyDefensive(const std::string& system) {
    return system == "shields" || system == "engines";
}

void generateEnemySystems(ShipBlueprint& ship, int sector, int difficulty, std::uint32_t& rng) {
    const int progression = enemyProgressionSector(sector, difficulty);
    EnemyBudget budget = enemyBudgetFor(progression, difficulty);

    for (auto& system : ship.systems) {
        const int minimum = std::max(0, system.minPower);
        const int maximum = std::max(minimum, system.maxPower);
        const int span = maximum - minimum;
        int rolledMaximum = minimum + (span * progression) / 8;

        int bonusMax = 0;
        if (difficulty == 0) bonusMax = progression <= 0 ? 0 : 1;
        else bonusMax = progression <= 1 ? 1 : 2;
        rolledMaximum = std::min(maximum,
            rolledMaximum + static_cast<int>(enemyRandom(rng) % static_cast<std::uint32_t>(bonusMax + 1)));

        if (system.optional) {
            const int chance = std::clamp(10 + progression * 10 + (difficulty == 2 ? 10 : 0), 10, 100);
            if (static_cast<int>(enemyRandom(rng) % 100) >= chance) {
                system.startingPower = 0;
                system.level = 0;
                system.availableByDefault = false;
                continue;
            }
        }

        int initial = std::max(0, system.startingPower);
        if (system.optional && initial <= 0) initial = std::max(1, minimum);
        initial = std::clamp(initial, minimum, rolledMaximum);
        system.startingPower = initial;
        system.level = initial;
        system.availableByDefault = initial > 0;

        if (system.optional) {
            if (enemyOffensive(system.system)) {
                --budget.offensive;
            } else if (difficulty == 2) {
                --budget.general;
            } else {
                // Optional non-offensive systems consume two general-budget
                // points on Easy/Normal and one on Hard.
                budget.general -= 2;
            }
        }
    }

    auto spend = [&](int& pool, bool (*classify)(const std::string&)) {
        while (pool > 0) {
            std::vector<int> candidates;
            for (int i = 0; i < static_cast<int>(ship.systems.size()); ++i) {
                const auto& s = ship.systems[i];
                if (s.availableByDefault && classify(s.system) && s.level < s.maxPower)
                    candidates.push_back(i);
            }
            if (candidates.empty()) break;
            const int index = candidates[enemyRandom(rng) % candidates.size()];
            auto& s = ship.systems[index];
            ++s.level;
            ++s.startingPower;
            --pool;
        }
    };

    spend(budget.offensive, enemyOffensive);
    spend(budget.defensive, enemyDefensive);

    // Any offensive/defensive budget left after its category has no eligible
    // upgrade becomes general budget. Negative optional-system costs are also
    // carried through here, matching the documented budget flow.
    int general = budget.general + budget.offensive + budget.defensive;
    spend(general, [](const std::string&) { return true; });

    int reactor = 0;
    for (auto& system : ship.systems) {
        if (!system.availableByDefault) continue;
        system.level = std::max(0, system.startingPower);
        reactor += system.startingPower;
    }
    ship.startingReactorPower = reactor;
}

int generateEnemyCrewCount(const ShipBlueprint& ship, int sector, int difficulty) {
    if (ship.minCrew <= 0 || ship.maxCrew <= 0) return 0;
    const int progression = enemyProgressionSector(sector, difficulty);
    return ship.minCrew +
           ((ship.maxCrew - ship.minCrew) * progression) / 8;
}

}

bool ShipContent::loadEnemyShip(const std::string& shipId, LoadedShip& out, int sector,
                                 int difficulty, unsigned randomSeed,
                                 const std::string& blueprintPath,
                                 const std::string& sectorType,
                                 const std::vector<CrewOverrideEntry>* crewOverride) {
    if (!loadShip(shipId, out, blueprintPath, randomSeed))
        return false;

    std::uint32_t rng = static_cast<std::uint32_t>(randomSeed);
    if (rng == 0) rng = static_cast<std::uint32_t>(std::hash<std::string>{}(shipId));

    generateEnemySystems(out.blueprint, sector, difficulty, rng);

    if (out.blueprint.maxCrew > 0) {
        const int count = generateEnemyCrewCount(out.blueprint, sector, difficulty);

        // Vanilla first applies an event-level crew override. Without one,
        // the first blueprint race owns the generated crew count. Pirate
        // blueprints use class="random", which samples the sector's crew
        // rarity table instead of treating "random" as a literal race.
        std::vector<CrewOverrideEntry> generated;
        if (crewOverride && !crewOverride->empty()) {
            generated = *crewOverride;
        } else {
            const std::string race = out.blueprint.crew.empty() ? "human" : out.blueprint.crew.front().race;
            generated.push_back({race, 1.0});
        }

        auto randomRace = [&](std::uint32_t& state) -> std::string {
            std::string type = sectorType;
            std::transform(type.begin(), type.end(), type.begin(),
                [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

            struct RaceWeight { const char* race; int rarity; };
            static constexpr RaceWeight civilian[] = {
                {"human",1},{"engi",2},{"mantis",2},{"rock",3},{"zoltan",5}
            };
            static constexpr RaceWeight engi[] = {
                {"engi",1},{"human",3},{"zoltan",4}
            };
            static constexpr RaceWeight zoltan[] = {
                {"zoltan",1},{"human",2},{"engi",3},{"mantis",3},{"rock",3},{"slug",3}
            };
            static constexpr RaceWeight mantis[] = {
                {"mantis",1},{"human",2},{"engi",3},{"rock",4}
            };
            static constexpr RaceWeight rock[] = {
                {"rock",1},{"human",2},{"zoltan",3}
            };
            static constexpr RaceWeight abandoned[] = {
                {"lanius",2},{"human",2},{"engi",3},{"mantis",3},{"rock",3},{"zoltan",4},{"slug",4}
            };
            static constexpr RaceWeight slugNebula[] = {
                {"slug",2},{"human",2},{"zoltan",4},{"rock",4},{"engi",4},{"mantis",4}
            };
            static constexpr RaceWeight uncharted[] = {
                {"human",1},{"slug",3},{"zoltan",4},{"rock",4},{"engi",4},{"mantis",4}
            };
            static constexpr RaceWeight crystal[] = {{"crystal",1}};
            static constexpr RaceWeight lastStand[] = {
                {"human",1},{"engi",2},{"mantis",2},{"rock",3},{"zoltan",5}
            };

            const RaceWeight* table = civilian;
            std::size_t size = sizeof(civilian) / sizeof(civilian[0]);
            if (type.find("ENGI") != std::string::npos) { table = engi; size = sizeof(engi) / sizeof(engi[0]); }
            else if (type.find("ZOLTAN") != std::string::npos) { table = zoltan; size = sizeof(zoltan) / sizeof(zoltan[0]); }
            else if (type.find("MANTIS") != std::string::npos) { table = mantis; size = sizeof(mantis) / sizeof(mantis[0]); }
            else if (type.find("ROCK") != std::string::npos) { table = rock; size = sizeof(rock) / sizeof(rock[0]); }
            else if (type.find("ABANDONED") != std::string::npos || type.find("LANIUS") != std::string::npos) { table = abandoned; size = sizeof(abandoned) / sizeof(abandoned[0]); }
            else if (type.find("SLUG") != std::string::npos) { table = slugNebula; size = sizeof(slugNebula) / sizeof(slugNebula[0]); }
            else if (type.find("NEBULA") != std::string::npos || type.find("DEEP_SPACE") != std::string::npos) { table = uncharted; size = sizeof(uncharted) / sizeof(uncharted[0]); }
            else if (type.find("CRYSTAL") != std::string::npos) { table = crystal; size = sizeof(crystal) / sizeof(crystal[0]); }
            else if (type.find("FINAL") != std::string::npos) { table = lastStand; size = sizeof(lastStand) / sizeof(lastStand[0]); }

            int totalWeight = 0;
            for (std::size_t i = 0; i < size; ++i)
                totalWeight += std::max(1, 6 - table[i].rarity);
            if (totalWeight <= 0) return std::string("human");
            int roll = static_cast<int>(enemyRandom(state) % static_cast<std::uint32_t>(totalWeight));
            for (std::size_t i = 0; i < size; ++i) {
                roll -= std::max(1, 6 - table[i].rarity);
                if (roll < 0) return std::string(table[i].race);
            }
            return table[size - 1].race;
        };

        out.blueprint.crew.clear();
        int generatedIndex = 0;
        for (const auto& overrideEntry : generated) {
            if (overrideEntry.race.empty()) continue;
            int amount = 0;
            if (overrideEntry.proportion > 0.0) {
                // Proportional overrides must never create crew beyond the
                // generated count. In particular, a zero-count ship must
                // remain empty rather than gaining one crew member.
                amount = std::max(0, static_cast<int>(overrideEntry.proportion * count));
            } else {
                amount = std::max(0, static_cast<int>(-overrideEntry.proportion));
            }
            for (int i = 0; i < amount; ++i) {
                std::string race = overrideEntry.race;
                if (race == "random")
                    race = randomRace(rng);
                CrewBlueprint member;
                member.race = race;
                member.name = race + "_" + std::to_string(++generatedIndex);
                member.room = -1;
                out.blueprint.crew.push_back(std::move(member));
            }
        }
    }

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
            const int total = remaining;
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
                    if (weapon->power != 1 && weapon->power >= total) continue;
                    if (weapon->power * 4 <= remaining) continue;
                    if (slot == 0 && total >= 3 && weapon->power < 2) continue;

                    const bool shieldBreaker =
                        weapon->type == "LASER" || weapon->type == "ION" ||
                        weapon->type == "CRYSTAL";
                    const bool hullDamage = weapon->damage > 0 || weapon->hullBust > 0;
                    if ((needsShieldBreaker || needsHullDamage) &&
                        !((needsShieldBreaker && shieldBreaker) ||
                          (needsHullDamage && hullDamage)))
                        continue;
                    candidates.push_back(id);
                }

                if (candidates.empty()) {
                    // The flags are soft constraints: retry with the normal
                    // power restrictions when no flagged weapon is available.
                    for (const auto& id : *list) {
                        const auto* weapon = database_.findWeapon(id);
                        if (!weapon || weapon->power <= 0 || weapon->power > remaining) continue;
                        if (weapon->power != 1 && weapon->power >= total) continue;
                        if (weapon->power * 4 <= remaining) continue;
                        if (slot == 0 && total >= 3 && weapon->power < 2) continue;
                        candidates.push_back(id);
                    }
                }
                if (candidates.empty()) break;

                const auto& selected = candidates[enemyRandom(rng) % candidates.size()];
                out.blueprint.initialWeapons.push_back(selected);
                const auto* weapon = database_.findWeapon(selected);
                remaining -= weapon->power;
                needsShieldBreaker = needsShieldBreaker &&
                    !(weapon->type == "LASER" || weapon->type == "ION" || weapon->type == "CRYSTAL");
                needsHullDamage = needsHullDamage &&
                    !(weapon->damage > 0 || weapon->hullBust > 0);
            }
        }
    }

    for (const auto& id : out.blueprint.initialWeapons)
        if (const auto* weapon = database_.findWeapon(id))
            out.initialWeaponBlueprints.push_back(*weapon);

    if (!out.blueprint.droneLoadList.empty()) {
        if (const auto* list = database_.findBlueprintList(out.blueprint.droneLoadList)) {
            int remaining = systemPower("drones");
            const int total = remaining;
            const int slots = out.blueprint.droneListCount >= 0
                ? out.blueprint.droneListCount
                : (out.blueprint.droneSlots > 0 ? out.blueprint.droneSlots : 2);
            std::set<std::string> used;
            for (int slot = 0; slot < slots && remaining > 0; ++slot) {
                std::vector<std::string> candidates;
                for (const auto& id : *list) {
                    if (used.count(id)) continue;
                    const auto* drone = database_.findDrone(id);
                    if (!drone || drone->power <= 0 || drone->power > remaining) continue;
                    if (total >= 4 && drone->power >= total) continue;
                    candidates.push_back(id);
                }
                if (candidates.empty()) break;
                const auto& selected = candidates[enemyRandom(rng) % candidates.size()];
                out.blueprint.initialDrones.push_back(selected);
                used.insert(selected);
                remaining -= database_.findDrone(selected)->power;
            }
        }
    }

    for (const auto& id : out.blueprint.initialDrones) {
        const auto* drone = database_.findDrone(id);
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