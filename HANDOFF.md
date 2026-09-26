# FTL for PS Vita — Development Handoff

> This file is the persistent handoff/state document for continuing the port across chat sessions.
> Update it whenever a major subsystem, build state, decision, or blocker changes.

## Repository

- GitHub: https://github.com/teststs1111/FTL-for-VITA
- Main branch: `main`
- Project: PS Vita port work for Project Wormhole / Tachyon
- Target: a Vita-native playable FTL-style runtime reconstructed from the open-source Tachyon implementation
- Proprietary FTL source/assets are **not** stored in this repository.

## Current state — 2026-09-26

### Latest commits

- `4ece88fd42df2b6cd4c2ff576b4360ca172a1587` — `Treat native Vita resolution as successful vitaGL init`
- `645dade5ca9d0fe1d41654c3c41ea5f200451f98` — `Build vitaGL without splashscreen for Vita startup stability`
- `5b454234140da23b66120a6efd9972b99a77b7aa` — `Allow returning from beacon map to ship management`
- `2c23d522b7e9ce613c44f31a8372f486c5722c1e` — `Move gameplay flow closer to FTL beacon and pause progression`
- `fe3d37b16a2e35c6f4e235f15d92c9bdf106160c` — `Set Vita application title to FTL: Faster Than Light`

### CI state

- Latest Vita build for gameplay-flow changes: **in progress** (run #99)
- Latest Host build for gameplay-flow changes: **in progress** (run #407)
- Previous known-good Vita/Host builds remain successful; the new run must be green before calling this gameplay-flow change validated.
- Vita VPK artifact is available and not expired.
- Artifact size: 1,056,989 bytes
- Artifact SHA-256: `0693d0783a06d63bbc9c7cb71bbbe7f7d67120aa7150d4fe92a5756ffd9ea407`

### Startup/crash milestone

The previous crash dump showed a `vitaGL Splashscreen` thread with a GXM data-abort around the vitaGL startup path. The Vita workflow now builds a fresh vitaGL with `HAVE_SBRK=1 NO_SPLASHSCREEN=1`.

The renderer also no longer treats `vglInitExtended(960,544,...)` returning `GL_FALSE` as failure. In current vitaGL, native 960x544 initialization can return `GL_FALSE` because the return value indicates resolution fallback rather than generic initialization success.

This removes two identified startup hazards, but **real-device runtime success is not yet verified**.

### Vita application metadata

The VPK application name is now:
`FTL: Faster Than Light`

## What is already implemented

### Build / platform
- C++17 project migration prototype
- Host CMake build
- Host regression tests
- GitHub Actions host build/test
- VitaSDK toolchain detection
- Vita SELF/VPK packaging configuration
- vitaGL 960x544 bring-up renderer
- Vita controller polling
- Start+Select exit path for hardware bring-up

### FTL data/archive
- Tachyon reconstructed `PKG\\n` archive support
- Vanilla FTL `.dat` archive support
- Vanilla archive format uses a little-endian slot/offset table followed by per-file size, filename length, UTF-8 filename and raw data
- BXML reader:
  - document-local string table
  - 7-bit unsigned varints
  - element names
  - attributes
  - text nodes
  - child elements
  - end-of-element markers
- Regression coverage exists for both archive paths and BXML.

### Runtime systems
The C++ prototype already contains substantial runtime scaffolding for:
- ship runtime
- crew
- systems
- shields
- weapons
- drones
- combat
- projectiles
- enemy fire delay handling
- defense-drone interception behavior

This is still a reconstruction/prototype and is not the complete FTL game.

### Localization
- `include/i18n/localization.hpp`
- `src/i18n/localization.cpp`
- Japanese is the default locale.
- English is available as a development fallback.
- Stable translation keys are used instead of hard-coded UI strings.
- Core FTL UI/combat/resource terms are covered.
- Localization regression tests exist.
- Actual Vita Japanese glyph rendering is still a UI-layer task.

## Real FTL asset policy

A user-owned legitimate `ftl.dat` may be supplied locally for technical validation.

**Never commit or redistribute the proprietary FTL `ftl.dat` or extracted proprietary game assets in this repository.**

Expected paths:
- Vita: `ux0:data/wormhole/ftl.dat`
- Host: `FTL_DAT_PATH=/path/to/your/ftl.dat`
- Host default: `./ftl.dat`

The repository should contain only the code needed to read/use user-supplied assets.

## Localization strategy

FTL itself has official Japanese support in modern versions. The port should prefer the original user-supplied Japanese resources where technically possible rather than replacing the entire game translation with a manually maintained dictionary.

The built-in localization catalog is intended for:
1. port-specific UI that does not exist in original FTL resources;
2. temporary development labels;
3. fallback behavior.

Do not treat the small built-in catalog as the final Japanese translation.

## Porting order

Work in dependency order:

1. **CI/build stability**
2. **Real user-supplied `ftl.dat` validation**
3. **PNG/texture loading from `ftl.dat`**
4. **Blueprint/database integration**
5. **Ship rooms, doors and systems**
6. **Crew/weapons/projectiles/combat integration**
7. **Sector/beacon/event flow**
8. **FTL UI + Japanese text rendering + touch controls**
9. **Audio**
10. **Save/load**
11. **Mod support**

Do not jump to polish while a lower-level dependency is broken.

## Immediate next actions

### 1. Continue rendering/data integration
The PNG/texture pipeline is now covered through PNG decoding, AssetStore byte caching, deterministic file enumeration, and a regression test that reads PNG bytes from a synthetic archive through AssetStore. The live renderer now prefers the exact ship artwork stem declared by the FTL blueprint (img=), instead of choosing an arbitrary ship PNG.

### 2. Real-data validation completed for the supplied archive
The user supplied a local `ftl.dat` for validation. It is **not committed** to GitHub.
- Archive header: reconstructed `PKG\\n` container, 3,219 entries
- Archive size: 280,573,482 bytes
- PNG resources: 2,837
- Japanese text resource: `data/text-ja.xml` (UTF-8, about 920 KB)
- Player blueprint: `PLAYER_SHIP_HARD`, `layout="kestral"`, `img="kestral"`
- Exact player hull artwork: `img/ship/kestral_base.png`
- The current renderer now resolves that artwork from the blueprint metadata.

Next real-data work:
- validate BXML parsing against selected real XML resources
- connect the original Japanese resource into the localization layer
- inspect room/interior/weapon/projectile artwork paths
- establish background and HUD asset selection without embedding the archive

### 3. Rendering
After data validation:
- load PNGs from the archive
- connect texture cache to the live ShipScene
- use AssetStore::fileNames() against a user-supplied archive to identify real ship/background PNG paths
- establish a real ship/background scene
- add Vita Japanese text rendering using the Vita font/PVF facilities
- then build the FTL HUD/menu layer

### Gameplay fidelity target

The project should converge toward the actual FTL loop and interaction model, not merely reproduce the look of the ship screen. Prioritize:
1. sector generation and connected beacon navigation;
2. event / store / distress / empty / hostile beacon outcomes;
3. jump fuel consumption and sector progression;
4. combat encounter entry/exit and rewards;
5. ship management, power allocation, crew movement, doors, fires, breaches and oxygen;
6. real FTL HUD, targeting, weapon charge and pause behavior;
7. sector 8 flagship sequence and final escape;
8. original Japanese text resources and Vita Japanese glyph rendering;
9. audio and save/load;
10. expansion/mod dataset selection.

Do not add convenience/debug controls that bypass the real gameplay loop unless they are behind a development-only build flag.

## Important implementation constraints

- Keep desktop-only Swing/GLFW tooling out of the first Vita runtime.
- Keep host tests deterministic.
- Prefer small, testable C++ slices over large rewrites.
- Preserve existing working behavior when adding new systems.
- Do not embed proprietary FTL assets.
- When a real FTL asset is required for testing, use a local/user-supplied copy.
- Every meaningful change should have a clear commit message.
- Update this handoff document when the project reaches a new major milestone or hits a new blocker.

## Progress tracking

Use subsystem progress rather than inventing a single overall percentage.

Suggested tracking:

| Area | State |
|---|---|
| Host build/test | Green — run #303 |
| Archive reader | Implemented; real `ftl.dat` validation pending |
| BXML | Implemented/tested |
| PNG/texture pipeline | Implemented/tested; live blueprint-driven ship asset hookup is green |
| Blueprint/data integration | Prototype implemented |
| Ship/runtime simulation | Prototype implemented |
| Combat | Prototype implemented; regression tests active |
| Vita renderer | Bring-up stage; gameplay-flow UI layered on top |
| Japanese localization | Foundation implemented |
| Japanese font rendering | Pending |
| FTL UI/touch | Pending |
| Sector/event flow | 🟡 Prototype beacon flow added; real sector/event data pending |
| Audio | Pending |
| Save/load | Pending |
| Mod support | Pending |

## Chat continuation protocol

When a new chat starts with this repository URL, read this file first, then:

1. inspect the latest commit;
2. inspect current CI status;
3. inspect any active blocker;
4. continue from **Immediate next actions**;
5. update this file after major changes.

This file is the durable project memory. Chat history is supplementary.


## 2026-09-26 continuation: real ftl.dat / Advanced Edition investigation

- A user-supplied legitimate `ftl.dat` was made available for direct technical inspection. It must remain external and must never be committed or redistributed.
- Confirmed real archive characteristics from the supplied file:
  - reconstructed `PKG\\n` archive
  - 3,219 entries
  - 280,573,482 bytes
  - 2,837 PNG resources
  - `data/text-ja.xml` is present (UTF-8, about 920 KB)
- Important AE finding: AE data is contained inside the same `ftl.dat`; there is no need for an external `.dlc` archive for the normal game configuration.
- The archive contains AE-related resources including:
  - `data/dlcBlueprints.xml`
  - `data/dlcBlueprintsOverwrite.xml`
  - `data/dlcPirateBlueprints.xml`
  - `data/dlcEvents.xml`
  - `data/dlcEventsOverwrite.xml`
  - Therefore the target behavior is: open one `ftl.dat`, then enable/disable the AE dataset/layers inside that archive. Do **not** revert to the previous external `.dlc` approach.
- A first implementation pass was made to let blueprint/event loading include those internal AE resources when AE is enabled, including overwrite event-list handling. This pass currently fails Host build #557 and Vita build #249 and is **not validated** yet. Fix the build before treating the AE integration as complete.
- A further data-fidelity issue was identified: real event data uses forms such as `<event load="...">` extensively, while the current parser primarily assumes `<event name="...">`. This must be corrected so event resolution matches the real FTL data model.
- Next priorities:
  1. Fix Host/Vita build failures from the AE-layer pass.
  2. Correct event/eventList parsing for real `load`-based event references.
  3. Implement genuine AE OFF filtering and AE ON layering from the single archive.
  4. Apply blueprint overwrite semantics consistently (base -> AE additions -> AE overwrite).
  5. Validate sector/event selection against the supplied real data.
  6. Continue wiring original `data/text-ja.xml` into the runtime localization path.
- Current user requirement: continue autonomously where possible; ask only when an external input is genuinely required.

## 2026-09-26 continuation: build blocker after AE-layer pass

- Namespace closure causing the large `main_game.cpp` compiler cascade was fixed in commit `3523041c3492f98c2edc1db36cdda9e810c91ea8`.
- Real `eventList` `<event load="...">` references are now parsed in commit `23eaa057c4d06c598af5d6cf7a287d1888c6c93f`, while retaining support for named inline event entries.
- Host #560 and Vita #252 are queued for the latest event-parser changes; these must be checked before declaring the parser fix green.


- Host build #557: **failed during Build step**.
- Vita build #249: **failed** after the same commit.
- The failure has not been marked as solved. The next session must inspect/fix the compile error before adding more AE behavior.
- Do not claim the AE-layer implementation is green until both Host and Vita workflows succeed.

## 2026-09-26 continuation: original sector/event data path
- Added `EventDatabase` to scan the loaded FTL archive for `data/events*.xml` and expose named events/eventLists.
- Added `SectorDatabase` to parse `data/sector_data.xml` (and AE sidecar when present), including sector descriptions, minimum sector, start event, and beacon event pools.
- Beacon selection now prefers the original sectorDescription event pool instead of always forcing combat. This is the first step toward the actual FTL beacon -> event -> choice -> combat/store/reward loop.
- The current five-node map geometry remains a temporary stand-in; the next major gameplay task is the real connected/procedural beacon graph and Rebel fleet pressure.
- Event parsing currently implements the common event/choice/load/hostile/store/repair/item_modify paths. Complex nested requirements, blue options, quests, multi-stage rewards, and full combat encounter resolution still need to be wired.
- AE/mod layering must not assume an external `.dlc` file for the normal AE configuration. The supplied real `ftl.dat` contains the AE resources internally; use internal dataset selection/layering.


## 2026-09-26 continuation: connected beacon graph
- Added a deterministic seeded multi-lane sector graph (8 rows × 3 lanes) with branching/converging links.
- Normal navigation now consumes fuel when jumping to a connected beacon; combat no longer consumes a second fuel unit on victory.
- Reaching the final row advances to the next sector; sector 8 final-row completion enters the victory state.
- The graph is still a compatibility/prototype layer; the next fidelity step is to apply original FTL beacon type weighting, Rebel fleet pursuit, and sector-specific graph constraints rather than using the generic 8×3 generator.

- Build log inspection found two concrete compile errors in main_game.cpp: missing auto on scanner label and missing closure of the anonymous namespace before MainGame definitions. Fixed in 4a8019658319fe126b1951dcc76fb148ec155dc3.

- Host #569 reached successful compilation/linking of wormhole_core and tests; final link exposed one missing CombatRuntime::activateCloaking definition. Implemented in e13152613b79be869dccd1ed0acc06a0600443e3. This should be the next build verification point.

- Host #571 exposed a second cloaking compile issue: ShipRuntime has no hasSystem/SystemType API. Replaced it with direct RuntimeSystem type lookup in 7d1647908ce232acc4acd2b2f7fca1dbad6e06ee.

- Host/Vita builds #573/#265 are green on 163bf2c0. AE event overwrite handling was advanced in 618411db: overwrite event definitions can replace existing IDs, and OVERRIDE_ event-list names map back to the base pool ID.


## 2026-09-26 continuation: current real-data validation / CI state

- Latest main commit: `e04d0231e349c3567550a15ace01751ac6a3466d` (`Register optional real ftl.dat smoke test`).
- Latest CI for that commit is green: Host build **#578: success** and Vita build **#270: success**.
- Added `tests/test_real_ftl_dat.cpp` and registered it as an optional CTest real-data smoke test. It skips cleanly when `FTL_DAT_PATH` is absent, so proprietary data is not required in CI.
- The user-supplied `ftl.dat` was directly inspected outside the repository: `PKG\\n` archive, 3,219 entries, 280,573,482 bytes, 2,837 PNG resources, and `data/text-ja.xml`.
- **Correction to older handoff text:** the inspected archive does **not** contain `data/newEvents.xml`. Do not depend on that path. Confirmed AE resources are `data/dlcEvents.xml`, `data/dlcEventsOverwrite.xml`, `data/dlcBlueprints.xml`, `data/dlcBlueprintsOverwrite.xml`, and `data/dlcPirateBlueprints.xml`.
- Runtime startup now loads `data/text-ja.xml` into the localization layer when the archive is opened. EventDatabase, SectorDatabase, and BlueprintDatabase are all archive-backed.
- Event parsing now handles named events plus common `event load="..."` references, weighted pools, AE overwrite pools, common item/resource rewards, hostile/store/repair/quest markers, and basic choice requirements. Complex nested requirements, blue options, quest chains, and complete reward semantics remain incomplete.
- SectorDatabase parses real sector descriptions/event pools, but SectorGraph still uses a deterministic generic 8x3 compatibility graph. Original FTL beacon generation constraints and Rebel fleet pursuit remain.
- Vita startup/rendering is buildable, but real Vita hardware gameplay verification is still outstanding.

### Current subsystem progress estimate

| Area | Progress |
|---|---:|
| Build/toolchain | ~90% |
| Archive/BXML | ~90% |
| Real-data ingestion | ~65% |
| Blueprint/ship data | ~60% |
| Ship/crew/systems/combat simulation | ~45% |
| Sector/beacon/event loop | ~40% |
| Vita renderer/input | ~35% |
| Japanese text/font/UI | ~25% |
| Audio | ~0% |
| Save/load | ~0% |
| Mod support | ~0% |

Overall maturity is roughly **40% toward the stated real-FTL gameplay target**. This is an engineering progress estimate, not a percentage of code or a completion guarantee.

### Next implementation order

1. Replace remaining prototype event classification with real event-choice execution and runtime state changes.
2. Replace the generic SectorGraph with FTL-like beacon generation and Rebel fleet pressure.
3. Wire real ship room/system/weapon/crew data deeper into the playable loop.
4. Complete original Japanese text lookup and Vita Japanese glyph rendering.
5. Continue FTL HUD/touch, audio, save/load, and flagship sequence.


## 2026-09-26 continuation — real FTL event localization
- Latest implementation commits: `23d6560`, `f92da8b`, `1fefbbf`, `67c281e`.
- Event definitions and choices now retain the original FTL `text id=` keys in addition to inline text.
- Vita event rendering resolves those keys through the user-supplied `data/text-ja.xml` localization table, so events using the real FTL text database can display Japanese text instead of exposing raw localization IDs.
- Added regression coverage for event and choice localization keys.
- The latest commit's GitHub Actions status has not surfaced yet; do not treat this as a new green CI result. The last confirmed green baseline remains Host #578 / Vita #270 at `e04d0231`.
- The next fidelity target remains the event effect model: nested rewards/crew/damage/boarder outcomes and complete choice requirements, followed by more faithful sector beacon generation and Rebel Fleet pressure.
- Estimated overall progress remains about 40% toward the real-FTL gameplay target; this is an engineering estimate, not a completion guarantee.


## 2026-09-26 continuation — Library ftl.dat handoff source
- A legitimate user-supplied **ftl.dat is already stored in the ChatGPT Library** and is available as the project's real-data validation source across continuation sessions.
- When real FTL data needs to be inspected, parsed, counted, or validated, **retrieve/use the Library copy of ftl.dat first** rather than asking the user to upload it again.
- The Library copy is for local technical inspection only. **Never commit, embed, extract-and-commit, or redistribute the proprietary archive or its extracted proprietary assets to GitHub.**
- Known inspected Library archive: header is reconstructed PKG\\n; 3,219 entries; 280,573,482 bytes; 2,837 PNG resources; Japanese resource data/text-ja.xml.
- AE resources are inside the same archive: data/dlcBlueprints.xml, data/dlcBlueprintsOverwrite.xml, data/dlcPirateBlueprints.xml, data/dlcEvents.xml, data/dlcEventsOverwrite.xml.
- data/newEvents.xml does not exist in the inspected archive.
- The desired game behavior is a single-archive model matching the real game: base data and AE data are selected/layered internally from the same ftl.dat, not packaged as a separate external DLC archive.
- If a future continuation needs the archive and it is not already mounted in the current runtime, use the Files/Library retrieval path to materialize the existing Library file. Do not ask the user to provide it again unless the Library copy is genuinely unavailable.
- This Library source is especially important for validating event effects, choice requirements, ship/weapon/crew data, sector data, Japanese localization, and asset paths against the real game data.


## 2026-09-26 continuation — event crew runtime integration
- Real event XML was inspected from the Library-supplied `ftl.dat` / extracted `data/events.xml`.
- Confirmed real `crewMember` forms include:
  - `amount`
  - `id`
  - `class`
  - individual skill attributes such as `pilot`, `engines`, `shields`, `weapons`, `repair`, `combat`
  - `all_skills="1"`
- Confirmed `removeCrew` may contain:
  - `class="..."` to target a specific crew race/class
  - child `<clone>true|false</clone>`
  - child `<text id="..."/>` for the outcome text.
- Added structured `EventCrewMemberEffect` and `EventCrewRemovalEffect` data models.
- EventDatabase now parses crew effects both on direct events and on nested `<choice><event>...</event></choice>` outcomes.
- ShipRuntime now supports:
  - adding event-generated crew with an 8-alive-crew cap;
  - reusing dead crew slots;
  - removing a crew member by race/class;
  - generic crew removal;
  - clone-aware removal when a powered, undamaged Clone Bay is present.
- Runtime crew now stores the six basic FTL skill values (pilot/engines/shields/weapons/repair/combat) and `all_skills="1"` is represented as level 2 for those fields.
- MainGame now applies crew additions/removals together with the already-supported event resource and damage effects.
- Dynamic crew is now included in save files. Save format advanced to `FTL_VITA_SAVE 4`; versions 2/3 remain readable.
- Added real-data smoke coverage for `CREW_DEAD_TEST` removal parsing and ShipRuntime add/remove operations.
- No proprietary FTL data was committed.
- Latest implementation commit: `ef312a9ea171b3475543fd167fa371c6d69ac49e` (includes a follow-up fix for dynamic crew save-slot restoration).
- GitHub Actions status for that commit has not surfaced yet (`workflow_runs=[]`, `statuses=[]`); **do not call this change CI-green until a new Host/Vita result is available**.

### Next event-fidelity targets
1. Parse/apply `boarders` into the combat boarding runtime.
2. Parse `autoReward` and its reward tables instead of treating only explicit item modifications.
3. Parse `quest` / quest chains more completely, including event completion semantics.
4. Expand blue-option requirements to actual crew skills and equipment/augment requirements.
5. Continue with `weapon`, `item_modify`, `environment`, `distressBeacon`, and nested combat outcomes.
6. Then replace the generic beacon graph with original FTL beacon generation constraints and Rebel Fleet pursuit.


## 2026-09-26 continuation — real `boarders` event integration
- The Library-supplied real `data/events.xml` was inspected directly. Confirmed `boarders` nodes use `min`, `max`, and `class`; some also use `max_group`.
- Observed real examples include human, ghost, slug, mantis, and random boarder classes.
- Added `EventBoarderEffect` to the event data model and parse it from both direct event nodes and nested choice event outcomes.
- Boarder counts are rolled from the real XML min/max range and capped by `max_group` when present.
- Event boarders are held until the associated combat starts, then injected into the existing `CombatRuntime::boarders` system.
- Existing CombatRuntime already handles boarding movement, open-door pathing, boarding combat, death/removal, and boarder rendering, so this work reuses that runtime instead of creating a second boarding implementation.
- Event boarders are assigned valid player rooms before combat. `class="random"` is resolved to an available enemy crew race for the runtime/texture path.
- This means real event outcomes such as `<boarders min="3" max="5" class="human"/>` now have a path from original XML -> EventDatabase -> MainGame -> CombatRuntime.
- Latest boarder integration commit: `2484415ba271060a07ca8d1daf04d15b3689d93c`.
- No proprietary archive/assets were committed.
- CI has not yet surfaced a new workflow result for this continuation; do not claim green until Host/Vita results appear.

### Next target
1. Parse and apply `autoReward` using the original reward semantics.
2. Improve `quest` chain/state handling.
3. Expand Blue Option requirements to crew skills, augment/equipment and resource conditions.
4. Continue `weapon`, `item_modify`, `environment`, and `distressBeacon` semantics.
5. Replace the compatibility beacon graph with original FTL generation constraints and Rebel Fleet pursuit.


## 2026-09-26 continuation — real `autoReward` integration
- Inspected the original event-file comment in the supplied `data/events.xml`, confirming the auto-reward vocabulary: `standard`, `stuff`, `fuel`, `missiles`, `droneparts`, `fuel_only`, `missiles_only`, `droneparts_only`, `weapon`, `augment`, `drone`, and `item`.
- Added structured `EventAutoReward` data to event definitions and choices.
- Parser now reads `level="LOW|MED|HIGH|RANDOM"` plus the reward type from original `autoReward` nodes.
- Runtime now applies the resource/scrap portions of these rewards using the real FTL reward tier ranges for Normal difficulty and sector progression. Resource ranges use the documented fixed FTL low/medium/high tables.
- `standard` grants tiered scrap plus two distinct resource types; `stuff` grants low scrap plus two tiered resources.
- `fuel`, `missiles`, and `droneparts` grant their resource plus tiered scrap; the *_only variants grant only that resource.
- `weapon`, `augment`, `drone`, and mixed `item` rewards now select actual loaded blueprint entries and add them to the runtime when the corresponding slot is available.
- `RANDOM` tier is resolved deterministically from the existing run seed/state so host tests remain reproducible.
- This is intentionally implemented against the real archive's data model rather than inventing a separate DLC reward system.
- Latest auto-reward implementation commit: `3f026ae50904e4d816747cbcf3ec606d8bd5379c` (follow-up corrected `stuff` to use sector-scaled LOW scrap).
- Web cross-check: FTL reward documentation confirms the tier/resource ranges and autoReward categories used here. citeturn0search0turn0search6
- CI status has not produced a new workflow result for this continuation; do not claim green yet.

### Next target
1. Verify/expand `autoReward` bonus-item probabilities and exact overwrite behavior.
2. Implement fuller `quest` chain state and quest-event targeting.
3. Upgrade Blue Options from race/system presence to actual crew skills and equipment/augment requirements.
4. Parse/apply `environment`, `distressBeacon`, `weapon`, and `item_modify` semantics more completely.
5. Replace the compatibility sector graph with original FTL beacon generation and Rebel fleet pressure.


## 2026-09-26 continuation — real quest target handling
- Inspected all 21 `<quest>` nodes in the supplied base `data/events.xml`.
- The real archive overwhelmingly uses the form `<quest event="TARGET_EVENT" />` without a separate quest name/id.
- Fixed runtime quest registration so the originating concrete event ID becomes the stable quest key when no explicit quest name/id exists.
- Choice-level quest creation now uses the originating event ID for the same reason.
- This makes the existing save/load quest state meaningful for the actual FTL data: active quest -> target event ID -> completion when that target event is reached.
- Existing `completeQuestForEvent()` now removes the matching active quest and target mapping when the target event is entered.
- Latest quest fix commit: `8ee73f12a995756de70a56577f70e456514e9ed2`.
- No proprietary data was committed.


## 2026-09-26 continuation — Blue Option requirements
- Inspected the real base event XML: hidden/blue-style choices use `req` + optional `lvl`, including crew/system requirements such as `pilot`, `engines`, `shields`, `weapons`, `doors`, `medbay`, `teleporter`, `cloaking`, `hacking`, `mind`, `sensors`, and equipment identifiers.
- Runtime choice checks now evaluate crew skill levels for pilot/engines/shields/weapons/repair/combat when `lvl` is present.
- Runtime choice checks also recognize owned augment identifiers through the existing augment inventory.
- Existing system-level and weapon/drone checks remain in place.
- Latest Blue Option commit: `0ed1be757810b4bee729bba9d0e1f778fa5c85ce`.
- No proprietary archive or extracted asset was committed.


## 2026-09-26 continuation — environment, distressBeacon, weapon event effects
- Inspected the real base `data/events.xml`: 7 `environment` nodes, 11 `distressBeacon` nodes, and 7 `weapon` nodes.
- Observed real environment values are `PDS` targeting `player`, `asteroid`, and `sun`.
- Event data now preserves environment type/target and explicit distress-beacon markers on both top-level events and nested choice events.
- Beacon classification now honors the explicit `<distressBeacon/>` tag before relying on event-name heuristics.
- Real event `weapon name="..."` effects are now applied as actual weapon rewards. Named weapons are loaded from the real Blueprint database; `RANDOM` selects a deterministic non-owned weapon when a weapon slot is available.
- Environment data is currently modeled but not yet fully mapped to combat hazard mechanics (PDS/asteroid/sun damage/evasion/oxygen behavior remains a separate fidelity step).
- Latest commits: `2d79d2fd92e03b78ab316faa5154ff10cf306c94`, `14242aa2d3251db508ba76f332780eacb5f208bb`, `1ff00bcfe40b54a2660d803e5243339fab020f2c`, `ba061afd675d77503c14c761af397f3200ae5bac`.
- No proprietary archive or extracted asset was committed.


## 2026-09-26 continuation — environmental hazard runtime
- Real event environment values confirmed from the supplied archive: `PDS target="player"` (3), `asteroid` (3), `sun` (1).
- CombatRuntime now models environment state and applies deterministic periodic hazards:
  - asteroid: removes one shield layer, otherwise deals 1 hull/system damage with small fire/breach chances to both ships;
  - sun: periodic fires on both ships, reduced fire count while shields are present, with room damage chance;
  - PDS: ASB-style periodic 3 damage + breach against the configured target, with player evasion derived from engines/piloting and cloaking.
- EventDefinition/EventChoice environment metadata is now carried into pending combat state and applied when the hostile encounter starts.
- This is a gameplay approximation of the documented vanilla hazard timings/behavior; visual warning/siren/projectile presentation and exact internal ASB/asteroid/sun formulas remain future fidelity work. Environmental hazard behavior references include the FTL Environmental Hazards documentation. 
- Latest implementation commits: `731198e742ca9ceffbb78ef7d4f03965e1e85903`, `af2dec0046b724489f3fc06cff2d5f4cce3ec823`, `576928b91d659c6411a888589daaa2080c9dab67a6`.


## 2026-09-26 continuation — item_modify and inline eventList fidelity
- Re-inspected all 41 real item_modify nodes in the supplied base data/events.xml.
- Confirmed item_modify is not limited to top-level events: the archive also uses it inside nested choice events, directly on choices, and inside inline eventList entries.
- Resource modifiers include both positive rewards and negative costs/trades, with ranged values such as scrap -25..-10, fuel -4..-2, and mixed multi-resource transactions.
- Added a shared parser for scrap/fuel/missiles/drones that preserves signed min/max ranges instead of normalizing negative maxima upward.
- Added support for item_modify directly on <choice> as well as the nested <choice><event>...</event></choice> form.
- Fixed eventList parsing so unnamed inline <event> entries receive stable synthetic IDs and are retained in weighted pools. Previously these entries were silently discarded, which could prevent real event outcomes from ever being selected.
- Added regression coverage for an inline eventList item modifier and a negative choice-level scrap modifier.
- No proprietary FTL data or extracted assets were committed.
- Latest implementation commit: 3fef8f02340bb42fba0de76abe6b69c0555f6ef1.
- Latest regression-fixture commit: 973506103903974803b97f7002d22d1b55fd6fbe.
- GitHub Actions has not surfaced a workflow result for 9735061 yet (workflow_runs=[], statuses=[]); do not call this CI-green until Host/Vita results appear.

### Next event-fidelity target
1. Inspect and model deadCrew / destroyed encounter reward blocks, including their item_modify and weapon rewards.
2. Complete Blue Option numeric resource conditions and quest/flag conditions.
3. Then improve event effect edge cases (augment, secretSector, modifyPursuit, reveal_map, etc.) that are still outside the current runtime model.
4. After event fidelity, replace the generic sector graph with original FTL beacon generation constraints and Rebel fleet pursuit.


## 2026-09-26 continuation — ship destroyed/deadCrew outcome data
- Inspected all 7 real <destroyed> and 7 real <deadCrew> blocks in data/events.xml.
- Outcome blocks can contain fixed/ranged item_modify rewards, autoReward, weapon rewards, and localized text keys.
- Added EventShipOutcome plus EventDatabase lookup keyed by the event ship name.
- EventDatabase now parses destroyed/deadCrew outcome definitions from real event <ship> nodes.
- EnemyDestroyed now uses an explicit destroyed outcome when one exists, applying its signed resource ranges, autoReward and weapon reward. The previous generic sector-based scrap reward remains only as a compatibility fallback for ships without an explicit destroyed block.
- deadCrew outcome definitions are now parsed and test-covered, but the combat runtime does not yet terminate an encounter when all enemy crew die; that application path remains the next subtask.
- Added regression coverage for fixed/ranged destroyed rewards and deadCrew autoReward parsing.
- No proprietary FTL data or extracted assets were committed.
- Latest implementation commit: 7e6c4728135ce066f043e1f33e1584cde8367dcc.
- Latest outcome parser/test commits: 3a6914d07a71bb77d0e3006dcb18670aaffbc45c and 1e2330f0812da270a4f577c468f437015bb1dda1.
- GitHub Actions still has not surfaced a workflow result for 1e2330f (workflow_runs=[], statuses=[]). A local clone/build was also unavailable because the execution environment could not resolve github.com. Do not claim CI or local build success.

### Next implementation order
1. Add an explicit enemy-crew-dead combat outcome and apply deadCrew rewards without requiring hull destruction.
2. Implement remaining Blue Option numeric resource / quest-state conditions.
3. Wire additional real event effects such as modifyPursuit, reveal_map, secretSector and augment rewards.
4. Then replace the generic sector graph with original beacon generation constraints and Rebel fleet pursuit.


## 2026-09-26 continuation — enemy crew wipe combat path
- CombatRuntime now distinguishes an enemy defeat caused by complete crew elimination from ordinary hull destruction.
- A manned enemy is marked EnemyDestroyed when every RuntimeCrew member is dead; automated ships with zero crew still require hull destruction.
- MainGame now selects the real deadCrew outcome for crew-wipe victories, while destroyed outcomes remain the path for hull destruction.
- deadCrew reward parsing was already present; this change connects it to the actual combat outcome.
- No separate test executable exists yet for CombatRuntime; the repository currently has data/input/localization/real-data tests, so this path is covered structurally but still needs a dedicated combat regression.
- Latest combat header commit: 9528b179fbfb1693416e81e6500ff6da680c6e22.
- Latest combat implementation commit: 40758f1cefa50bb648c2132079697ada857f935f.
- Latest MainGame reward integration commit: f7f9e9f3dc8bc8c3396a17d92ae6c283495ada03.


## 2026-09-26 continuation — conditional hidden / Blue Options
- Real FTL event data uses `hidden="true"` extensively for conditional Blue Options; dropping these nodes loses valid choices.
- `EventChoice.hidden` was added and the parser now preserves the attribute instead of discarding the choice.
- Hidden choices without a requirement remain internal and are not selectable/rendered.
- Hidden choices with a satisfied `req` are now selectable and rendered, matching the conditional nature of Blue Options.
- Event UI now compacts visible choices instead of leaving gaps caused by hidden branches.
- Existing `req` handling continues to cover crew race, crew skill + `lvl`, systems + `lvl`, augment, weapon and drone requirements.
- Numeric resource requirements and persistent story/flag requirements remain a later fidelity item; the current real-data inspection did not justify inventing unsupported semantics.
- Reference inspection confirms the original event format uses hidden conditional choices such as `req="doors" lvl="3"` and `req="ADV_SCANNERS"`; see FTL Event Parser examples. citeturn0search0turn0search3
