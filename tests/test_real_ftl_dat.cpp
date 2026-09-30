#include "data/asset_store.hpp"
#include "data/bxml.hpp"
#include "data/event_database.hpp"
#include "data/ftl_dat.hpp"
#include "data/sector_database.hpp"
#include "data/ship_content.hpp"
#include "game/ship_runtime.hpp"
#include <cassert>
#include <cstdlib>
#include <string>
#include <vector>
#include <set>
#include <map>

int main() {
    const char* env = std::getenv("FTL_DAT_PATH");
    if (!env || !*env) return 0; // Proprietary user data is never required in CI.

    wormhole::FtlDat archive;
    assert(archive.open(env));
    const auto names = archive.fileNames();
    assert(names.size() == 3219);
    assert(archive.contains("data/blueprints.xml"));
    assert(archive.contains("data/dlcBlueprints.xml"));
    assert(archive.contains("data/dlcBlueprintsOverwrite.xml"));
    assert(archive.contains("data/dlcEvents.xml"));
    assert(archive.contains("data/dlcEvents_anaerobic.xml"));
    assert(archive.contains("data/dlcEventsOverwrite.xml"));
    assert(archive.contains("data/sector_data.xml"));
    assert(archive.contains("data/text-ja.xml"));
    assert(archive.contains("img/ship/kestral_base.png"));

    wormhole::AssetStore assets;
    assert(assets.openArchive(env));

    wormhole::EventDatabase events(assets);
    events.setAdvancedEdition(false);
    assert(events.load());
    assert(events.resolve("STORE_REBELSIDE_SEARCH", 0) == nullptr);

    events.setAdvancedEdition(true);
    assert(events.load());
    assert(events.resolve("STORE_REBELSIDE_SEARCH", 0) != nullptr);
    // Lanius event pools live in data/dlcEvents_anaerobic.xml in the real
    // archive and must be available when AE is enabled.
    assert(events.resolve("HOSTILE_LANIUS", 0) != nullptr);
    assert(events.resolve("NEUTRAL_LANIUS", 0) != nullptr);
    assert(events.find("LANIUS_FIGHT") != nullptr);
    const char* nebulaPoolIds[] = {
        "NEBULA_EMPTY", "NEBULA_REBEL", "NEBULA_AUTO",
        "NEBULA_AUTO_WARNING", "NEBULA_PIRATE_SMUGGLE",
        "NEBULA_AUTO_DEFENSE_ITEM", "NEBULA_TRADER", "STORM_REBEL",
        "STORM_AUTO", "STORM_ITEMS", "NEBULA_LOST_SHIP",
        "NEBULA_BOARDING", "STORM_BOARDING", "NEBULA_MANTIS_FIGHT",
        "NEBULA_WEAPONS_TRADER"
    };
    for (unsigned seed = 0; seed < 64; ++seed) {
        const auto* event = events.resolve("NEBULA", seed);
        assert(event != nullptr);
        bool known = false;
        for (const char* id : nebulaPoolIds)
            if (event->id == id) { known = true; break; }
        assert(known);
    }

    const auto* crewRemoval = events.find("CREW_DEAD_TEST");
    assert(crewRemoval != nullptr);
    assert(crewRemoval->crewRemovals.size() == 1);
    assert(!crewRemoval->crewRemovals.front().clone);

    const auto* fleetDelay = events.find("FUEL_FLEET_DELAY");
    assert(fleetDelay != nullptr);
    assert(fleetDelay->special.modifyPursuit == -1);
    assert(fleetDelay->choices.size() == 1);
    assert(fleetDelay->choices.front().hidden);
    assert(fleetDelay->choices.front().load == "NO_FUEL");
    const char* noFuelPools[] = {"FUEL_FLEET_DELAY", "FUEL_NOTHING", "FUEL_TRADER",
        "FUEL_EXPLORE", "FUEL_APPROACH", "FUEL_OFF_ENGI_DUBIOUS",
        "FUEL_OFF_ROCK_WRECK", "NO_FUEL_REFUGEE_FRIENDLY"};
    for (unsigned seed = 0; seed < 128; ++seed) {
        const auto* resolved = events.resolve("NO_FUEL", seed);
        assert(resolved != nullptr);
        bool known = false;
        for (const char* id : noFuelPools)
            if (resolved->id == id) { known = true; break; }
        assert(known);
    }
    // The canonical NO_FUEL list intentionally repeats FUEL_NOTHING four times,
    // giving it 4/11 of the base-game weight while every other entry occupies
    // one slot. Keep duplicate weighting intact rather than flattening IDs.
    std::map<std::string, int> noFuelCounts;
    for (unsigned seed = 0; seed < 1100; ++seed) {
        const auto* resolved = events.resolve("NO_FUEL", seed);
        assert(resolved != nullptr);
        ++noFuelCounts[resolved->id];
    }
    assert(noFuelCounts["FUEL_NOTHING"] == 400);
    for (const char* id : noFuelPools) {
        if (std::string(id) == "FUEL_NOTHING") continue;
        assert(noFuelCounts[id] == 100);
    }
    const auto* fleetAdvance = events.find("FUEL_FLEET_DISTRESS");
    assert(fleetAdvance != nullptr);
    assert(fleetAdvance->special.modifyPursuit == 1);

    const auto* warning = events.find("FUEL_ON_REBEL_WARNING");
    assert(warning != nullptr);
    assert(warning->hostile);
    assert(warning->hostileShipId == "REBEL_AUTO_WARNING");

    const auto* mercenary = events.find("MERCENARY");
    assert(mercenary != nullptr);
    bool hasTwoTurnDelay = false;
    for (const auto& choice : mercenary->choices) {
        if (choice.special.modifyPursuit == -2) {
            hasTwoTurnDelay = true;
            break;
        }
    }
    assert(hasTwoTurnDelay);

    wormhole::SectorDatabase sectors(assets);
    sectors.setAdvancedEdition(true);
    assert(sectors.load());
    assert(sectors.select(0, 0) != nullptr);
    assert(sectors.find("REBEL_SECTOR") != nullptr);
    const auto* final = sectors.select(7, 0);
    assert(final != nullptr && final->name == "FINAL");
    auto finalPool = [&](const char* name, int min, int max) {
        for (const auto& pool : final->events) {
            if (pool.name == name) {
                assert(pool.min == min && pool.max == max);
                return true;
            }
        }
        return false;
    };
    assert(finalPool("STORE", 1, 1));
    assert(finalPool("BOSS_REPAIR_STATION", 3, 3));
    assert(finalPool("BOSS_HOSTILE", 6, 6));
    assert(finalPool("BOSS_NEUTRAL", 7, 10));

    const auto assertPool = [](const wormhole::SectorDefinition* sector,
                               const char* name, int min, int max) {
        assert(sector != nullptr);
        for (const auto& pool : sector->events) {
            if (pool.name == name) {
                assert(pool.min == min && pool.max == max);
                return;
            }
        }
        assert(false);
    };
    const auto* standard = sectors.find("STANDARD_SPACE");
    assertPool(standard, "STORE", 1, 2);
    assertPool(standard, "NEBULA", 0, 4);
    assertPool(standard, "HOSTILE1", 2, 2);
    assertPool(standard, "QUESTS", 1, 1);

    const auto* civilian = sectors.find("CIVILIAN_SECTOR");
    assertPool(civilian, "NEBULA", 0, 8);
    assertPool(civilian, "HOSTILE_CIVILIAN", 4, 6);
    assertPool(civilian, "QUESTS", 0, 2);

    const auto* nebula = sectors.find("NEBULA_SECTOR");
    assertPool(nebula, "NEBULA_STORE", 1, 1);
    assertPool(nebula, "NEBULA_EMPTY", 4, 4);
    assertPool(nebula, "NEBULA_HOSTILE", 5, 6);
    assertPool(nebula, "NEBULA_NEUTRAL", 7, 8);

    const auto* slug = sectors.find("SLUG_SECTOR");
    assertPool(slug, "NEBULA_STORE_SLUG", 2, 2);
    assertPool(slug, "NEBULA_NOTHING_SLUG", 2, 4);
    assertPool(slug, "NEBULA_HOSTILE_SLUG", 5, 7);
    assertPool(slug, "NEBULA_NEUTRAL_SLUG", 3, 5);

    wormhole::ShipContent content;
    assert(content.open(env));
    // FTL's fixed weapon/drone lists honor the explicit count attribute;
    // entries beyond count are not loaded.
    wormhole::bxml::Node countedShip;
    countedShip.name = "shipBlueprint";
    countedShip.attributes["name"] = "COUNTED_TEST";
    countedShip.attributes["layout"] = "dummy";
    wormhole::bxml::Node countedWeapons;
    countedWeapons.name = "weaponList";
    countedWeapons.attributes["count"] = "1";
    wormhole::bxml::Node weaponA; weaponA.name = "weapon"; weaponA.attributes["name"] = "LASER_BURST_1";
    wormhole::bxml::Node weaponB; weaponB.name = "weapon"; weaponB.attributes["name"] = "LASER_BURST_2";
    countedWeapons.children = {weaponA, weaponB};
    countedShip.children.push_back(countedWeapons);
    wormhole::ShipBlueprint countedBlueprint;
    assert(wormhole::parseShipBlueprint(countedShip, countedBlueprint));
    assert(countedBlueprint.weaponListCount == 1);
    assert(countedBlueprint.initialWeapons.size() == 1);
    assert(countedBlueprint.initialWeapons.front() == "LASER_BURST_1");
    
    // Enemy system generation data must preserve min/max bounds and optional
    // installation state from the canonical ship blueprint schema.
    wormhole::bxml::Node systemList;
    systemList.name = "systemList";
    wormhole::bxml::Node optionalShields;
    optionalShields.name = "shields";
    optionalShields.attributes["room"] = "2";
    optionalShields.attributes["power"] = "2";
    optionalShields.attributes["min"] = "2";
    optionalShields.attributes["max"] = "8";
    optionalShields.attributes["start"] = "false";
    systemList.children.push_back(optionalShields);
    countedShip.children.push_back(systemList);
    wormhole::ShipBlueprint systemBlueprint;
    assert(wormhole::parseShipBlueprint(countedShip, systemBlueprint));
    assert(systemBlueprint.systems.size() == 1);
    assert(systemBlueprint.systems.front().minPower == 2);
    assert(systemBlueprint.systems.front().maxPower == 8);
    assert(systemBlueprint.systems.front().optional);
    assert(!systemBlueprint.systems.front().availableByDefault);
    wormhole::bxml::Node crewCountNode;
    crewCountNode.name = "crewCount";
    crewCountNode.attributes["amount"] = "2";
    crewCountNode.attributes["max"] = "5";
    crewCountNode.attributes["class"] = "rock";
    countedShip.children.push_back(crewCountNode);
    wormhole::ShipBlueprint crewBlueprint;
    assert(wormhole::parseShipBlueprint(countedShip, crewBlueprint));
    assert(crewBlueprint.minCrew == 2);
    assert(crewBlueprint.maxCrew == 5);
    assert(crewBlueprint.crew.size() == 1);
    assert(crewBlueprint.crew.front().race == "rock");


    content.setAdvancedEdition(false);
    assert(content.loadPlayerShip());
    assert(content.playerShip()->blueprint.id == "PLAYER_SHIP_HARD");
    // autoBlueprints.xml is canonical base-game data and must expose enemy auto ships.
    assert(content.loadPlayerShip("data/blueprints.xml", "AUTO_BASIC"));
    assert(content.playerShip()->blueprint.id == "AUTO_BASIC");
    const auto& autoLists = content.blueprints().blueprintLists();
    assert(autoLists.size() == 40);
    const std::vector<std::string>* zoltanList = content.blueprints().findBlueprintList("SHIPS_ZOLTAN");
    assert(zoltanList != nullptr && !zoltanList->empty());
    const auto* selectedZoltan = content.blueprints().selectBlueprint("SHIPS_ZOLTAN", 0);
    assert(selectedZoltan != nullptr);
    assert(content.blueprints().findShip(*selectedZoltan) != nullptr);
    assert(content.blueprints().findBlueprintList("WEAPONS_MISSILES") != nullptr);
    wormhole::LoadedShip autoBasic;
    assert(content.loadShip("AUTO_BASIC", autoBasic, "data/blueprints.xml", 12345u));
    wormhole::LoadedShip fleetFuel;
    assert(content.loadShip("REBEL_FLEET_FUEL", fleetFuel));
    assert(fleetFuel.blueprint.id == "REBEL_FLEET_FUEL");
    assert(fleetFuel.blueprint.maxHealth > 0);

    // Rebel Flagship combat drones must retain their canonical weapon
    // definitions from ftl.dat. In particular BOSS_2 includes COMBAT_BEAM.
    wormhole::LoadedShip flagshipPhase2;
    assert(content.loadShip("BOSS_2", flagshipPhase2));
    bool foundCombatBeam = false;
    bool foundCombatLaser = false;
    for (const auto& droneId : flagshipPhase2.blueprint.initialDrones) {
        const auto* drone = content.blueprints().findDrone(droneId);
        assert(drone != nullptr);
        if (droneId == "COMBAT_BEAM") {
            foundCombatBeam = true;
            assert(drone->weaponBlueprint == "DRONE_BEAM");
            assert(drone->weaponType == "BEAM");
            assert(drone->weaponDamage == 1);
            assert(drone->weaponShots == 1);
        }
        if (droneId == "COMBAT_1") {
            foundCombatLaser = true;
            assert(drone->weaponBlueprint == "DRONE_LASER");
            assert(drone->weaponType == "LASER");
            assert(drone->weaponDamage == 1);
            assert(drone->weaponShots == 1);
        }
    }
    assert(foundCombatBeam && foundCombatLaser);

    wormhole::LoadedShip generatedEnemy;
    assert(content.loadEnemyShip("AUTO_BASIC", generatedEnemy, 5, 1, 12345u));

    wormhole::LoadedShip pirateEnemy;
    assert(content.loadEnemyShip("REBEL_FAT_P", pirateEnemy, 5, 1, 24680u,
                                 "data/blueprints.xml", "PIRATE_SECTOR"));
    assert(!pirateEnemy.blueprint.crew.empty());
    for (const auto& crew : pirateEnemy.blueprint.crew)
        assert(crew.race != "random" && !crew.race.empty());

    const std::vector<wormhole::CrewOverrideEntry> override = {
        {"mantis", 0.80}, {"engi", 0.20}
    };
    wormhole::LoadedShip overriddenEnemy;
    assert(content.loadEnemyShip("MANTIS_FIGHTER", overriddenEnemy, 5, 1, 13579u,
                                 "data/blueprints.xml", "MANTIS_SECTOR", &override));
    int mantisCount = 0;
    int engiCount = 0;
    for (const auto& crew : overriddenEnemy.blueprint.crew) {
        if (crew.race == "mantis") ++mantisCount;
        if (crew.race == "engi") ++engiCount;
    }
    assert(mantisCount == 3 && engiCount == 1);

    // A zero generated crew count must not be inflated by a proportional
    // override entry.
    const std::vector<wormhole::CrewOverrideEntry> zeroOverride = {
        {"mantis", 0.20}
    };
    wormhole::LoadedShip zeroCrewEnemy;
    assert(content.loadEnemyShip("MANTIS_FIGHTER", zeroCrewEnemy, 1, 0, 97531u,
                                 "data/blueprints.xml", "MANTIS_SECTOR", &zeroOverride));
    assert(zeroCrewEnemy.blueprint.crew.empty());

    int generatedReactor = 0;
    std::set<std::string> generatedDrones;
    for (const auto& system : generatedEnemy.blueprint.systems) {
        assert(system.startingPower >= 0);
        assert(system.startingPower <= system.maxPower);
        if (system.availableByDefault) generatedReactor += system.startingPower;
    }
    for (const auto& drone : generatedEnemy.blueprint.initialDrones)
        assert(generatedDrones.insert(drone).second);
    assert(generatedEnemy.blueprint.startingReactorPower == generatedReactor);
    // AE overwrite lists replace the base list under its canonical name;
    // OVERRIDE_* is an input-file convention, not a runtime list ID.
    content.setAdvancedEdition(true);
    assert(content.loadShip("AUTO_BASIC", autoBasic, "data/blueprints.xml", 12345u));
    assert(content.blueprints().findBlueprintList("SHIPS_REBEL") != nullptr);
    assert(content.blueprints().findBlueprintList("OVERRIDE_SHIPS_REBEL") == nullptr);
    assert(autoBasic.blueprint.minSector == 1);
    assert(autoBasic.blueprint.maxSector == 8);
    assert(autoBasic.blueprint.weaponLoadList == "WEAPONS_AUTO");
    assert(!autoBasic.initialWeaponBlueprints.empty());
    int autoWeaponPower = 0;
    for (const auto& weapon : autoBasic.initialWeaponBlueprints) {
        assert(weapon.power > 0);
        autoWeaponPower += weapon.power;
    }
    assert(autoWeaponPower <= 2);

    content.setAdvancedEdition(true);
    assert(content.loadPlayerShip("data/blueprints.xml", "PLAYER_SHIP_ANAEROBIC"));
    assert(content.playerShip()->blueprint.id == "PLAYER_SHIP_ANAEROBIC");

    content.setAdvancedEdition(false);
    assert(content.loadPlayerShip());
    wormhole::ShipRuntime runtime;
    assert(runtime.load(content));
    const std::size_t beforeCrew = runtime.crew.size();
    wormhole::RuntimeCrew extra;
    extra.race = "human";
    extra.name = "event_test";
    assert(runtime.addCrew(extra) >= 0);
    assert(runtime.crew.size() >= beforeCrew);
    assert(runtime.removeCrewByRace("human", false) == 1);

    return 0;
}
