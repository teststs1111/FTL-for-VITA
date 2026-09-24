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

- `8f9c1aaf66f206b4c13465288ea87fb6025006c9`
- Message: `Fix defense volley flight timing`
- This adjusted a combat regression test after confirming that the second projectile in a volley needs more simulated flight time.

### CI state

The latest GitHub Actions Host build run associated with the latest commit is currently **failed**.

Important: do not assume the previous defense-volley timing fix is sufficient. The next action is to inspect the failed job/log, identify the exact assertion or build failure, fix the root cause, and rerun CI.

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

### 1. Fix CI first
Inspect the failed Host build run for commit `8f9c1aaf66f206b4c13465288ea87fb6025006c9`.

Previous related regression:
- defense drone intercepts one projectile from a multi-projectile volley
- the remaining projectile must have enough simulated flight time to hit
- an enemy fire delay was added to prevent an unrelated immediate second volley during tiny test updates

Do not blindly increase timing again. Check the actual failing assertion/log first.

### 2. After CI is green
Continue with real-data compatibility:
- inspect the user's real `ftl.dat` when supplied
- enumerate relevant resource paths without committing the asset
- identify Japanese text resources
- identify PNG/texture paths
- validate BXML parsing against real files
- add host tests using synthetic/minimal fixtures where possible
- only then wire real asset loading into the runtime

### 3. Rendering
After data validation:
- load PNGs from the archive
- connect texture cache to the renderer
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
| Host build/test | Active — latest run currently failing |
| Archive reader | Implemented; real `ftl.dat` validation pending |
| BXML | Implemented/tested |
| PNG/texture pipeline | Next major slice |
| Blueprint/data integration | In progress / next after assets |
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
