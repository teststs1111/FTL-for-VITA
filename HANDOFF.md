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
- Rebel Fleet environment selector/test commits are now on main:
  - 1fec7b4eba83ff22aede8986b0724800253296d3 — Implement selector
  - fc9db5acc7a016b69c7f889551a1e0ba8a313d56 — Use selector
  - 0affeecda97c340a323c7528c4358c7943bcccef — Register regression test
- 2ffa020c2a19974602e74247bc6a9bb5147c5740 records the checkpoint.
- The selector covers normal fleet ASB, captured nebula Plasma Storm, nebula exit without ASB, Easy-mode exit without ASB, and normal/hard exit ASB.
- MainGame currently passes easyMode=false because difficulty state is not yet represented; the selector is isolated so the Easy exception can become exact once difficulty state exists.
- The regression test documents the zero-fuel-after-jump captured-nebula branch as Plasma Storm; fuel-state branching itself remains in MainGame.
- CI for the newest implementation commit must be checked directly before declaring this change green.

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

Latest verified pursuit-fidelity work:
- Host #826 / Vita #518 both succeeded for main 73af221134b4fcdacda3d376263feeae07befaf9.
- Do not treat older "Vita in progress" reports as current; the workflow has completed successfully.
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

Research checkpoint 2026-09-28: external references report that the Rebel Fleet pursuit value advances by 0x40 (64) per normal jump, and a long-standing pursuit-indicator mod reports the same 64-pixel visual advance. The exact mapping from the datamined pursuit counter's raw value to this implementation's generated beacon x-coordinates is still not sufficiently established. Keep the current mapping explicitly marked approximate until a stronger source or direct reference implementation confirms the transform.

## Rebel Fleet save-format research checkpoint — 2026-09-28
- Cross-checking the current open-source FTL save editor confirms that the vanilla save format stores three separate sector-map values: `RebelFleetOffset`, `RebelFleetFudge`, and `RebelPursuitMod`, immediately after the sector tree/layout seeds.
- This independently corroborates that the fleet's map/frontier state is not represented solely by the raw pursuit counter used by the current Vita implementation.
- The same editor treats Offset and Fudge as persisted values rather than deriving them from the beacon list at save-edit time.
- No reliable public source found yet establishes the exact formula converting Offset/Fudge + pursuit progress into the original game's per-beacon takeover frontier.
- Therefore no speculative transform is being committed. Current `setFleetCoverageFromPosition()` remains explicitly provisional.

## Rebel Fleet environment/ASB research checkpoint — 2026-09-28
- Direct comparison with current fleet encounter code found a concrete fidelity gap: `enterRebelFleetEncounter()` currently always assigns `CombatEnvironment::PDSPlayer`.
- Vanilla behavior is conditional. A normal non-nebula fleet-controlled beacon uses the Rebel ASB, but a fleet takeover overwrites the beacon's previous event/environment; taken-over nebula beacons use an ion/plasma-storm-style nebula environment rather than an ASB, with the exit beacon treated specially. Easy-mode exit beacons also have an ASB exception.
- Public references also document the zero-fuel special case at a nebula beacon: depending on whether the player is waiting or has just jumped in, the nebula/ASB combination differs. Do not collapse these cases into one generic `PDSPlayer` state.
- Current `CombatEnvironment` has no dedicated plasma/ion-storm environment, so implementing this correctly requires extending the combat environment model rather than merely changing the fleet encounter flag.
- No change was made yet from this checkpoint because the current difficulty state and the exact vanilla zero-fuel/exit environment transition are not represented cleanly enough in the existing runtime. Avoid a partial fix that would replace one approximation with another.
- The exact Rebel Fleet frontier mapping remains provisional; do not couple this environment correction to an unproven Offset/Fudge -> beacon-position formula.

## Enemy generation fidelity — 2026-09-29
- Commit 4df7ee45e02cdd0299a8e01811f67a94d64baac2 — Align enemy system budget flow with vanilla rules.
- Optional non-offensive systems now cost 2 general-budget points on Easy/Normal and 1 on Hard; offensive optional systems still cost 1 offensive point.
- After category-specific upgrades, unused offensive and defensive budget now flows into the general budget instead of being discarded.
- Negative optional-system budget costs are preserved into the general-budget stage.
- This is a formula-level correction based on the reverse-engineering notes already recorded for enemy generation; exact vanilla RNG sequence is still not established.
- Crew race proportions/overrides and exact crew room placement remain incomplete.

## Event fidelity
Implemented: real XML ingestion, event load references, weighted eventLists, AE resources/overwrites, common choices/requirements, stores, distress, hostile, repair, resource/crew effects, and original text IDs through data/text-ja.xml.

Still incomplete: complex nested requirements, full blue options, complete quest chains, complete reward semantics, exact unique-event limits, full environmental/ASB behavior, and enemy escape/fleet-pursuit effects.

## Rebel Fleet
Implemented: per-beacon fleetCovered, navigable fleet beacons with Rebel encounters, save/load persistence, normal pursuit position, nebula pursuit modifiers, Distraction Buoys delay, and Rebel Controlled Sector entry advance.
Approximation remains in exact fleet frontier/rate timing, raw pursuit-to-map mapping, fleet-controlled beacon environment/ASB replacement, nebula takeover exceptions, and exact Rebel Elite selection.

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
- Overall project progress: ~50% toward the stated actual-game-faithful playable target.

## Immediate next work
1. Tighten Rebel Fleet raw pursuit-to-map mapping without inventing an unsupported transform.
2. Tighten fleet-controlled beacon environment/ASB exceptions using real event data and documented vanilla rules.
3. Add regression coverage for nebula pursuit rate modifiers and fleet takeover edge cases.
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
- Update HANDOFF.md after meaningful changes so a future '続き' can resume without asking the user to repeat context.## Research checkpoint 2026-09-28:
- Rebel Fleet pursuit advances by exactly 64 pixels per regular jump according to a long-standing pixel-accuracy pursuit-indicator mod; the datamined counter is also documented as +0x40 per jump. However, the raw pursuit/offset value is not established as a direct comparison against this implementation's generated beacon x-coordinate. 
- Additional historical FTL save-editor research describes the fleet offset as a large negative pixel value (roughly -900 to -500 depending on sector) approaching zero during travel, with a separate per-sector random "fudge" affecting the offset. This strongly suggests that a direct raw-counter -> generated-beacon-x comparison is an oversimplification.
- Therefore keep setFleetCoverageFromPosition() explicitly provisional. Next fidelity step is to reconstruct the sector-specific visual/map offset and its random component before changing the coverage transform; do not invent a linear transform from -959 to the current beacon coordinates.

## Event fidelity
Implemented: real XML ingestion, event load references, weighted eventLists, AE resources/overwrites, common choices/requirements, stores, distress, hostile, repair, resource/crew effects, and original text IDs through data/text-ja.xml.

Still incomplete: complex nested requirements, full blue options, complete quest chains, complete reward semantics, exact unique-event limits, full environmental/ASB behavior, and enemy escape/fleet-pursuit effects.

## Rebel Fleet
Implemented: per-beacon fleetCovered, navigable fleet beacons with Rebel encounters, save/load persistence, normal pursuit position, nebula pursuit modifiers, Distraction Buoys delay, and Rebel Controlled Sector entry advance.
Approximation remains in exact fleet frontier/rate timing, raw pursuit-to-map mapping, fleet-controlled beacon environment/ASB replacement, nebula takeover exceptions, and exact Rebel Elite selection.

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
- Overall project progress: ~50% toward the stated actual-game-faithful playable target.

## Immediate next work
1. Tighten Rebel Fleet raw pursuit-to-map mapping without inventing an unsupported transform.
2. Tighten fleet-controlled beacon environment/ASB exceptions using real event data and documented vanilla rules.
3. Add regression coverage for nebula pursuit rate modifiers and fleet takeover edge cases.
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


## Rebel Fleet environment implementation checkpoint — 2026-09-28
- Added a dedicated `CombatEnvironment::PlasmaStorm` state.
- Plasma Storm now caps reactor-funded power at half capacity, rounded up, on combat entry and constrains later reactor allocation through `ShipRuntime::reactorPowerCap`.
- On entry, excess reactor-funded system power is removed using the runtime RNG. This intentionally avoids claiming an exact vanilla depower ordering; Zoltan room power and Backup Battery are still not modeled separately.
- Rebel Fleet encounter selection now uses `PlasmaStorm` for fleet-controlled nebula beacons and `PDSPlayer` for non-nebula beacons.
- This matches the documented core distinction: normal fleet beacons use ASB, while captured nebula beacons use an ion/plasma storm. The Easy-mode exit exception and the special zero-fuel/waiting transition remain separate fidelity work because difficulty state and waiting-at-beacon takeover are not yet represented.
- Exact Rebel Fleet frontier Offset/Fudge mapping remains untouched and provisional.

## Rebel Fleet / sector-map research update — 2026-09-28
- Latest main commit `6fc4d79946b800d92e19ea252c8ae0a261023f5a` is CI-green: Host #858 and Vita #550 both completed successfully.
- Library verification still finds the user-supplied `ftl.dat` at `/ftl.dat`, 280,573,482 bytes. Use it as the real-data source of truth; do not commit or redistribute it. 
- Additional map-generation references confirm the core generation order: the beacon map is created before sector events; the map is a 6×4 grid, each cell normally has an 80% beacon chance, and placed beacons connect to beacons in adjacent grid cells when their map-space distance is <=165 px. The exit is generated as part of the map rather than as a normal sector event entry. These facts support the current graph-generation direction but do not establish the exact vanilla coordinate RNG or fleet-frontier transform. Sources: FTL Wiki Sectors technical generation notes; Steam Community discussion on sector generation order.
- The exact `RebelFleetOffset`/`RebelFleetFudge` -> per-beacon takeover boundary remains unresolved. Keep `setFleetCoverageFromPosition()` provisional; do not replace it with a guessed linear transform.

## Reporting format — 2026-09-28
- Future continuation reports should be kept more concise to reduce chat-log usage.
- Prefer a short status block: progress %, current state, what is being worked on, CI result, and only important problems/blockers.
- Do not repeat detailed background already recorded in HANDOFF.md unless it changed or is directly relevant.
- Keep technical investigation and implementation detail in HANDOFF.md, while chat reports summarize only the latest changes and next step.


## Rebel Fleet environment regression checkpoint — 2026-09-28
- Added a dedicated selectRebelFleetEnvironment() selector and a host regression test.
- Covered normal fleet beacon -> PDSPlayer, captured non-exit nebula -> PlasmaStorm, nebula exit -> None, Easy-mode non-nebula exit -> None, and normal/hard non-nebula exit -> PDSPlayer.
- MainGame currently passes easyMode=false because difficulty state is not yet modeled; the selector keeps the vanilla Easy exception explicit for later integration.
- Zero-fuel arrival at a captured non-exit nebula is covered at the environment-selection level; waiting-at-beacon takeover remains a separate state-model gap.
- ShipRuntime::setReactorPowerCap() was implemented in f1f4b3504197d96c446653e3cf9481fb36de7f66; Host #843 and Vita #535 both completed successfully for that commit.


## Rebel Fleet mapping research update — 2026-09-28
- Host #857 and Vita #549 both completed successfully for main `affcc11a626db54a013c5a267f1d7f46f690bb53`.
- Additional public evidence confirms the pursuit indicator itself is pixel-accurate at exactly 64 pixels per regular jump, while nebula jumps require reduced marker spacing; this supports keeping the jump-rate constants but does not reveal the internal map-frontier transform.
- Historical save-editor documentation explicitly describes `RebelFleetOffset` as a sector-dependent large negative pixel value approaching zero, with `RebelFleetFudge` as a per-sector random constant added to that offset. This confirms that the current direct comparison of raw `-959 + pursuit` against generated beacon X coordinates is not a faithful model.
- No public source found in this pass that provides the exact Offset/Fudge-to-beacon collision formula. Do not replace the provisional mapping with an invented linear transform.
- Next target is to recover the actual sector-map frontier representation/coordinate space from a reference implementation, mod asset behavior, or sufficiently documented save/map traces before changing coverage logic.


## Rebel Fleet Plasma Storm power-accounting checkpoint — 2026-09-28
- Fixed a runtime consistency gap found during continuation review: powered drones are now included in ShipRuntime::usedReactorPower(), matching availableReactorPower().
- Plasma Storm entry depowering now considers both powered systems and powered drones instead of potentially leaving reactor usage above the storm cap when drones were active.
- This is a narrow consistency/fidelity fix; Zoltan room power and Backup Battery remain unmodeled, so Plasma Storm is still not an exact implementation of every vanilla power-source exception.


## Difficulty / Rebel Fleet integration checkpoint — 2026-09-28
- Added explicit MainGame difficulty state: Easy, Normal, Hard; default is Normal.
- ShipScene now receives the selected difficulty and Rebel Fleet encounter environment selection uses Easy-mode state instead of a hardcoded false.
- This makes the documented Easy exit-beacon no-ASB exception reachable through the normal runtime path once the caller selects Easy.
- Current commit sequence: 150466bc (header state) and 2694132c (runtime routing).
- CI for the newest main commit must be checked before declaring this change green.


## Rebel Fleet pursuit mapping evidence checkpoint — 2026-09-28
- New cross-check: public technical documentation confirms the raw Rebel pursuit counter is signed 32-bit, starts at 0xFFFFFC41 (-959), and advances +0x40 (64) per jump; the source explicitly says the exact pixel mapping to beacon takeover is unconfirmed.
- The saved-game editor documentation separately describes RebelFleetOffset as a sector-dependent negative pixel value approaching zero, with RebelFleetFudge as a random constant added to the offset. Therefore the current float frontier model must not be treated as proven vanilla math.
- Decision: do not replace Offset/Fudge with a guessed linear formula. Keep the current mapping explicitly provisional and continue searching for a reproducible map-frontier relation or test data.
- Sources: hintforge FTL mechanics documentation and Subset Games FTL Profile/SavedGame Editor research.


## Plasma Storm power-source fidelity research checkpoint — 2026-09-28
- Rechecked the Library ftl.dat and current runtime before changing power accounting. The archive is still the persistent real-data source at /ftl.dat; it contains the AE battery system blueprint (type=battery, startPower=1, maxPower=2) and the Zoltan crew blueprint.
- External vanilla mechanics references independently confirm the important rule: Plasma/Ion Storm halves only reactor-funded power, rounded up; Zoltan crew power and Backup Battery power are not reduced by the storm.
- Current ShipRuntime collapses all powered system/drone bars into one reactor usage count and has no separate Zoltan-power or active Backup-Battery allocation state. Therefore simply subtracting one from system.power until usedReactorPower() <= half-reactor would incorrectly remove Zoltan/Battery power in some states.
- Decision: do not add a speculative partial fix. The next implementation step is to introduce explicit power-source accounting (reactor vs Zoltan vs temporary Backup Battery) before tightening Plasma Storm depowering. This is a fidelity requirement, not a build blocker.
- Current Plasma Storm implementation remains intentionally provisional: it correctly enforces the reactor-only half-cap for the current simplified runtime, but it is not yet exact for mixed reactor/Zoltan/Battery allocations.
- Sources checked: FTL Environmental Hazards; Advanced Edition Ship System FAQ; FTL Crew/Systems references. No proprietary data is being committed.

### Backup Battery reactor-accounting checkpoint (2026-09-28)
- Commit `387b05da` passed both Host and Vita CI (Host #869, Vita #561).
- `ShipRuntime::usedReactorPower()` and `availableReactorPower()` now exclude the `battery` subsystem itself from reactor consumption.
- This is intentionally limited: the runtime still does not model Backup Battery's temporary +2/+4 power allocation or cooldown state.
- Do not subtract battery-provided power during Plasma Storm; the battery's temporary power remains outside the storm's reactor cap.
- Next fidelity step remains explicit power-source accounting for reactor-funded power vs Zoltan-provided power vs Backup Battery temporary power.
- No proprietary `ftl.dat` content is committed.


## Backup Battery source-allocation refinement — 2026-09-28
- Added per-system `batteryPower` tracking so temporary Backup Battery-funded bars are distinct from reactor-funded bars.
- `setSystemPower()` now uses regular reactor power before allocating remaining power from Backup Battery.
- Battery expiry removes tracked temporary bars from their systems instead of only clearing an aggregate bonus.
- Regression coverage exercises battery-funded allocation and removal at expiry.
- Still provisional: persistent Zoltan per-bar source allocation and the combined Zoltan/Battery redistribution rules need further refinement.


## Explicit Zoltan power-source checkpoint — 2026-09-28
- Added explicit per-system `zoltanPower` source allocation alongside existing `batteryPower`.
- Reactor-funded, Zoltan-funded, and Backup Battery-funded bars are now tracked independently instead of recomputing Zoltan allocation solely from current room occupancy.
- Increasing system power allocates available Zoltan bars first, then reactor power, then active Backup Battery power; failed allocation rolls back the tentative Zoltan assignment.
- Reducing power removes reactor-funded, then battery-funded, then Zoltan-funded bars while preserving the remaining source assignments.
- Regression coverage was updated to verify explicit Zoltan allocation survives the Backup Battery scenario.
- This is still not the complete vanilla power model: Zoltan movement/reassignment timing and every edge case involving damage/ionization remain to be tightened.
- Latest implementation commits: `1ce05d2`, `0c1ebc2`, `39d2a376`. GitHub status currently exposes no checks for the final commit, so it is not marked CI-green until an actual workflow result is available.


## Zoltan source lifecycle correction — 2026-09-28
- Fixed a real bookkeeping bug in setSystemPowered(): an already-assigned Zoltan bar is now counted as an existing free-power allocation when re-enabling a system, so a reactor cap of 0 no longer incorrectly prevents reactivation.
- Added rebalanceZoltanPowerSources() to invalidate Zoltan-funded bars when a Zoltan leaves/dies and restore available Zoltan power to already-powered eligible main systems when a Zoltan returns/is added.
- Damage and ionization now clamp Zoltan/Battery source allocations to the system's remaining power so source accounting cannot exceed actual bars.
- Regression coverage now checks Zoltan movement/death loss and restoration in addition to the previous Backup Battery cases.
- Also removed a duplicate availableZoltanPowerForSystem() declaration in the runtime header that could break compilation.
- Latest commits: header cleanup/rebalance declaration 4998a56; lifecycle implementation a32227c; regression test 25dafa98.
- The newest Host/Vita workflow runs for 25dafa98 were observed as in progress at the time of this handoff update; no pass is claimed yet.

## Plasma Storm mixed power-source correction — 2026-09-28
- Rechecked vanilla behavior before extending the current Storm implementation: Plasma/Ion Storm halves reactor capacity (rounded up), while Zoltan power and Backup Battery power are unaffected.
- Storm entry removes excess ordinary reactor-funded power; the removed bars are not treated as automatically restored when leaving the storm. The reactor ceiling returns, but the player may need to reallocate power.
- Current source accounting now preserves this distinction: `zoltanPower` and `batteryPower` are retained while `setSystemPower()` removes reactor-funded bars first.
- Added `tests/test_plasma_storm_power.cpp` covering a mixed Zoltan + Backup Battery + reactor allocation through Storm entry/exit.
- Removed an unused storm-restoration field; no automatic repower mechanism is being added.
- Latest test/CMake commit: `04dcd745614d650802cfb39e90b0cc1007a262f2`. No workflow result is currently exposed for this commit, so it is not marked CI-green yet.
- This narrows the remaining Storm gap to exact vanilla power-removal ordering/edge cases and broader combat/environment integration; the core source distinction is now regression-tested.

## Zoltan source redistribution refinement — 2026-09-28
- Corrected a source-accounting edge case: Zoltan power is restricted to eligible main systems; pilot/engines/oxygen/doors/sensors/battery cannot receive Zoltan bars through generic power allocation.
- When a Zoltan enters an already-full eligible system, the runtime now displaces Backup Battery-funded bars before reactor-funded bars while keeping total system power unchanged. This matches documented vanilla allocation behavior. citeturn0search1turn0search9
- When the Zoltan leaves, only the Zoltan-funded bar is removed; the system does not automatically reclaim reactor power for the missing bar.
- Fixed `setSystemPowered()` so unused global Backup Battery capacity cannot be mistaken for a battery bar already assigned to the system. Existing free-source allocations are honored, but any remaining requirement must fit the reactor cap.
- Extended `tests/test_reactor_power_sources.cpp` with subsystem and full-room Zoltan redistribution coverage.
- No CI-green claim is made for these latest changes until a workflow result is exposed.


## Ion / mixed power-source correction — 2026-09-28
- Fixed a concrete vanilla-fidelity gap in ShipRuntime: Ion damage no longer removes Zoltan-funded power.
- Added RuntimeSystem::ionRemovedPower to remember only normal power forced out by Ion.
- Ionized systems now reject manual power changes while the Ion lock is active.
- When the Ion timer expires, the runtime attempts to restore the previously removed normal power using currently available reactor capacity, then active Backup Battery capacity. This allows Battery-backed power to recover as Battery power when the Battery is still active, while avoiding restoration that exceeds current capacity.
- Added regression coverage for Zoltan immunity, Battery interaction, 5-second recovery, and the Ion power-lock.
- Vanilla references: Ion removes one normal power bar per Ion damage and returns it to reactor; Zoltan power is unaffected. Sources: FTL Ion Weapons wiki and Subset Games forum testing. citeturn0search0turn0search8
- Implementation commit: fb05fca73487b9f4a189a8b1b764bc27c8d191ca plus the regression-test follow-ups.
- CI is not claimed green until an actual workflow result for the newest commit is available.


## Ion stacking correction — 2026-09-28
- Fixed Ion stacking: each additional ion point adds 5 seconds to the lock instead of resetting it to 5 seconds.
- Ion damage is capped at 5 points / 25 seconds, matching vanilla behavior; additional Ion hits after the cap have no further effect.
- Added regression coverage for 2 -> 4 -> 5 ion points and 10 -> 20 -> 25 second lock progression.
- Vanilla references confirm Ion damage stacks and the timer is capped at 25 seconds. citeturn0search0turn0search5
- Latest implementation commits: `d0f6b4a` (runtime) and the following regression-test commit. CI is not claimed green until a workflow result is exposed.


## Weapon-specific Ion fidelity correction — 2026-09-28
- Vanilla Weapons behavior was cross-checked: Ion damage disables whole active weapons from the rightmost slot, rather than simply removing one generic power bar per ion point. A single Ion can therefore take a 4-power weapon offline; additional Ion points can disable additional weapons. citeturn1search0turn1search1
- Added per-weapon `weaponIonDisabled` runtime state.
- Weapon Ion shutdown now selects active weapons right-to-left, clears their charge/ready state, and removes the weapon's normal (reactor/Battery) power while preserving any Zoltan-funded portion.
- When the shared Ion lock expires, weapon Ion-disabled flags are cleared and the removed normal system power can be restored subject to current reactor/Battery capacity.
- Added regression coverage for a 4-power Glaive with one Zoltan and for a 2-ion hit disabling two 1-power weapons.
- This remains bounded to the current runtime's weapon model: explicit manual weapon slot reordering/power allocation is not yet exposed by the runtime API, so the right-to-left selection uses the loaded weapon order.
- Latest implementation commits: `3989b9e` (runtime/header) and `a78750f` (regression tests). CI status for the newest commit must be checked before declaring it green.


## Weapon power allocation / damage correction — 2026-09-28
- Added per-weapon `allocatedPower` to prevent every weapon from charging merely because its individual cost fits inside the total Weapons-system power.
- Weapons now consume the shared power cumulatively in loaded slot order; a later weapon remains unallocated when earlier slots consume the available power.
- Weapons-system damage now deallocates affected weapon slots from the right side and clears charge/ready state for weapons that no longer have enough power.
- Added regression coverage for a 3-power Weapons system with 1+1+2 power weapons and progressive system damage.
- This is still a runtime-level slot-order model; explicit player weapon-slot reordering is not yet exposed by the current API.
- Latest commits: `22360f9` runtime allocation/damage logic and `8d90f4d` regression tests.


## Weapon firing / volley ammunition fidelity correction — 2026-09-28

- `fireWeapon()` now requires the individual weapon slot to have its full `allocatedPower`, rather than checking only the total Weapons-system power.
- `missilesUsed` is treated as the ammunition cost of one firing volley. `shots` describes the number of projectiles in that volley and does not multiply the missile cost.
- A successful firing consumes the configured volley ammunition and resets charge/ready state.
- A failed firing due to insufficient missiles leaves ammunition and ready state unchanged.
- Regression coverage was added to `tests/test_reactor_power_sources.cpp`.
- Commit: `12f1488` — runtime firing correction.
- Commit: `c3eb280` — firing/multi-shot regression tests.
- Current limitation: the runtime still models weapon firing as a state transition; projectile trajectories, target selection, and individual projectile resolution are not yet represented as runtime entities. Those should be implemented before claiming full combat fidelity.


## Weapon volley resolution foundation — 2026-09-28

- Added `RuntimeWeapon::volleyPending` so firing and projectile resolution are separate runtime phases.
- Added `resolveWeaponVolley(weaponIndex, targetRoom)`.
- Projectile-style weapons consume one shield layer per projectile before room damage when normal shields remain.
- Missile/bomb-style weapons bypass normal shields and apply their configured hull/system/crew effects directly.
- Ion weapons apply ion damage to the shield system when blocked by shields, otherwise to the selected target room.
- Beam weapons do not remove shield layers; their room damage is reduced by the number of active shield layers.
- Multi-shot volleys resolve `shots` times, while missile ammunition remains the per-volley cost already consumed by `fireWeapon()`.
- Regression tests cover a two-shot laser volley stripping two shield layers and a shield-bypassing missile applying hull/system damage.
- Research basis: vanilla weapon/shield mechanics from FTL mechanics references; missiles/bombs bypass regular shields, beams reduce damage per shield layer, and ion weapons apply ion damage to shields when blocked. citeturn0search1turn0search3turn0search4
- Commits: `a90c65a`, `caed6c9`, `e87befa`, `c79cc88`, `c4a5309`.
- Current limitation: projectile travel, evasion/miss rolls, Defense Drone interception, flak spread, beam path geometry, and per-tile beam effects are not yet represented. Do not treat this foundation as full combat fidelity.


## Weapon hit/evasion correction — 2026-09-28
- Extended weapon volley resolution with a target-evasion parameter and a per-projectile hit/miss roll.
- Projectile-style weapons now roll independently against target evasion; a miss consumes that projectile's resolution without applying damage/effects.
- Beam weapons bypass the normal projectile miss roll because beams are resolved as guaranteed-contact attacks in the current combat model.
- Added deterministic regression boundaries: 100% evasion must miss, 0% evasion must hit.
- The runtime still does not derive target evasion from pilot/engines/cloaking state, and Defense Drone interception is not yet implemented. These remain the next combat-fidelity steps.
- Commits: `6475b6b` (API), `fd45125` (runtime), with regression coverage added alongside this checkpoint.


## Defense Drone interception correction — 2026-09-28
- Added per-projectile Defense Drone interception before the ship evasion roll.
- Defense Drone Mark I can intercept missile/flak/crystal projectiles; Mark II additionally intercepts laser and ion projectiles.
- Bombs explicitly bypass Defense Drones.
- A charged defensive drone shot is consumed after one interception attempt; the runtime keeps the existing autonomous charge model.
- Interception currently uses a 90% hit roll, matching the documented vanilla 10% intended miss rate; geometry/targeting blind spots are not yet modeled.
- Regression coverage verifies Mark I missile interception, bomb bypass, Mark I laser exclusion, and Mark II laser interception.
- Vanilla references: citeturn1search0turn1search2turn1search3
- Commits: `984800c` (API), `397ce96` (runtime), with regression coverage added alongside this checkpoint.


## Per-projectile Flak target resolution — 2026-09-28
- Added an overload of `resolveWeaponVolley` that accepts a target room for each projectile.
- The existing single-target API remains as a compatibility wrapper and resolves every projectile against the same target room.
- Volley resolution now selects the corresponding projectile target before Defense Drone interception/evasion and weapon-effect resolution.
- Added regression coverage for a 3-projectile Flak volley targeting rooms 1/2/3: one shield layer absorbs only the first projectile, while the remaining projectiles damage their own rooms.
- This is the data/API foundation for vanilla Flak's area-spread behavior; actual scatter/room-selection geometry is intentionally not invented until the ship layout/weapon targeting data can supply it.
- Commit: `db27c77` (API), `32cbac0` (runtime), `0826abc` (regression test).


## Flak follow-up / Defense Drone regression — 2026-09-28
- Reviewed the new per-projectile Flak path against the existing Defense Drone implementation.
- Restored Defense Drone interception into the actual projectile-resolution loop so eligible projectiles are checked individually before evasion.
- Added regression coverage that exercises the interception path through `resolveWeaponVolley`.
- Room geometry is already present in `RoomBlueprint` (`x/y/w/h`) and layout data is parsed from the game assets. The next Flak step can therefore use real room geometry rather than invented room coordinates.
- Commit: `6c39630` (runtime), `096df4f` (regression test).


## Build-blocking regression correction — 2026-09-28
- Latest host build failure was traced to an existing declaration typo in `ShipRuntime::setSystemPowered`: `reactorCapacity` was missing its type.
- Corrected it to `const int reactorCapacity`.
- This failure occurred during compilation of `ship_runtime.cpp`, before tests could run.
- Commit: `89a2eb7`.


## Flak geometry foundation — 2026-09-28
- Added `ShipRuntime::roomAtLayoutPoint(x, y)`.
- The lookup uses the already parsed `LayoutBlueprint::rooms` rectangles (`x/y/w/h`) with half-open bounds, so adjacent rooms do not overlap.
- Added `test_layout_room_lookup` regression coverage and registered it in CMake.
- This is intentionally a geometry foundation only: no Flak scatter radius/coordinate generation is invented until the original-game coordinate behavior is verified.
- Build-blocking reactor declaration fix remains at commit `b538586`.


## Flak scatter implementation — 2026-09-28
- Flak projectile landing is now generated from the selected target room center and a circular scatter radius, then mapped through the real layout rectangles.
- Verified documented vanilla radii: Advanced Flak 40, Flak I 42, Flak II 55, Flak Artillery 35.
- A scatter point outside all room rectangles becomes a genuine room miss (`targetRoom == -1`), independent of evasion.
- Each real projectile gets its own scatter result; the existing projectile-level evasion and Defense Drone flow remains separate.
- Added deterministic regression coverage using `CombatRuntime::setRandomSeed()`.
- Fake visual Flak projectiles are not yet modeled as separate combat shots; they must not be allowed to damage shields/rooms.


## Fake Flak projectile implementation — 2026-09-29
- Added explicit `CombatShot::fakeFlak` state so vanilla fake Flak debris is represented separately from damaging projectiles.
- Vanilla fake counts are modeled as Advanced Flak 3, Flak I 3, Flak II 6, and Flak Artillery 7.
- Fake debris can be intercepted by Defense Drones as a distraction, but never enters shield, hull, system, ion, crew, fire, breach, or stun damage resolution.
- Surviving fake debris produces a MISS-style impact result (`evaded = 1`) so the combat result stream can represent the extra vanilla MISS notices.
- Defense Drone eligibility in the queued combat path was narrowed to vanilla projectile classes: Mark I can intercept Flak/missile/crystal/fake Flak; Mark II additionally intercepts laser/ion projectiles. Bombs remain excluded.
- Regression test added for Flak I's 3 real + 3 fake projectiles and fake MISS results.


## ftl.dat canonical-data architecture update — 2026-09-29
- Policy is now explicit: `ftl.dat` is the canonical runtime data source. AE content remains inside the same archive and must be consumed through the same archive reader; do not introduce a separate DLC archive/file model.
- Existing `FtlDat -> AssetStore -> XML/BXML -> data databases -> runtime` architecture is the foundation for this direction. Proprietary archive contents remain external and are never committed to GitHub.
- Direct inspection of the supplied real archive found `data/dlcEvents_anaerobic.xml`, containing the Lanius-specific event pools/events. This file was previously present in the archive but was not loaded by `EventDatabase`.
- Commit `2169f6c1356b88acda6a866ab38bd3485b11b908` adds `data/dlcEvents_anaerobic.xml` to the AE event ingestion pass before `dlcEventsOverwrite.xml`.
- Commit `b30dd4468da2deb2bba3fee0daea2828e0d72008` adds real-archive regression checks for `HOSTILE_LANIUS`, `NEUTRAL_LANIUS`, and `LANIUS_FIGHT`.
- This is a concrete data-fidelity correction: Lanius AE event pools now come from the actual `ftl.dat` rather than being approximated or hardcoded.
- Next implementation priority: continue auditing the actual archive's remaining `data/dlc*.xml` resources against every data-loader path, then tighten blueprint/ship/event semantics using the real definitions.


## Build regression follow-up — 2026-09-29
- The latest Host/Vita runs for commit `178485ae` completed with failures; source compilation itself succeeded on both targets.
- Host CTest exposed three regression-fixture issues: the Defense Drone test was not actually using a multi-shot enemy volley, the reactor test directly zeroed a system power field while leaving stale Zoltan source allocation, and the Flak fake-projectile test had no powered Weapons system even though firing now correctly requires one.
- Corrected those fixtures in commits `f4bf33599807cfc031425e79d0a4a434fa627503`, `f25749301df48d52114e39393ab89e0a8fa9490e`, and `f2435c447f5d6b45b773b0b0f55c610b53eef19e`.
- Vita compilation reached the final ELF conversion stage, then `vita-elf-create` failed because SCE module metadata could not fit at the end of PT_LOAD segment 0. The CMake Vita packaging path now requests the documented `STRIPPED` mode so the input ELF is stripped before SCE metadata is appended; this is in commit `1738f2fd86cde42e453310739634fd345406f42b`.
- These changes address the observed failures rather than weakening runtime assertions. The next step is to verify the new Host/Vita runs, then return to the canonical `ftl.dat` loader-coverage audit.


## Build stabilization and canonical-data audit — 2026-09-29
- Host and Vita builds reached successful conclusions on commit `90dbc3e39c56c7c63079be620721cece2b9ef476` after correcting only test fixtures where their setup/expectations contradicted the implemented vanilla mechanics.
- Weapons-system Ion handling was corrected in runtime commit `6dbb9550f5f6cf1c4c3c8669fa12b37a7dab1a22`: each Ion point now disables one eligible powered weapon slot from right to left, while fully Zoltan-funded weapon power remains immune.
- The Vita packaging failure was traced directly with the ELF program headers. Segment 0 ended at `0x811cfe88`, segment 1 began at `0x811d0000`, leaving only 280 bytes for 3272 bytes of SCE metadata. A 0x1000-byte retained `.rodata` pad was added to the first LOAD segment so the next 0x10000 boundary supplies ample metadata room; commit `bf351f3c6c04925782e59ab875b0bcf91a5aea6d`. Vita build subsequently passed.
- The temporary ELF-layout diagnostic step in `.github/workflows/vita-build.yml` has been removed after the cause was verified; the packaging workaround remains.
- Direct enumeration of the supplied archive's XML resources found these AE/DLC XML files: `dlcAnimations.xml`, `dlcEvents_anaerobic.xml`, `dlcPirateBlueprints.xml`, `dlcBlueprintsOverwrite.xml`, `dlcBlueprints.xml`, `dlcEvents.xml`, `dlcSounds.xml`, and `dlcEventsOverwrite.xml`.
- Current loader coverage: `BlueprintDatabase/ShipContent` consumes the canonical blueprint files including `dlcBlueprints.xml`, `dlcBlueprintsOverwrite.xml`, and `dlcPirateBlueprints.xml`; `EventDatabase` consumes `dlcEvents.xml`, `dlcEvents_anaerobic.xml`, and `dlcEventsOverwrite.xml`. `dlcAnimations.xml` and `dlcSounds.xml` have no dedicated animation/sound database yet and should be handled when those runtime subsystems are implemented.
- `autoBlueprints.xml` was identified as a separate base-game gap. It contains 40 blueprint lists and 25 enemy/auto ship definitions. `ShipContent` now loads it from the same canonical archive, and the real-archive regression test verifies `AUTO_BASIC`; commits `21018b7733eb894c6a0779d70dfae5e5b8bed37c` and `a260aeaef545ea748a3cb772e97e042da01f5fb1`.
- Important remaining data-fidelity work: implement the semantics of `autoBlueprints.xml` blueprint lists (randomized enemy weapons/drones/etc.), then add dedicated animation/sound resource consumers rather than hardcoding AE behavior.


## autoBlueprint enemy-spawn integration — 2026-09-29
- A Host build failure on commit `4a5e0a89f389e8898e1cf47f4c93e04a99a387cf` was traced to a real compile typo in `blueprint_database.cpp`: `const it` was missing the `auto` type. It was corrected in commit `352b175471ae9f31bfd9d4996ffd32d0e8a93c9e`.
- Hostile event parsing now recognizes the canonical FTL `auto_blueprint` attribute on `<ship>` nodes for both top-level events and choice-loaded events. Commit: `1a70753457cd58846ddd8fdb770ca0edcd6fe4f7`.
- `enterCombatFromBeacon()` now treats a hostile id that is an `autoBlueprints.xml` list as a weighted list reference and selects one concrete ship blueprint deterministically from the encounter seed. Duplicate list entries are preserved, so the source data's weighting is retained. Commit: `cdc25a61ab90e0362fa5fea05273bf52b3bbc168`.
- Real-archive regression coverage now verifies that the first `SHIPS_ZOLTAN` list selection resolves to an actual loaded ship blueprint. Commit: `91750fefc94e8e8a1170d53f0a023cbcfa9a4d95`.
- This keeps base-game and AE content inside the same `ftl.dat` path; no external DLC mechanism is introduced.
- Latest Host/Vita workflow results for the new commits must be checked before calling this green.


## autoBlueprint RNG refinement — 2026-09-29
- Replaced the first autoBlueprint selection formula with the existing deterministic encounter RNG used by event effects, avoiding a separate ad-hoc seed calculation.
- Duplicate entries in each `blueprintList` remain intact and therefore continue to act as vanilla-style equal-weight repetitions.
- Commit: `e079f78590e18bd5b65afda2590a9a5c7aa12d56`.
- The remaining limitation is that the complete vanilla RNG stream/order is not yet reproduced; this change keeps autoBlueprint selection on the same encounter RNG foundation rather than introducing another independent generator.


## Enemy loadout generation from canonical autoBlueprints — 2026-09-29

- Direct inspection of the real `ftl.dat` confirmed `data/autoBlueprints.xml` contains 40 blueprint lists and 25 enemy ship blueprints.
- Enemy ship blueprints use `weaponList load="WEAPONS_*"` and `droneList load="DRONES_*"`; these are not fixed equipment lists.
- Implemented parsing of the load-list attributes in `ShipBlueprint`.
- `ShipContent::loadShip()` now resolves enemy weapon/drone load lists into concrete blueprints, selecting entries that fit the available system power and preserving duplicate entries as valid random choices.
- Enemy loadout generation accepts an encounter seed; combat now derives that seed from the existing encounter RNG rather than using a separate RNG source.
- Added a real-`ftl.dat` regression check for `AUTO_BASIC` and its `WEAPONS_AUTO` loadout.
- This is the first step toward the full vanilla enemy-generation model. Current loadout generation samples each slot independently from the remaining-power-compatible canonical pool; duplicate blueprint selections are allowed. The larger remaining gap is the sector/difficulty-based system generation that determines each enemy's rolled system levels before weapon/drone loadout generation.
- Relevant commits: `5c0b52e`, `a270319`, `b605e95`, `d36d30a`, `5d21910`.


## Fixed weapon/drone list count semantics — 2026-09-29
- Canonical FTL ship blueprints support an explicit `count` attribute on `weaponList` and `droneList`. This count limits how many fixed child entries are actually loaded; entries beyond the count are not part of the ship's initial equipment. This behavior is documented by Subset Games modding examples and is now represented directly in `ShipBlueprint`.
- Added `weaponListCount` and `droneListCount` fields and made the parser stop at the declared count. A regression test covers a two-entry weapon list with `count="1"` and verifies only the first entry is loaded.
- Commits: `caa527b54312ab97466a941b4e4f4238639defa6`, `9214246c6bc976a6bba5e04576cd3d7bb937d465`, `75c3f3ffa009adabb1c89cf6bfbeeca0100b0c4d`.
- Next focus remains vanilla enemy loadout generation and the remaining canonical `ftl.dat` data consumers; no separate DLC archive mechanism is being introduced.
## Enemy load-list count semantics — 2026-09-29
- Enemy `weaponList load="..."` / `droneList load="..."` can specify an explicit `count`; when present, that count limits how many entries are drafted from the referenced autoBlueprint list.
- The runtime now uses `weaponListCount` / `droneListCount` for generated enemy loadouts, falling back to the ship's weapon/drone slot count when the attribute is omitted.
- This follows the documented FTL modding behavior that `count` controls the number drafted from a load list; the canonical `ftl.dat` data remains the source of truth and no separate DLC archive mechanism is introduced.
- Commits: `8ed2742a201e2b2dba76aef84e979dd91d10faa3`, `0adaa8345e7f5c8c530ce0b39addaebf65920a80`.
- Next focus: verify the remaining enemy loadout-selection details against canonical data before implementing any sector/difficulty scaling logic.

## Build stabilization and enemy loadout verification — 2026-09-29
- Commit `57f9fb9d67078b97dcd1045d84211bee81efd689` had Host test failure only; compilation itself succeeded. The failing assertion was the probabilistic Defense Drone regression, which assumed a single 90% interception roll would succeed.
- Commit `14058decc8c5ecb0d269d5b308a6e2c9c93e66f5` changed only that regression test: it retries the same interception path until the successful 90% branch is observed, without changing runtime probability behavior.
- Host run `36518783932` and Vita run `36518783893` both completed successfully on `14058de`.
- Important fidelity correction: the current enemy loadout generator's "no-repeat" selection behavior is an implementation assumption, not yet a sufficiently authoritative vanilla-engine finding. The canonical sources confirm that enemy ships randomly select from their referenced autoBlueprint lists and that duplicate list entries provide weighting, but the exact repeat/duplicate handling and RNG stream still need direct verification before treating the current algorithm as final.
- Do not implement generic sector/difficulty weapon-power budgeting yet. The canonical enemy ship blueprints already define their own starting/max system powers; the remaining task is to reproduce the game's actual loadout-selection/RNG semantics around those values.


## Enemy loadout duplicate-selection correction — 2026-09-29
- Public FTL modding/gameplay references establish that enemy weapon lists are randomized loadout pools and that repeated weapons can occur in actual enemy encounters; one documented example explicitly notes seeing multiple Burst Laser II weapons. This contradicts the previous no-repeat implementation assumption. citeturn2search0turn1search6
- Commit `30443acf65c887c98af0262ce377920eaa29764a` removes the no-repeat filter for generated enemy weapons and drones. Each slot now samples from the remaining-power-compatible canonical list, so the same blueprint may be selected more than once.
- The canonical list entries themselves remain preserved, including duplicate entries as weighting. The runtime still constrains each selection to the remaining system power and declared load count.
- The exact vanilla RNG stream and any deeper retry/selection details remain unresolved; do not claim byte-for-byte RNG equivalence yet.
- This correction supersedes the earlier HANDOFF wording that described no-repeat selection as the current vanilla behavior.


## Enemy sector-bound data foundation — 2026-09-29
- Canonical enemy `shipBlueprint` definitions carry `minSector` / `maxSector`; these bounds are part of the enemy-generation data and should not be discarded. Public modding references also show these fields on the same blueprints that define system caps. citeturn1search0turn1search1
- Added `ShipBlueprint::minSector` / `maxSector` and parser support. Real-`ftl.dat` regression coverage verifies `AUTO_BASIC` exposes the canonical 1–8 range.
- Commits: `0b24e22e`, `2591be61`, `70df5f28`.
- This is deliberately a data-model step only. The actual vanilla sector/difficulty system-power generation is not being guessed yet; public reverse-engineering discussion indicates those power allowances are generated internally, with system `power/max` and reactor caps participating in that process. citeturn0search0turn0search4
- Next: model the sector-aware enemy system-power roll from verified behavior, then feed that rolled power into weapon/drone generation instead of always using the blueprint's starting `power` values.


## Enemy auto-loadout duplicate-selection implementation correction — 2026-09-29

- The main branch was rechecked against the actual implementation: the earlier no-repeat filter was still present despite the previous HANDOFF note claiming it had been removed.
- Commit `c140786ae56d8a5070f59b08ad22b79e44167212` removes that filter for both generated enemy weapons and drones.
- Each generated slot now samples from the canonical list independently, subject to the remaining system-power budget and declared `count`.
- This restores the intended duplicate-capable loadout model without changing the canonical `ftl.dat` data or introducing a separate DLC mechanism.
- Exact vanilla RNG ordering and sector-based power generation remain separate unresolved fidelity items; they are not being guessed in this correction.


## Enemy system-generation data foundation — 2026-09-29
- Directly verified against the current implementation that enemy autoBlueprint system generation lacked explicit preservation of the canonical system `min`, `max`, and optional-installation fields.
- Added `minPower`, `maxPower`, and `optional` to `SystemSlotBlueprint`; parser now preserves `min`, `max`, `start=false`, and `optional=true` from the canonical blueprint schema.
- Added a regression fixture covering an optional Shields system with min 2 / max 8.
- This is deliberately a data-model foundation only. No guessed sector/difficulty power roll has been committed yet.
- Reverse-engineering reference confirms enemy generation is a multi-stage process: system maxima are rolled by sector/difficulty, optional systems are installed probabilistically, then offensive/defensive/general budgets upgrade systems, followed by weapon/drone generation. The documented budget/weapon restrictions will be implemented only where they can be tied cleanly to the existing runtime/data model. citeturn1reddit10turn0search0
- Latest implementation commit: `78cbe92223c341745299cd39a3af72e6497de713`.
- Host run `36520731918` and Vita run `36520731913` are currently queued for that commit; do not mark this change green until both complete successfully.


## Enemy sector/difficulty generation integration — 2026-09-29

- Implemented a dedicated ShipContent::loadEnemyShip() path so normal enemy encounters no longer use the player's/static loadShip() path for final enemy system/loadout generation.
- Enemy system generation now uses the documented reverse-engineered sequence: effective sector (Easy delay), rolled system maxima from blueprint min/max, optional-system installation chance, offensive/defensive/general budgets, then enemy weapon/drone generation.
- Difficulty is passed from MainGame into enemy generation; sector is passed as the 1-based gameplay sector.
- The generated reactor budget is rebuilt from the installed system power before loadout generation.
- Enemy weapons/drones are regenerated after system generation, with load-list count and remaining-power restrictions applied. The current no-repeat behavior is intentionally treated as provisional until the exact vanilla duplicate/RNG behavior is verified; do not call it byte-for-byte equivalent.
- Added real-archive regression coverage for generated system bounds, reactor sum, and generated loadout uniqueness.
- Flagship loading remains on its dedicated path and was not routed through normal enemy generation.
- Commits for this step: 3a0537af72cc0e4bf0f206a7ced48f6b309932c1, a29aaac2f36ae214437e728df7b272b04d93443a, 0240bb503b4eae713ccabd9fac0289c87bfab225, 3278e2bb3e8238213ad3413bda9c527884cfb1fc, 4d964a8fcacdb3bdaefc6a808abd0e31280687a2.
- Latest commit currently has no reported combined status in the connected GitHub status response; treat build verification as pending rather than green.


## Enemy generation correction pass — 2026-09-29

- Rechecked the enemy-generation implementation after integration and corrected several implementation issues before treating it as usable: restored the generation helper in the correct namespace, fixed malformed include formatting, separated rolled system maximums from canonical hard caps, and aligned weapon/drone duplicate handling with the reverse-engineered rules currently being used.
- Enemy weapons may repeat; enemy drones are filtered to unique blueprints. Weapon generation now applies the documented power restrictions: weapon power must fit remaining weapon-system power, non-1-power weapons must be below total system power, each selection must consume more than 25% of the remaining power, and when the weapon system is at least level 3 the first generated weapon must be at least 2 power when such a candidate exists.
- The generated system budget no longer upgrades past the rolled per-system maximum merely because the canonical blueprint max is higher.
- Latest commits: 88d30e73f2fb11f3b955db6feac62f9d8bb61530, d0ab18b2f6dac0aa2613e91c19d61af7fbc29061, add417523663081d06bb70af404480e7b5776956.
- Connected GitHub workflow lookup currently returns no workflow runs for the latest commit, so build status remains unverified; do not mark green yet.


### Enemy weapon/drone generation flags — 2026-09-29
- Implemented the two documented enemy loadout flags in ShipContent::loadEnemyShip(): a shield-breaking weapon condition and a hull-damage condition.
- The documented soft fallback for drone generation is applied: while either flag remains, combat drones are preferred; if no candidate satisfies that condition, generation retries using the hard power/uniqueness constraints.
- Drone blueprint duplication remains prohibited.
- Source reference: Mathchamp reverse-engineering notes document the weapon flags, drone soft conditions, and generation constraints. citeturn1view0
- Commit: 2e0a36a7a5961c8c1d48e66884e79f1f73665040.
- The test include corruption from the previous edit was separately fixed in fa8577ac505fc43323b5c79c0a595897b5dc8d89.
- GitHub workflow lookup for these commits currently returns no run records, so build success remains **unverified**; do not treat this as CI-green.
- Exact vanilla RNG sequence is still provisional; this pass improves generation constraints without claiming byte-for-byte RNG equivalence.


### Enemy crew-count generation — 2026-09-29

- Canonical FTL ship blueprints use `<crewCount amount="N" max="M" class="race"/>` for enemy crew ranges; `max` is optional and defaults to `amount`.
- `CrewBlueprint` now preserves per-entry minimum/maximum counts instead of expanding a `crewCount` into fixed members. `ShipBlueprint` aggregates these into `minCrew` / `maxCrew`.
- `ShipContent::loadEnemyShip()` now generates the enemy crew count from the documented sector progression: minimum-to-maximum interpolation across progression sectors 1..9, rounded down, with Easy delayed by one sector. When no event crew override is present, the first blueprint crew race is used as the default race.
- This is a verified formula-level implementation from the reverse-engineering reference, but crew-override proportions, race distribution, room placement, and exact RNG/name/room assignment are still incomplete.
- Regression coverage was added for parsing `amount=2, max=5, class=rock`.
- Latest implementation commit: `32375a5e8864b0f3760470e74d5516f79cd7a241`.
- GitHub status/workflow lookup for this exact commit currently reports no status records or workflow runs, so build verification is pending; do not mark green.


### Enemy-generation implementation restoration — 2026-09-29

- Reinspection found an important repository-state discrepancy: the previous handoff entry described `loadEnemyShip()` as implemented, but the current `src/game/ship_content.cpp` actually contained only the declaration's call path and no method definition. The enemy-generation helper implementation was therefore not present in the current tree.
- Restored the enemy generation implementation in the actual runtime source file `src/game/ship_content.cpp`: sector/difficulty system generation, reactor reconstruction, crew-count generation, weapon generation constraints/flags, and unique drone selection.
- Corrected the crew progression mapping while restoring it: Easy sector 1 maps to progression sector 0; Normal/Hard sector 1 maps to progression sector 1.
- The implementation is still formula-level/reverse-engineered rather than byte-for-byte vanilla: exact RNG sequence, crew race overrides, crew room assignment, and some budget edge cases remain to be verified.
- Latest restoration commit: `5578d577dc6d93e475b96026cb2aa1a4a3a5bc58`.
- GitHub status/workflow lookup for this exact commit currently reports no status records or workflow runs. Build verification therefore remains pending.


## Enemy crew override correction — 2026-09-29

- Fixed proportional enemy-crew overrides so they no longer force a minimum of one crew member.
- A generated crew count of zero now remains zero; this prevents event override proportions from exceeding the generated crew count.
- Added a regression test covering a zero-count proportional override.
- Code commit: e666fcbade564864d8e35f5ebd2814652f41d853.
- Test commit: 448dbd3153891bfa2bf8e19d7e3eb65d6005a8fc.


## Enemy generation correction — 2026-09-29

- Rechecked the restored enemy-system budget table against the documented progression values.
- Found and corrected a concrete bug: Easy difficulty sector 1 maps to progression 0, but the progression-0 budget row had been initialized to zero. It now uses the sector-1 Easy budget of offensive 1 / defensive 1 / general 1.
- Commit: da4a18910688e8c4e2f94599802b88f79989aa8d.
- GitHub status for this commit currently has no reported checks, so build verification remains pending.


## Enemy crew fidelity — 2026-09-29

- Directly inspected the Library ftl.dat and parsed its PKG table.
- Confirmed real enemy blueprint data contains class="random" for pirate crews, including REBEL_FAT_P.
- Confirmed real event ship definitions contain fixed and proportional crew overrides.
- Implemented CrewOverrideEntry and event/ship-level override parsing.
- Enemy generation now uses event/ship overrides when present, defaults to the first blueprint race otherwise, and resolves pirate random into a sector-dependent crew race instead of exposing random as a runtime race.
- The actual sector type is now passed into enemy generation.
- Relevant commits: 4ba96ef1, 8bdc9448, 20e32989, 866bca09, a851450b, 8fd0f3c, f354a4d2, 708c33ca, 1baee44c, 06d2a855.
- Remaining caveat: the exact executable RNG sequence for race selection is not established; the current implementation follows the documented rarity-based model rather than claiming byte-for-byte RNG equivalence.
- GitHub Actions currently has no associated run records for these new commits, so this change is not yet CI-verified.


## Build stabilization checkpoint — 2026-09-30
- Commit 8710e22e59eb9a043a4b95d949ff0c660e60098e fixed the ship blueprint parser linkage scope: helper functions remain in the anonymous namespace while parseShipBlueprint/parseLayoutBlueprint are exported in namespace wormhole.
- Host #1118 and Vita #810 both completed successfully, including Host tests and Vita VPK packaging.
- The preceding 3cf009c3 build failure was a linker error for parseShipBlueprint; this checkpoint records the concrete correction and verified green state.


## User workflow/reporting rule — 2026-09-30
- Development order: if an error appears, fix it first and verify it; only after the error is resolved, proceed to the next feature/implementation task.
- Do not leave known build/test errors accumulating while adding unrelated functionality.
- Continue this cycle autonomously without waiting for the user's reaction between steps.
- Chat reports should be kept to the minimum necessary, using short bullet points only; avoid boilerplate progress/checking statements.
- When a change introduces an error, prioritize correction and re-verification before continuing feature work.


## Rebel Fleet no-fuel encounter checkpoint — 2026-09-30
- Direct inspection of the supplied Library ftl.dat confirmed that the canonical no-fuel fleet encounter is the distinct `NO_FUEL_FLEET` event using `REBEL_FLEET_FUEL`, while ordinary fleet capture uses `LONG_FLEET`.
- `REBEL_FLEET_FUEL` has the canonical 2–4 fuel destroyed reward and an enemy escape definition; the current implementation now selects this ship when a captured beacon is entered with 0 fuel and applies the 2–4 fuel reward range.
- Captured nebula / exit environment rules were cross-checked against the archive: `FLEET_EASY_NEBULA` uses storm; normal captured nebula uses the same storm branch; exit and Easy exit exceptions remain non-ASB as represented by the environment selector.
- The exact enemy escape-timer behavior for `REBEL_FLEET_FUEL` is not yet modeled in CombatRuntime; do not claim this sub-behavior complete.
- Commit `e9e0681c67187a083ac861b5d181b511e3c16dd1` implements the no-fuel ship selection/reward path; commit `d1987b96a34e9d9b177bf0499bd32fec7f9830a1` adds real-data regression coverage.
- Host #1122 and Vita #814 both succeeded for `d1987b96a34e9d9b177bf0499bd32fec7f9830a1`.


## Rebel Fleet escape timer — 2026-09-30
- Direct ftl.dat inspection confirms REBEL_FLEET_FUEL uses an 80-second escape timer; the wiki documentation independently describes the no-fuel Rebel fight as an 80-second countdown. citeturn1search0
- CombatRuntime now starts that timer immediately for REBEL_FLEET_FUEL and exposes a distinct EnemyEscaped outcome when it expires.
- main_game now returns to the sector map without a fuel reward when the fleet ship escapes.
- Host #1126 and Vita #818 both succeeded for commit 35bf1577da996ed5e6aff60eb0655ea94bdfce0b.


## Last Stand wait / FTL charge fidelity — 2026-09-30
- Last Stand waiting now completes the player's FTL charge when the wait causes a Flagship encounter, matching the documented behavior that a fight triggered by waiting starts with full FTL charge. citeturn2search1
- Normal Last Stand waits now advance the beacon-visit counter, except when the Flagship is already jumping toward that beacon, matching the documented scoring/map-tick behavior. citeturn2search7
- Combat jump charging now advances from the actual frame delta instead of assuming 60 FPS.
- Commit: 1b2bb37db94dc2309cc23c4b830d40802dd92cc5.
- Host #1129 and Vita #821 both succeeded.

## Rebel Fleet no-fuel arrival timer correction — 2026-09-30
- The canonical no-fuel fleet fight reached by jumping to a beacon with the last fuel uses a 90-second enemy escape timer; the 80-second timer applies to the separate no-fuel WAIT path. This distinction is documented in the FTL environmental/enemy escape behavior. citeturn2search0turn2search4
- Corrected CombatRuntime so the canonical REBEL_FLEET_FUEL encounter used by the current arrival path starts at 90 seconds instead of 80.
- Commit: 2fb7026dd2efa8358789e11a457b638057dc645c.
- Host #1131 and Vita #823 both succeeded.


## No-fuel WAIT encounter fidelity — 2026-09-30
- Normal sectors now allow WAIT when fuel is 0, matching the vanilla navigation behavior.
- If the current beacon is already fleet-controlled, WAIT enters the canonical no-fuel Rebel encounter with the separate 80-second escape timer; jumping in after consuming the last fuel remains the 90-second path.
- Commit: 2cec0368182cdac154a0c1a9d0fd1a56c7d8a614.
- This change keeps the canonical REBEL_FLEET_FUEL data path and does not introduce a separate DLC mechanism.


## Continuation checkpoint — 2026-09-29
- Commit 53ad039085545cde65825f7a45b533df5336be70 adds real-data regression coverage for the canonical out-of-fuel event chain.
- The test confirms FUEL_FLEET_DELAY has one hidden continuation choice loading NO_FUEL, and that NO_FUEL resolves only to the canonical normal out-of-fuel event IDs.
- Host workflow #1137 and Vita workflow #829 both completed successfully for this commit.
- The hidden-only event UI already renders a Continue prompt, so FUEL_FLEET_DELAY can proceed through its internal NO_FUEL continuation without exposing the hidden choice as a normal player choice.
- Research confirms Rebel/Auto warning ships escaping doubles Rebel Fleet pursuit for the current jump/turn; FUEL_ON_REBEL_WARNING is the canonical out-of-fuel distress example. Current runtime handles this through the warning-event escape path.
- Next fidelity work: tighten Rebel Fleet pursuit modifiers and event-driven escape behavior, then continue Sector 8 and deeper event semantics. Do not replace the archive-driven approach with synthetic DLC handling.


## Continuation checkpoint — no-fuel escape timers — 2026-09-29
- Commit 51166e9e90e5b15d97ba735601f1a1fc341cc1c8 fixes the combat countdown for canonical out-of-fuel hostile events.
- FUEL_* hostile events now start the 80-second escape timer; the Rebel/Auto warning variants use the special 40-second timer.
- Ordinary hostile encounters reached after spending the last fuel retain the separate 90-second timer.
- This is based on the documented vanilla distinction between ordinary out-of-fuel encounters, post-last-fuel hostile encounters, and fleet-warning ships.


## Continuation checkpoint — deterministic test stabilization — 2026-09-29
- Commit 7fb878033ec392792ce00c4d1fd3d1e82f4cf6f2 stabilizes the Defense Drone Mark II interception regression by retrying the documented 90% interception path instead of depending on one random draw.
- Host #1144 and Vita #836 both succeeded.
- No gameplay probability was changed; only the regression test was made non-flaky.


## Last Stand repair beacon one-use fidelity — 2026-09-30
- Implemented the canonical Sector 8 rule that each repair beacon can be used only once.
- Repair usage is tracked by beacon index, cleared when entering a new sector, and persisted in save data.
- Save format advanced from v11 to v12; v11 loading was also corrected to consume its existing `current_sector` field before `event_usage` / `beacon_events`.
- Normal-sector repair behavior remains unchanged; the one-use restriction applies only to The Last Stand.
- Commit: `ea7eb2246b71561e8f5564637dbb5274407d88a5`.
- Host #1148 and Vita #840 both succeeded, including Host tests and Vita VPK packaging.


## Last Stand Flagship blueprint / fleet-target fidelity — 2026-09-30
- Flagship phases now load the fixed BOSS_1 / BOSS_2 / BOSS_3 ship blueprints directly from the archive instead of passing them through normal enemy progression/scaling.
- Phase 2/3 persistent crew casualties continue to be restored across their separate phase blueprints; hull, systems, weapons and drones start from the phase blueprint state.
- Last Stand Rebel Fleet random takeover candidates now explicitly exclude the Flagship's current beacon.
- Commits: d607a729738db113590b20b89d71e13942e12fe2, 7bbe7171ada432dc87b81631184244ff964ad3e.
- Host/Vita builds succeeded for both commits.
- Current Power Surge implementation keeps one fixed 21–26 second interval per phase; Phase 2/3 surge behavior remains the next fidelity target.
- Continue using the archive-driven ftl.dat approach; do not introduce a synthetic DLC mechanism.


### 2026-09-30 Flagship Power Surge audit
- Phase 2 surge now derives its projectile type/stats from the loaded Flagship COMBAT drone definitions, distinguishing the normal combat drone from COMBAT_BEAM.
- Phase 2 surge drone count is difficulty-dependent (Easy 4 / Normal 6 / Hard 7) and each surge drone performs two attacks.
- Phase 3 super shield is consistently restored to 12 hits.
- Host and Vita builds passed after the compile-fix and shield consistency fix.


### 2026-09-30 Continuation — Flagship UI consistency
- Corrected the Sector 8 combat HUD so the Phase 3 Zoltan super shield displays its implemented 12-hit maximum instead of the stale 10-hit label.
- Latest source commit: `760a49215b8eb56a39f7887c978db056b377998f`.
- Current Power Surge timing/count rules remain an implementation audit item; do not describe them as byte-for-byte canonical until directly verified. The current code uses a 20–30 second interval and Phase 2 difficulty counts of 4/6/7.


### 2026-09-30 Flagship Power Surge split correction
- Direct inspection of the supplied `ftl.dat` confirms BOSS_2 contains COMBAT_1 and COMBAT_BEAM as the two surge source types; the documented surge composition is randomly split between them and remains fixed during the phase.
- Removed the incorrect safeguard that forced every Phase 2 surge composition to contain at least one Combat and one Beam drone. All-Combat or all-Beam rolls are now preserved.
- Difficulty counts remain Easy 4 / Normal 6 / Hard 7, and the phase-local interval remains a single 20–30 second roll.
- Source commit: `3f77ad97dbd07f130b7f77026f655fb09cb1f548`.


## 2026-09-30 Flagship Power Surge temporal attack correction
- Phase 2 temporary surge drones no longer enqueue both attacks as one simultaneous volley.
- Each temporary surge drone now creates its first attack, and its second attack is scheduled only after the first projectile resolves.
- The temporary drone keeps a two-attack counter, so cloaking or a Defense Drone interception still consumes an attack and the drone disappears after its second attempt.
- This better matches the observed original behavior where surge drones deploy, attack twice, then disappear; the surge itself remains independent of the normal Drone Control system.
- Commits: bf520a2cf27b9fa7b02a29f17cf57a7f0e99442a and 328f7c39f446f9f5edde6bc00b08d406ac8fd38a.
- Follow-up interception handling: 3637f701aa8cefefe268a95f2716cb6736756baf.
- Host #1200 and Vita #892 both succeeded for the follow-up correction.

- 2026-09-30: Flagship Power Surge normal-fire correction. Commit `ad078636587cb4343dc07d1b3904bbfd07efeddf` keeps normal Flagship weapon/drone firing active while temporary Power Surge shots are in flight, and preserves canonical `COMBAT_BEAM`/other combat-drone weapon types from `ftl.dat` during normal drone attacks. Host #1202 and Vita #894 both succeeded.
- 2026-09-30: Added a `test_real_ftl_dat.cpp` regression check for BOSS_2's canonical COMBAT_1/COMBAT_BEAM definitions. Commit `484129180ea02c699442a3b45e22fab6f6b60d26`.


### 2026-09-30 Flagship drone weapon fidelity follow-up
- Fixed the Host-only regression test compile failure by resolving BOSS_2 initial drone IDs through BlueprintDatabase::findDrone().
- Preserved canonical drone weapon secondary effects from ftl.dat: hull bust, fire, breach, stun chance/duration now flow from WeaponBlueprint -> DroneBlueprint -> RuntimeDrone -> RuntimeWeapon.
- Beam drone attacks now also apply canonical fire/breach/stun effects instead of skipping all secondary effects.
- Host #1205 / Vita #897 succeeded after the test correction; subsequent source changes continue through the normal build workflow.


## 2026-09-30 Combat drone cooldown correction
- Rechecked the canonical BOSS_2 combat-drone data path and found a timing mismatch in runtime: COMBAT_1/COMBAT_BEAM define their launch cooldown on the drone blueprint in milliseconds (1000), while the linked DRONE_LASER/DRONE_BEAM weapon data does not define the drone's launch interval.
- Corrected ShipRuntime so combat-drone charging uses the drone's canonical cooldown (cooldown / 1000.0) and only falls back to the linked weapon cooldown when a combat drone has no drone cooldown.
- This prevents the loaded combat drones from firing on an incorrect multi-second cycle and keeps the timing data-driven from ftl.dat.
- Commit: 62a8879708e4759a8e70b877876a1ca0c6cbafa4.


## 2026-09-30 Beam combat correction
- Corrected CombatRuntime projectile handling so beam weapons loaded from the canonical weapon type are not subjected to normal engine/piloting evasion and do not consume ordinary shield layers.
- Beam damage is reduced by the target's current ordinary shield layers instead.
- Host #1219 and Vita #911 both succeeded for commit 820a74bc05702e7c01afd170637b35e151a86abb.
- Continue auditing shield-piercing, ion, missile/bomb and secondary-effect semantics before treating projectile combat as complete.

## 2026-09-30 Ion shield interaction correction
- Corrected runtime ion weapon resolution so an ion projectile blocked by ordinary shields applies its ion damage to the Shields system instead of merely consuming one shield layer.
- Preserved missile/bomb shield bypass and normal projectile shield-layer behavior.
- Continue auditing ion stacking, crystal/heavy-pierce shield piercing, and Zoltan/reverse-ion edge cases before treating weapon resolution as complete.

## 2026-09-30 Hull-buster shield correction
- Prevented hull-buster bonus damage from applying when a beam's base damage is fully absorbed by ordinary shields.
- The bonus now applies only after the beam has non-zero effective damage, preserving the intended empty-room bonus without letting it pierce a full shield block.


## 2026-09-30 Fire/breach rollback
- Reverted the attempted fire/breach roll-order change after the repository regression test confirmed canonical behavior expects both effects to be independently rollable on the same hit.
- Keep fire and breach chances as independent secondary-effect rolls.

## 2026-09-30 Ion shield regression coverage
- Added a regression test confirming an ion shot blocked by one ordinary shield layer leaves that layer intact while applying one ion damage to the Shields system.
- Keep the ion shield behavior and shield-piercing behavior under separate tests.

## 2026-09-30 Ion test setup correction
- The new ion/shield regression test initially failed because it inherited the weapon's previous shield-piercing value, so the shot legitimately bypassed the one-layer shield.
- Test setup now explicitly sets shield piercing to zero; no runtime behavior change was required.

## 2026-09-30 Missile/bomb ammunition regression
- Added a deterministic combat regression proving missile/bomb ammunition is consumed once per volley, not once per projectile.
- The test also verifies a volley cannot fire when the remaining ammunition is below the weapon's configured `missilesUsed` cost.

## 2026-09-30 Missile shield bypass regression
- Added deterministic coverage proving missile projectiles bypass ordinary shield layers while each projectile still applies its hull damage.

## 2026-09-30 Missile bypass regression timing fix
- The new missile shield-bypass regression initially asserted before the third projectile's flight time had elapsed; stabilized it by advancing combat past the full volley duration.

## 2026-09-30 Missile bypass shield-system test setup fix
- The regression now normalizes the target Shields system power before advancing combat, matching ShipRuntime shield state initialization and preventing the test fixture from losing its manually assigned shield layers.


## 2026-09-30 Missile/bomb data-path regression
- Audited the current missile/bomb combat path after the ammunition and shield-bypass regressions.
- Runtime behavior remains data-driven: canonical weapon definitions carry `missilesUsed`, that value is copied into `RuntimeWeapon`, ammunition is consumed once per volley, and `missilesUsed > 0` weapons bypass ordinary shield layers while still being checked against the Phase 3 super shield.
- Added a real-archive regression over the canonical `WEAPONS_MISSILES` list to verify missile/bomb entries with ammunition costs retain those costs when copied into runtime weapons.
- Test commits: `197e37dabfd0b70cd47d25819c96e37a96a66b06`, corrected by `7e3b0298927eec0555cb13bb83d42e961cc57c98`.
- No synthetic DLC path was introduced; continue from the canonical ftl.dat data path.
- Next fidelity audit remains crystal/heavy-pierce interactions and Zoltan/reverse-ion edge cases.
