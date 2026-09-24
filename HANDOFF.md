# FTL for PS Vita — Development Handoff

> This file is the persistent handoff/state document for continuing the port across chat sessions.
> Update it whenever a major subsystem, build state, decision, or blocker changes.

## Repository

- GitHub: https://github.com/teststs1111/FTL-for-VITA
- Main branch: `main`
- Project: PS Vita port work for Project Wormhole / Tachyon
- Target: a Vita-native playable FTL-style runtime reconstructed from the open-source Tachyon implementation
- Proprietary FTL source/assets are **not** stored in this repository.

## Current state — 2026-09-24

### Latest commit

- `27d7710d21b89b5ff99d6be6fe6e92c9e6cc58fc`
- Message: `Select live ship texture from blueprint artwork`
- CI run #303 is green.

### CI state

The latest GitHub Actions Host build/test run (#303) is **successful**. The live ship-texture integration and the preceding blueprint-artwork commits all passed host build/test.

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
| Vita renderer | Bring-up stage |
| Japanese localization | Foundation implemented |
| Japanese font rendering | Pending |
| FTL UI/touch | Pending |
| Sector/event flow | Pending |
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
