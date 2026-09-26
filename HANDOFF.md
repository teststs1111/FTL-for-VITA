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
  - `data/newEvents.xml`
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
