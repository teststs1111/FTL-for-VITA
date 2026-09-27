# FTL for PS Vita — Development Handoff

> Persistent project state. On every continuation, read this file first, then inspect the actual GitHub main branch and GitHub Actions directly.

## Repository / target
- GitHub: https://github.com/teststs1111/FTL-for-VITA
- Branch: main
- Target: a Vita-native FTL implementation that follows actual FTL gameplay/data behavior as closely as practical.
- Priority: fidelity to the original game over visual/prototype shortcuts.
- Proprietary FTL data/assets must never be committed or redistributed.

## Mandatory continuation protocol
1. Read the current HANDOFF.md from GitHub.
2. Check the actual latest commit on main.
3. Check GitHub Actions directly for that exact latest commit.
4. If CI is running/queued, inspect it again before declaring the change green.
5. If CI fails, inspect the failing job log before making the next change.
6. Check the ChatGPT Library file list for the user-supplied ftl.dat; do not ask the user whether it exists unless the Library itself no longer contains it.
7. Continue from the latest verified code state.
8. Update this file after meaningful implementation changes.

CI means GitHub Actions Continuous Integration. Do not substitute 'CI未取得' when the workflow can be checked directly.

## User-supplied real ftl.dat — Library source of truth
- Legitimate user-supplied ftl.dat is already stored in the ChatGPT Library.
- Library file: ftl.dat
- Library path: /ftl.dat
- Current verified size: 280,573,482 bytes
- Use this Library file directly for real-data validation; do not repeatedly ask the user to provide it.
- Do not commit, upload to GitHub, or redistribute it.
- Vita path: ux0:data/wormhole/ftl.dat
- Host: FTL_DAT_PATH=/path/to/your/ftl.dat

## Verified archive facts
- PKG\n reconstructed container
- 3,219 entries
- 2,837 PNG resources
- data/text-ja.xml present, about 920 KB
- data/sector_data.xml present
- AE resources: data/dlcBlueprints.xml, data/dlcBlueprintsOverwrite.xml, data/dlcPirateBlueprints.xml, data/dlcEvents.xml, data/dlcEventsOverwrite.xml
- PLAYER_SHIP_HARD, layout=kestral, img=kestral
- img/ship/kestral_base.png
- AE is inside the same ftl.dat. Do not revert to an external DLC-file model.

## Current verified main state — 2026-09-28
- Latest code changes are being validated now:
  - fa8f57a31aaf1d1798cea7cf2040401a5b2cb904 — Apply Rebel sector entry fleet advance
  - b46f88ecfa76d911873fe0a17fa3fc5c2c906ee8 — Verify pursuit modifiers in real FTL data
- These changes use verified real-data behavior: Rebel Controlled Sector entry advances pursuit by one jump; modifyPursuit values are applied to the following jump.
- Previous verified baseline: Host #823 / Vita #515 succeeded for main handoff commit 9024a89bb1200e04d0d670f5e8b75d61f73d4558.
- Do not mark the new changes green until Host and Vita CI for the latest main commit complete successfully.
- Sector graph, real archive/BXML, blueprint/event/sector databases, Japanese text loading, PNG pipeline, beacon event assignment, Rebel Fleet state, and Last Stand state are implemented to varying prototype/intermediate fidelity.

## Current change — vanilla default NEBULA resolution
Direct inspection of the real Library ftl.dat found a concrete gap: sector_data.xml refers to a special pool named NEBULA, but the archive does not serialize a literal eventList named NEBULA. The default pool is an engine-level vanilla pool.

Commits:
- fa61f02f29f268b22d1e2296031c598db1d8eb34 — Resolve vanilla NEBULA filler pool from real data
- aa6f2e86b5d5a9a96e5546db884ecf2aa76422a1 — Test vanilla NEBULA filler pool resolution

The resolver now constructs the vanilla 15-entry equal-weight pool:
- NEBULA_EMPTY
- NEBULA_REBEL
- NEBULA_AUTO
- NEBULA_AUTO_WARNING
- NEBULA_PIRATE_SMUGGLE
- NEBULA_AUTO_DEFENSE_ITEM
- NEBULA_TRADER
- STORM_REBEL
- STORM_AUTO
- STORM_ITEMS
- NEBULA_LOST_SHIP
- NEBULA_BOARDING
- STORM_BOARDING
- NEBULA_MANTIS_FIGHT
- NEBULA_WEAPONS_TRADER

All 15 were directly confirmed present in the supplied archive. The real-data test resolves NEBULA for 64 seeds and checks that every result belongs to this pool.

## CI state
After each code commit, check GitHub Actions directly for the newest main commit. Do not mark either commit green until both Host and Vita workflows for that exact commit succeed.

Latest pursuit-fidelity work:
- fa8f57a31aaf1d1798cea7cf2040401a5b2cb904 applies the documented +1-jump entry advance for REBEL_SECTOR.
- b46f88ecfa76d911873fe0a17fa3fc5c2c906ee8 adds real-data regression checks for FUEL_FLEET_DELAY (-1), FUEL_FLEET_DISTRESS (+1), and REBEL_SECTOR.
- CI for these new changes is pending; inspect the latest main commit directly before proceeding.

Previous verified real-data regression:
- 9fc176d9859cc41bd2c38660dd82ac1a3940ab5d — Verify real sector nebula pool definitions
- Host #819 / Vita #511 succeeded.

Previous verified pursuit fix:
- 03118ce240003658b8031292d14accbbcc7bc9e5 — Distraction Buoys changed from +1 to -1.
- Main handoff commit 9024a89bb1200e04d0d670f5e8b75d61f73d4558 had Host #823 / Vita #515 success.

## Sector / nebula fidelity
- 6 logical columns × 4 logical rows
- approximately 80% beacon occupancy
- 19–24 beacons
- randomized beacon coordinates
- links only between adjacent grid cells and within 165 px
- bidirectional links
- reachable exit guaranteed by regeneration
- per-beacon nebula state
- NEBULA_* pools processed before ordinary sector pools

Known approximation: exact vanilla beacon coordinate/occupancy sequence and exact cloud-overlap geometry are not yet reproduced. Current implementation selects explicit NEBULA_* beacon counts and grows connected groups, but it does not yet model the actual cloud graphics' overlap test. Do not claim current nebula placement is exact.

Research checkpoint 2026-09-28: external technical references confirm that FTL first places the 6×4 beacon map, processes all NEBULA_* lists before ordinary pools, draws nebula clouds, and converts ordinary beacons overlapped by those clouds into additional nebula beacons assigned from the default NEBULA pool. The exact cloud placement/overlap geometry remains undocumented in the accessible sources, so implementation should not invent a false exact formula.

## Event fidelity
Implemented: real XML ingestion, event load references, weighted eventLists, AE resources/overwrites, common choices/requirements, stores, distress, hostile, repair, resource/crew effects, and original text IDs through data/text-ja.xml.

Still incomplete: complex nested requirements, full blue options, complete quest chains, complete reward semantics, exact unique-event limits, full environmental/ASB behavior, and enemy escape/fleet-pursuit effects.

## Rebel Fleet
Implemented: per-beacon fleetCovered, navigable fleet beacons with Rebel encounters, save/load persistence, normal pursuit position, nebula pursuit modifiers, and Distraction Buoys delay.
Approximation remains in exact fleet frontier/rate timing, special event pursuit changes, ASB/environment replacement, and exact Rebel Elite selection.

## Last Stand
Implemented: Sector 8 entry resources, Flagship/Base state and save/load, 3–5 jump route scaffold, two-player-jump cadence, wait/post-phase behavior, Base countdown, individual takeover, and FINAL event-pool checks.
Approximation remains in exact Flagship/Base placement, takeover timing/selection, and complete Flagship behavior.

## Build/platform
- Host C++17/CMake build and regression tests
- VitaSDK workflow and VPK packaging
- vitaGL 960×544 bring-up
- Vita controller input
- Real Vita hardware gameplay verification is still outstanding

## Progress
- Archive/BXML: ~90%
- Real-data ingestion: ~70%
- Blueprint/ship data: ~60%
- Event system: ~50%
- Sector/beacon/event flow: ~45%
- Rebel Fleet: ~40%
- Combat/runtime: ~45%
- Vita renderer/input: ~35%
- Japanese text/font/UI: ~25%
- Audio: pending
- Save/load: partial
- Mod support: pending
- Overall engineering maturity: roughly 40–45% toward the stated actual-game-faithful playable target.

## Immediate next work
1. Verify Host/Vita CI for the latest pursuit-fidelity commits.
2. Continue tightening Rebel Fleet beacon frontier / exact pursuit-to-map mapping; current raw-position-to-beacon mapping is still an approximation.
3. Tighten fleet-controlled beacon environment/ASB exceptions using the real event data and documented vanilla rules.
4. Continue Sector 8 takeover/Flagship fidelity.
5. Continue deeper event-choice/effect fidelity.

## Non-negotiable rules
- Aim for actual FTL behavior, not a generic FTL-like approximation.
- Use Library ftl.dat as the real-data basis whenever applicable.
- Never commit or redistribute proprietary FTL assets/data.
- AE is contained in the single ftl.dat; do not introduce an external DLC archive model.
- Directly inspect GitHub Actions and failing job logs.
- Distinguish compile failures from test/verification failures.
- Keep changes small and testable.
- Update HANDOFF.md after meaningful changes so a future '続き' can resume without asking the user to repeat context.
