#include "data/asset_store.hpp"
#include "data/event_database.hpp"
#include "data/ftl_dat.hpp"
#include "data/sector_database.hpp"
#include "data/ship_content.hpp"
#include "game/ship_runtime.hpp"
#include <cassert>
#include <cstdlib>
#include <string>
#include <vector>

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

    wormhole::SectorDatabase sectors(assets);
    sectors.setAdvancedEdition(true);
    assert(sectors.load());
    assert(sectors.select(0, 0) != nullptr);
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

    wormhole::ShipContent content;
    assert(content.open(env));
    content.setAdvancedEdition(false);
    assert(content.loadPlayerShip());
    assert(content.playerShip()->blueprint.id == "PLAYER_SHIP_HARD");

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
