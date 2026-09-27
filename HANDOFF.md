# FTL for PS Vita — Development Handoff

> This file is the persistent handoff/state document for continuing the port across chat sessions.
> Update it whenever a major subsystem, build state, decision, or blocker changes.

## Repository

- GitHub: https://github.com/teststs1111/FTL-for-VITA
- Main branch: `main`
- Project: PS Vita port work for Project Wormhole / Tachyon
- Target: a Vita-native playable FTL-style runtime reconstructed from the open-source Tachyon implementation
- Proprietary FTL source/assets are **not** stored in this repository.

## Current state — 2026-09-27

### Latest commits

- `8dbd67e266fe15b5255651ecdd108ec73ecc9634` — `Enter Rebel encounter on fleet-controlled beacon`
- `881cff0b1e5b73c48fde65e693a60163feacf39f` — `Allow navigation to Rebel-controlled beacons`
- `a37d3eb2aa95d4003833470a08fa662d5fe9b75b` — `Fix sector graph coverage test scope`
- `8391a2467c110abd4279c8cb0f389fafa35624a3` — `Test variable sector graph generation`
- `660fee07e65d87f22f2e8730a2f81ad8cdce9f7f` — `Add sector graph reachability regression tests`
- `3e41699b19c51936d35e577c99dacde124100ee1` — `Track variable sector graph row layout`
- `868321f8e917fbc8a390388a4ed9561fda26a5ed` — `Use variable beacon rows for sector maps`
- `d0fe6652c29ca73f0672b7d21d3a1fee4ee984cd` — `Document automatic Rebel fleet advancement`

### CI state

- Commit `a37d3eb2aa95d4003833470a08fa662d5fe9b75b` Host and Vita Actions are both green.
- The subsequent commits `881cff0b1e5b73c48fde65e693a60163feacf39f` and `8dbd67e266fe15b5255651ecdd108ec73ecc9634` implement the next Fleet encounter step; their new Actions must be verified before marking them green.

### Startup/crash milestone

The previous crash dump showed a `vitaGL Splashscreen` thread with a GXM data-abort around the vitaGL startup path. The Vita workflow now builds a fresh vitaGL with `HAVE_SBRK=1 NO_SPLASHSCREEN=1`.

The renderer also no longer treats `vglInitExtended(960,544,...)` returning `GL_FALSE` as failure. In current vitaGL, native 960x544 initialization can return `GL_FALSE` because the return value indicates resolution fallback rather than generic initialization success.

This removes two identified startup hazards, but **real-device runtime success is not yet verified**.

### Vita application metadata

The VPK application name is now:
`FTL: Faster Than Light`

## What is already implemented
- Rebel Fleet per-beacon coverage state: `BeaconNode::fleetCovered`, v6 save/load of covered beacon indices, v5-and-earlier row-based compatibility restore, and red/!! map rendering.

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
| Sector/event flow | 🟡 Real sector database connected; variable beacon graph now implemented; individual fleet coverage and exact vanilla map rules remain pending |
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


## 2026-09-27 continuation — real event special effects
- Re-inspected the Library copy of the legitimate user-supplied `ftl.dat`; it is still external and is not committed to the repository.
- Confirmed real event data contains `modifyPursuit`, `reveal_map`, `secretSector`, and named `augment` effects. Observed `modifyPursuit` values include -2, -1, and +1; `secretSector` occurs in the base event data; named/random-style augment rewards are present.
- Added `EventSpecialEffects` to the event data model and parser for both top-level events and nested choice events.
- Runtime now applies:
  - `modifyPursuit` to the existing Rebel-fleet boundary state, clamped to the current compatibility graph;
  - `reveal_map` to persistent map-revealed state;
  - `augment` rewards using the real loaded augment blueprint database and the existing three-slot inventory;
  - `secretSector` to a persistent pending-secret-sector state marker.
- The secret-sector marker is intentionally not yet converted into a full Crystal Home-sector topology switch; that requires replacing the generic sector graph with a real sector-transition model. No fake transition was added.
- Latest implementation commits: `82093cc`, `1c5b9ac`, `95b45f3`.
- CI has not surfaced a workflow run for the latest commit yet; **do not call this continuation CI-green**.

### Next implementation order
1. Replace the compatibility graph with real sector selection/topology and use the parsed `unique` sector definitions.
2. Turn `reveal_map` into actual hidden-beacon visibility rather than only persistent state.
3. Convert `secretSector` into the real Crystal Home-sector transition.
4. Add dedicated regression fixtures for special event effects.
5. Continue deeper ship/system/weapon/crew fidelity and the real FTL HUD/input layer.


## 2026-09-27 continuation — real sector selection / unique / secret-sector state
- Fixed a stray closing brace in `src/game/main_game.cpp` that sat between `applyEventImmediateEffects` and `eventChoiceAvailable`.
- `SectorDatabase` now supports:
  - selecting candidates while excluding already-used `unique="true"` sector definitions;
  - direct lookup by the actual sector definition name.
- MainGame now selects and retains one concrete `sectorDescription` for the current sector instead of asking the database to choose a different sector definition for each beacon.
- Beacon event pools now come from that retained real sector definition, so `min/max` usage tracking applies inside the selected sector type.
- The existing compatibility beacon geometry remains in place; this change does **not** claim the 8x3 graph is already the original FTL topology.
- `secretSector` now replaces the next normal sector selection with the real `CRYSTAL_HOME` definition from `sector_data.xml` when that definition is available. No synthetic Crystal event pool is created.
- Save format advanced to `FTL_VITA_SAVE 5`; current sector type, used unique sector definitions, map-revealed state, and pending secret-sector state are persisted. Versions 2/3/4 remain readable.
- `reveal_map` now affects the current map UI by allowing event classifications to be displayed without requiring Long-Ranged Scanners; the underlying compatibility graph is still visible as before.
- Latest implementation commits:
  - `2ae8099` — sector database API for unique-aware selection / lookup
  - `277f013` — unique-aware sector selection implementation
  - `f3ec4b3` — MainGame sector-state integration and syntax fix
  - `e0d25ff` — save format v5 for sector-state persistence
- GitHub combined status for `f3ec4b3` returned no statuses; no CI-green claim is made.
- Local repository cloning/building is still unavailable in the current execution environment because DNS/network access to github.com is unavailable.
- No proprietary `ftl.dat` or extracted FTL assets were committed.

### Next implementation order
1. Replace the compatibility 8x3 graph with a closer original FTL beacon-generation model while preserving the now-fixed concrete sector definition.
2. Model sector-specific topology constraints and Rebel fleet pursuit independently from the generic graph.
3. Verify the exact Crystal Home transition path against the real event data and ensure it occurs only in valid story conditions.
4. Add dedicated sector/save regression coverage.
5. Continue real FTL HUD/input fidelity, audio, and save/load completeness.


## 2026-09-27 continuation — MainGame augment compile fix
- Found a concrete type mismatch in `src/game/main_game.cpp`: `hasAugment()` accepted only `const char*`, while real augment IDs from the blueprint database are `std::string` values.
- This affected calls such as the RANDOM augment reward path and direct blueprint augment lookup.
- Changed `hasAugment()` to accept `const std::string&`, which also accepts the existing string-literal callers through normal construction.
- Fix commit: `f63e02379730f7ea7c2d91347eb3d7b482ca596e`.
- This is a source-level compile fix identified from the latest main branch; GitHub Actions has not yet surfaced a workflow result for this commit, so Host/Vita build success is still not claimed.
- Next: wait for/inspect the Host and Vita workflow results, then continue with the remaining sector/beacon fidelity work.


## 2026-09-27 continuation — CI build errors fixed
- GitHub Actions was finally inspected directly; the latest `7addfb3` Host and Vita builds both failed during compilation.
- Host failure showed two concrete issues: the event parser source contained literal `\\n` text between statements, and the event database header on `main` did not contain the `EventSpecialEffects` model used by `main_game.cpp`.
- Vita failure independently exposed the same missing `EventSpecialEffects` type/member declarations, plus a stale `SystemType::Engines` comparison in `combat_runtime.cpp`; the runtime stores system types as strings.
- Fixed the event special-effects declarations in `include/data/event_database.hpp` and restored the `special` members on event/choice definitions.
- Fixed both malformed parser statement separators in `src/data/event_database.cpp`.
- Fixed the PDS engine check to compare against the runtime string value `"engines"`.
- Fix commits: `3ad28bc`, `5d4544c`, `e8345f8`.
- The failures were source/compile issues, not an inability of GitHub Actions to run. A new CI run is now expected from the latest fix commit.
- Do not claim Host/Vita green until that new run completes successfully.


## 2026-09-27 continuation — CI verified green after direct Actions inspection
- Direct GitHub Actions inspection was performed instead of relying on the connector's commit-status summary.
- The earlier `0545cba` Host/Vita runs were confirmed as actual failures. Their concrete compile failures were traced to the event parser's literal `\\n` source text, missing/incorrect special-effect declarations, and the stale engine-type comparison; those source issues were repaired in the intervening commits.
- The subsequent `d03aa92` run exposed another literal newline escape and the malformed `hidden/req` declarations; these were corrected.
- The `e6ecacf` run reached compilation but exposed a malformed localized-event test fixture. That fixture was corrected in `a57659b`.
- The `a57659b` Host run passed compilation/tests, while its predecessor showed the data test failing because the inline event pool was not retained. Investigation found the DLC test XML itself was malformed: the `CHOICE_ITEM` fixture omitted the closing `</item_modify>` tag, causing the whole DLC XML parse to be discarded. The parser was also hardened to retain effect-only and synthetic inline events.
- Final verification commit: `8e0a89fae0232ab80ee8822fd11a2e79d0ba6796`.
- Host Actions run `36280349373`: **success**; build completed and all 4 CTest tests passed.
- Vita Actions run `36280349426`: **success**; Vita VPK build and artifact upload completed successfully.
- This is the first directly verified Host + Vita green result after the recent compile-repair sequence.
- No proprietary `ftl.dat` or extracted FTL assets were committed.

### Next implementation target
1. Replace the compatibility 8x3 beacon graph with a closer original FTL sector/beacon generation model.
2. Model Rebel Fleet advancement after jumps, rather than relying mainly on event `modifyPursuit` deltas.
3. Preserve the now-working concrete sector selection, unique-sector tracking, event pools, and save-state behavior while introducing the more faithful topology.
4. Add dedicated regression coverage for beacon reachability, fleet pressure, sector transition, and Crystal Home conditions.


## 2026-09-27 continuation — automatic Rebel fleet advancement
- Added automatic Rebel fleet movement to the normal beacon-jump path.
- The compatibility graph still represents fleet pressure as a row boundary, but the boundary now advances by one row after each successful player jump instead of moving only when an event contains `modifyPursuit`.
- Existing event `modifyPursuit` effects continue to apply afterward, allowing pursuit-related events to move the boundary backward/forward relative to normal progression.
- The change is intentionally isolated from fuel/combat reward handling: fuel is consumed once per jump, then the fleet advances, then the beacon event/combat flow continues.
- Implementation commit: `8e72b5b0bd7e474501ca8c56514d7f4ae064dac1`.
- Direct CI verification for this commit: Host run `36280519300` **success**; Vita run `36280519307` **success**, including VPK build and artifact upload.

### Next target
1. Replace the fixed 8x3 compatibility topology with a more faithful FTL-style variable beacon graph while preserving reachability.
2. Add explicit fleet-covered beacon state instead of only a row cutoff.
3. Verify sector-specific generation constraints and exit/starting-beacon behavior against the supplied sector data.
4. Add regression coverage for automatic fleet advancement and pursuit modifiers.


## 2026-09-27 continuation: sector graph fidelity

- Replaced the fixed 8x3 beacon grid with a deterministic variable-width graph.
- Each of the eight progression rows now contains 3–5 beacons, placed across a wider logical column range.
- Links only advance one row at a time, use nearby columns, and include a guaranteed spine from the selected start beacon to the exit row.
- Added `tests/test_sector_graph.cpp` covering:
  - deterministic generation;
  - variable node counts;
  - valid forward-only row links;
  - reachable exit;
  - initial beacon selection;
  - fleet-row filtering.
- This is intentionally an incremental compatibility step. The next map-fidelity step is to replace the remaining row-only Rebel fleet approximation with explicit covered-beacon state / movement while preserving deterministic tests and save compatibility.
- CI for this change is pending; the previously verified green baseline is `8e72b5b0bd7e474501ca8c56514d7f4ae064dac1`.


### Latest Fleet encounter fidelity

Fleet-controlled beacons are now navigable instead of being removed from the route. On arrival, if the beacon is covered after the normal Fleet advance, MainGame enters a Rebel ship encounter before normal event selection. The current implementation dynamically chooses the first non-player/non-boss ship blueprint whose ID contains `REBEL`; this is a compatibility step toward exact Rebel Fleet/ASB encounter rules. The row-boundary model remains underneath the explicit per-beacon flags.
 The encounter now prefers conventional `REBEL_FIGHTER` / `REBEL_SCOUT` / `REBEL_ELITE` blueprint IDs when available and enables the existing player-target PDS/ASB combat hazard; alternate datasets still use a deterministic Rebel-ship fallback.

## 2026-09-27 continuation — Rebel-controlled beacon navigation fix
- Direct source inspection found a logic mismatch: MainGame correctly checks `fleetCovered` after arrival and starts a Rebel fleet encounter, but SectorGraph was still filtering covered destinations out of `selectable()`.
- This made the new Rebel fleet encounter path unreachable from the map.
- Fixed `SectorGraph::selectable()` so fleet-covered beacons remain navigable; the fleet flag is now interpreted at arrival, where MainGame starts the Rebel encounter and PDS/ASB environment.
- Updated the sector-graph regression to require that a deliberately covered reachable beacon remains selectable.
- Fix commits: `0726b23` and `695cbbd`.
- CI verification is pending for these latest source changes.

## 2026-09-27 continuation — combat retreat advances Rebel fleet
- Direct source inspection found that normal beacon jumps called `advanceRebelFleetAfterJump()`, while a successful FTL retreat from combat consumed fuel and returned to the sector map without advancing the Rebel fleet.
- Fixed the combat-retreat completion path to call `advanceRebelFleetAfterJump()` immediately after successful fuel consumption.
- This keeps fleet movement consistent across normal navigation, post-combat navigation, and combat escape; aborted FTL charging still does not move the fleet.
- Source commit: `4eeebc75` (behavior change), followed by `d040a421` to restore the file's original formatting/newline state.
- Host CI run: `36283829547`; Vita CI run: `36283829552`. Both were running when this handoff entry was written.


## 2026-09-27 continuation — match real Rebel Fleet Elite encounter/reward
- Re-inspected the legitimate user-supplied `ftl.dat` directly. The archive's real Rebel ship blueprints use `REBEL_SKINNY_ELITE` (with `REBEL_SKINNY_ELITE_DLC` in the AE override layer); the previously used compatibility IDs `REBEL_FIGHTER` / `REBEL_SCOUT` / `REBEL_ELITE` are not the real blueprint names in this archive.
- Updated the Rebel Fleet encounter selection to prefer `REBEL_SKINNY_ELITE`, then the archive's DLC elite variant, with the old generic IDs retained only as alternate-dataset fallbacks.
- Rebel-controlled beacons now use the vanilla-style fixed reward path: defeating the fleet Elite Fighter grants **1 fuel** and does not grant the normal scrap/event reward.
- Successful FTL retreat clears the fleet-encounter reward marker so a later unrelated combat cannot inherit the special reward.
- The existing player-target PDS/ASB environment remains enabled for the fleet encounter.
- Source commit: `7b0b50c8`.
- The archive was inspected locally from the user's Library copy; no proprietary data was added to the repository.


## 2026-09-27 continuation — queue pursuit modifiers until the next jump
- Rechecked current Host/Vita CI for `1edcfbf`: both are green.
- Revisited the real FTL pursuit behavior. `modifyPursuit` is a modifier to the fleet's next advancement, not an immediate teleport of the fleet boundary. The current implementation was applying it immediately, which could make a beacon become captured before the next jump.
- Changed `MainGame` to accumulate `fleetPursuitDelay_` and consume it on the next successful jump: `-1` cancels that jump's normal fleet advance, `+1` makes that jump advance two steps, and multiple chained modifiers accumulate.
- Save format is now v7 and persists the pending pursuit modifier. v2-v6 loading remains supported.
- Source commit: `1845d088`.
- External mechanics references support the next-jump interpretation and also identify additional fidelity work: nebula destinations reduce that jump's advance, Rebel-controlled sector entry adds an advance, and Distraction Buoys/out-of-fuel/other events can defer pursuit. citeturn0search0turn0search6


## 2026-09-27 continuation — nebula pursuit and save-state fidelity
- Added explicit `BeaconNode::nebula` state and a sector-level nebula marker in the compatibility graph.
- Nebula sectors (`NEBULA_SECTOR`, `SLUG_SECTOR`, `SLUG_HOME`) now mark their generated beacons as nebula destinations. This is an incremental fidelity layer; non-nebula sectors still need the original per-beacon NEBULA_* event assignment instead of the current generic graph flag.
- Rebel Fleet pursuit now accumulates fractional progress. A normal jump advances by 1.0, a nebula destination in a normal sector uses 0.5, and a nebula destination in a nebula/Slug sector uses 0.8. Pending `modifyPursuit` modifiers are applied before that multiplier and the fractional remainder carries into later jumps.
- Fleet pursuit state is reset at sector boundaries, matching the fact that each new sector starts with its own fleet position rather than carrying the previous sector's row boundary.
- Save format is now v8 and persists the fractional pursuit remainder. v2-v7 compatibility remains supported.
- Also fixed an existing save/load mismatch: saved augments are now restored before system state parsing, and v7/v8 saves correctly read current-sector, unique-sector, map, and fleet-covered state blocks.
- Source commits: `0d292d9`, `eb84d20`, `5bd27f7`, `0e277c7`, `afaaa92`, `1231008`.
- Current Host/Vita Actions for `1231008` are still in progress at the time of this update; do not mark this change green until both complete successfully.
- External FTL mechanics references confirm that normal-sector nebula jumps reduce Rebel pursuit to about 50%, nebula-sector jumps to about 80%, and Distraction Buoys delay the fleet by one jump at sector start. citeturn0search0turn1search5


## 2026-09-27 continuation — Distraction Buoys sector-start fleet delay
- Implemented the Advanced Edition Distraction Buoys behavior in commit `df27982ccfe1e049f180d4d6b6cf6e62c3a57a7c`.
- When the player owns `DISTRACTION_BUOYS`, entering a new sector now adds one pending Rebel Fleet delay before the first jump of that sector.
- The delay is applied after selecting the new sector definition, including the initial sector and normal sector transitions, and is not applied in The Last Stand (sector 8), matching the documented final-sector behavior.
- The existing pursuit-delay pipeline then consumes that pending delay on the first jump, so this uses the same state path as event-based `modifyPursuit` effects instead of introducing a separate fleet counter.
- The delay is reset/re-applied on each actual sector transition and is not re-applied when loading a save or merely toggling the AE dataset.
- External mechanics references confirm Distraction Buoys postpone Rebel Fleet advancement by one jump at sector start and have no practical effect on The Last Stand's separate fleet model. citeturn0search0turn1search0
- CI for `df27982` must still be checked directly before calling this change green.

### Next target
1. Implement scout/auto-ship escape pursuit doubling for the next jump.
2. Replace generic sector-wide nebula flags with per-beacon NEBULA_* event assignment in normal sectors.
3. Continue toward the original 6×4 / 16–24 beacon sector-generation constraints and sector-specific event weighting.


## 2026-09-27 continuation — data-driven nebula beacon assignment
- Added per-beacon nebula state support in `SectorGraph` and replaced the previous normal-sector all-clear approximation in commit `cb3ae1193f7a73bd9b63e51b5b0c36ea8f397b91`.
- For non-nebula sectors, the runtime now reads the original sector event pools and treats event-list names beginning with `NEBULA_` as the source of nebula beacon counts. The deterministic assignment respects the aggregate min/max ranges and keeps the starting beacon clear.
- Nebula/Slug sectors that currently use the compatibility whole-sector nebula model are intentionally unchanged; the remaining fidelity work is to reproduce the original 6×4 placement/overlap algorithm so additional neighboring beacons can be converted into nebula beacons exactly as the game does.
- This is a data-driven intermediate step, not a claim of exact vanilla map generation. The original generation process first places beacons on a 6×4 grid, then processes NEBULA_* lists before normal event assignment; overlapping nebula graphics can convert additional beacons. citeturn3search0
- Host/Vita Actions for `cb3ae119` are currently queued (Host/Vita must both complete successfully before this change is marked green).

### Next target
1. Verify the queued Host/Vita builds for the nebula assignment change.
2. Move the sector graph toward the documented 6×4 / ~16–24 beacon generation and adjacency rules.
3. Preserve per-beacon nebula state through save/load and fleet takeover transitions.
4. Implement Rebel scout/auto-ship escape pursuit acceleration once the combat runtime has an explicit enemy-escape outcome.


## 2026-09-27 continuation — beacon-level Rebel Fleet frontier
- Latest verified commit: `efbfa4f2e9895c35a288f68cd78691b2ca5201d5` (`Fix fleet frontier regression expectation`).
- Rebel Fleet coverage is now tracked at individual beacon level via `BeaconNode::fleetCovered`, with `SectorGraph::advanceFleetCoverage(steps)` extending a connected frontier through links instead of marking an entire row covered at once.
- Fleet-controlled beacons remain navigable; arrival can trigger the Rebel fleet encounter rather than making the route disappear.
- Save/load continues to persist covered beacon indices, while the older row-based coverage method remains available for compatibility/legacy restore.
- Verification: Host build #726 **success** and Vita build #418 **success** on the same commit. The Host suite completed all 5 tests successfully.
- This is still an approximation of vanilla FTL fleet positioning: the next frontier beacon is currently selected deterministically by lowest node index. The next fidelity step is to model more faithful fleet route/frontier selection and sector-specific pursuit behavior.

### Next target
1. Replace lowest-index fleet frontier selection with a map-aware deterministic rule closer to FTL's actual pursuit behavior.
2. Continue tightening the 6×4 / 16–24 beacon generation and adjacency constraints against the real FTL map model.
3. Keep per-beacon nebula and fleet state consistent through navigation, sector transitions, and save/load.


## 2026-09-27 continuation — vanilla 6x4 sector graph and Fleet state cleanup
- SectorGraph now uses 6 logical rows x 4 logical columns, about 80% beacon occupancy, 16–24 beacons, randomized beacon coordinates, adjacent-row links with a distance limit, and retry-based start-to-exit reachability.
- The exit is one explicit beacon in the final row rather than an entire final-column exit zone.
- Normal Rebel Fleet coverage uses continuous x-position pursuit; Sector 8 uses individual beacon takeover instead of the normal frontier model.
- Commit 84382a2 fixed a regression-test assumption that positional coverage would always expand to exactly two nodes.
- Commit c70e372 fixed new-game initialization so initial Fleet position is applied after map generation and the Distraction Buoys one-jump delay is not accidentally cleared.
- CI for c70e372 was still queued/running when this entry was prepared; verify Host and Vita directly before marking it green.
- Remaining Last Stand fidelity work: model the Flagship movement cadence/location more faithfully while preserving the three-phase flagship combat sequence and individual beacon takeover behavior.


## 2026-09-27 continuation — Last Stand Flagship persistence and map presentation
- Verified the previous Last Stand movement implementation at commit `53e97de`: Host build #756 **success** and Vita build #448 **success**.
- Added save-format v10 in `src/game/main_game.cpp` so Sector 8 Flagship state survives save/load:
  - current Flagship beacon;
  - Federation Base beacon;
  - route index and generated route;
  - two-player-jump movement counter;
  - Base three-turn countdown;
  - one-turn post-phase wait.
- Fixed v10 loading so saved fleet-covered beacon indices are restored **after** regenerating the deterministic sector graph. This avoids losing explicit Fleet state during load.
- Added Sector 8 map markers for the Flagship and Federation Base, plus a visible Base countdown and Flagship movement cadence hint.
- Random Last Stand beacon takeover now excludes the Federation Base so the Base remains the dedicated Flagship destination/countdown location.
- CI verification for the new commits:
  - Host build #759 (`c5e8571`) **success**
  - Vita build #451 (`c5e8571`) **success**
- No proprietary `ftl.dat` or extracted FTL assets were committed.

### Next implementation order
1. Add a proper Sector 8 wait/idle action that advances the Flagship/random beacon takeover exactly when the player waits, matching the vanilla Last Stand map tick.
2. Add the vanilla Sector 8 entry resource behavior (fuel/hull repair) after confirming the existing initialization path does not already provide it.
3. Replace the current Flagship route scaffold with explicit Last Stand start/base node selection and more faithful 3–5 Flagship-jump routing.
4. Continue save/load regression coverage for Last Stand state and per-beacon Fleet/nebula state.


## 2026-09-27 continuation — Last Stand entry resources
- Added the vanilla Sector 8 entry resource behavior in commit `b09914b`: entering The Last Stand from Sector 7 grants +10 fuel and repairs +10 hull, capped at max hull.
- This is applied only on the actual Sector 7 → Sector 8 transition, so save loading and normal map regeneration do not repeat the bonus.
- The existing Flagship/wait implementation remains unchanged.
- CI for `b09914b` must be verified before marking this change green.

### Next target
1. Verify Host/Vita CI for the Last Stand entry-resource change.
2. Replace the Flagship route scaffold with explicit Last Stand start/base node selection and more faithful 3–5 jump routing.
3. Add Last Stand save/load regression coverage and tighten random beacon takeover timing.

## 2026-09-27 continuation — Last Stand Flagship 3–5 jump route

- Corrected the Sector 8 Flagship route direction in `31cadcb8e3dc2e668729d152ef5178e7e2a543eb`.
- The Flagship now starts on the rightmost map row and selects a deterministic 3, 4, or 5 Flagship-jump route toward the Federation Base, matching the documented vanilla 6/8/10 player-jump cadence.
- Route construction was hardened in `e39933a3d061d30722969178b3e173c452d12ee4`: a breadth-first search follows actual incoming beacon links, preventing a greedy predecessor choice from producing a dead-end.
- The stored route is ordered Flagship -> Base; `flagshipRouteIndex_` starts at 0 and advances toward the Base.
- CI verified directly for `e39933a3d061d30722969178b3e173c452d12ee4`: Host build #769 and Vita build #461 both **success**.
- No proprietary FTL assets were committed.

### 2026-09-27 continuation — v10 validation and vanilla placement review

- Verified Host #771 and Vita #463 for commit `6eb76d3`: both **success**.
- Added a visible Sector 8 Flagship next-destination route cue in `renderSectorMap()`, matching vanilla behavior where the next Flagship beacon is shown on the map.
- Added v10 Last Stand state validation in commit `8f58ef7`: after deterministic map regeneration, saved Flagship route/node indices are checked before being trusted; malformed or stale v10 Flagship state falls back to fresh Last Stand initialization instead of leaving invalid beacon references.
- Vanilla placement review found that the Flagship is documented as starting on column 4 or 5 and the Federation Base around column 2/3 on Normal/Easy (3/4 on Hard), while the Flagship still requires 3–5 Flagship jumps to reach the Base. The current normal-sector graph is not sufficient to reproduce all of those constraints by simply using row-distance, so the existing route scaffold is intentionally **not** changed blindly.
- Vanilla sources also confirm the next-destination line, two-player-jump cadence, random beacon takeover, wait behavior, and three consecutive Base turns. cite references were used in the development log, not embedded in repo docs.
- CI for `8f58ef7` is currently queued: Host #772 / Vita #464.

### Next target
1. Verify Host/Vita CI for `8f58ef7`.
2. Design a dedicated Last Stand route generator instead of forcing vanilla Flagship/Base placement through the normal sector graph.
3. Add/strengthen Host regression coverage for v10 Flagship persistence, 2-player-jump movement, post-phase wait, and 3-turn Base countdown.
4. Verify random beacon takeover timing and interaction with Flagship movement.

## 2026-09-27 continuation — Last Stand route regression verified
- Commit `314e65e8357d3201c0ea3bee7b336dca71979d48` added regression coverage that searches the generated Sector 8 graph for a connected Flagship-to-Base route of 3–5 edges using the actual Last Stand links.
- Host #784 and Vita #476 both **success**.
- Code review confirms the current runtime behavior: Flagship advances every 2 player jumps, waiting consumes a map tick without fuel, post-phase retreat sets a one-turn wait, and Base occupation increments the three-turn countdown on player jumps.
- The next testing target is to exercise these runtime state transitions rather than only validating graph shape.


## 2026-09-27 continuation — Last Stand runtime state transitions verified
- Added `src/data/last_stand_state.hpp/.cpp` so the Flagship map-tick state machine is shared by runtime code and regression tests.
- Added regression coverage for: first/second player jump cadence, one-turn post-phase wait, Flagship retreat, Federation Base three-turn countdown, and route-end countdown behavior.
- Host #791 and Vita #483 for commit `ff79190c3144ed7b93517c5d3b215c0a520c0bdb` both **success**.
- The initial Host failure (#790) was a test-source formatting error (`\\n` literal); fixed in the next commit and reverified successfully.
- No proprietary FTL assets were committed.

### Next target
1. Verify the Sector 8 wait/idle path against the shared state machine and ensure it advances the same map tick as a jump without consuming fuel.
2. Strengthen Last Stand save/load regression coverage for the new state fields.
3. Revisit exact vanilla Flagship/Base placement and route generation after runtime persistence is covered.


## 2026-09-27 continuation: Last Stand event-pool verification

- Vita build #485 for `2377e3dc61808afadee7e047a5d8b149b1ffcdec` is verified **success**; Host #793 is also **success**.
- The Last Stand repair beacon now uses the documented `BOSS_REPAIR_STATION` rewards: +15 hull, +22-44 scrap, +5 fuel, +4 missiles, +5 drone parts.
- Added real-data regression checks in `tests/test_real_ftl_dat.cpp` for the vanilla `FINAL` sector event pools:
  - `STORE`: 1-1
  - `BOSS_REPAIR_STATION`: 3-3
  - `BOSS_HOSTILE`: 6-6
  - `BOSS_NEUTRAL`: 7-10
- These checks run only when `FTL_DAT_PATH` points at the user-supplied archive, so proprietary data remains outside CI/repository history.
- Next target: make Last Stand beacon event assignment persist per beacon and verify the event pool min/max behavior against the actual map flow, then continue save/load and exact vanilla placement fidelity.


## 2026-09-27 continuation — Last Stand event assignment hardening

- Verified Host #797 and Vita #489 for commit `9a99b4639cee34a58fe3f34d4bce4fab6efd415f`: both **success**.
- Confirmed v11 saves preserve the Last Stand Flagship route/state and per-beacon event assignments.
- Hardened `beginBeaconEvent()` so a beacon assignment is written only after the selected event successfully resolves to a concrete event id; failed lookups no longer poison the assignment map.
- New commit: `590010b48888fd67a547dff19b157955c976d0af`.
- Host #798 is currently **in progress** and Vita #490 is **queued**; do not treat this commit as CI-green yet.
- No proprietary `ftl.dat` or extracted FTL assets were committed.

### Next target
1. Verify Host/Vita CI for `590010b`.
2. Strengthen Last Stand event-pool assignment so the generated beacon distribution follows the vanilla FINAL min/max pool more faithfully instead of relying only on visit order.
3. Continue Last Stand save/load regression and exact vanilla placement/wait behavior.


## 2026-09-27 continuation — Vanilla beacon-count correction

- Rechecked the vanilla map specification: sector maps use **19-24 beacons**, not 16-24. This is independently documented in the current FTL research corpus. citeturn0search0turn0search1
- Corrected `SectorGraph::generate()` from 16-24 to 19-24 beacons and updated the regression assertion.
- Commits: `5de01cc` (generator), `08dbb43` (test).
- The change is intentionally limited to the beacon-count invariant; no proprietary FTL assets were committed.

### Next target
1. Verify Host/Vita CI for the beacon-count correction.
2. Continue replacing approximation in map placement/links with the documented vanilla generation rules.
3. Continue Last Stand event/save fidelity work.
