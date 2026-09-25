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


## 2026-09-26 continuation: original sector/event data path
- Added `EventDatabase` to scan the loaded FTL archive for `data/events*.xml` and expose named events/eventLists.
- Added `SectorDatabase` to parse `data/sector_data.xml` (and AE sidecar when present), including sector descriptions, minimum sector, start event, and beacon event pools.
- Beacon selection now prefers the original sectorDescription event pool instead of always forcing combat. This is the first step toward the actual FTL beacon -> event -> choice -> combat/store/reward loop.
- The current five-node map geometry remains a temporary stand-in; the next major gameplay task is the real connected/procedural beacon graph and Rebel fleet pressure.
- Event parsing currently implements the common event/choice/load/hostile/store/repair/item_modify paths. Complex nested requirements, blue options, quests, multi-stage rewards, and full combat encounter resolution still need to be wired.
- DLC/mod layering remains archive-profile based: the base `ftl.dat` stays external, with optional sidecar archives layered through `.dlc`.


## 2026-09-26 continuation: connected beacon graph
- Added a deterministic seeded multi-lane sector graph (8 rows × 3 lanes) with branching/converging links.
- Normal navigation now consumes fuel when jumping to a connected beacon; combat no longer consumes a second fuel unit on victory.
- Reaching the final row advances to the next sector; sector 8 final-row completion enters the victory state.
- The graph is still a compatibility/prototype layer; the next fidelity step is to apply original FTL beacon type weighting, Rebel fleet pursuit, and sector-specific graph constraints rather than using the generic 8×3 generator.
