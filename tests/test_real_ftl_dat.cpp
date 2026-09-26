#include "data/asset_store.hpp"
#include "data/event_database.hpp"
#include "data/ftl_dat.hpp"
#include "data/sector_database.hpp"
#include "data/ship_content.hpp"
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

    wormhole::SectorDatabase sectors(assets);
    sectors.setAdvancedEdition(true);
    assert(sectors.load());
    assert(sectors.select(0, 0) != nullptr);
    assert(sectors.select(7, 0) != nullptr);

    wormhole::ShipContent content;
    assert(content.open(env));
    content.setAdvancedEdition(false);
    assert(content.loadPlayerShip());
    assert(content.playerShip()->blueprint.id == "PLAYER_SHIP_HARD");

    content.setAdvancedEdition(true);
    assert(content.loadPlayerShip("data/blueprints.xml", "PLAYER_SHIP_ANAEROBIC"));
    assert(content.playerShip()->blueprint.id == "PLAYER_SHIP_ANAEROBIC");

    return 0;
}
