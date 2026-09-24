# FTL for PS Vita

PS Vita port work for Project Wormhole / Tachyon.

This repository is the working port tree. User-owned FTL assets such as `ftl.dat` are not included.

## Current status

### Working
- C++17 migration prototype
- Vita-independent `ftl.dat` archive reader
- Tachyon-compatible BXML reader
- Host-side regression tests
- GitHub Actions host build/test
- VitaSDK CMake toolchain detection
- Vita SELF/VPK packaging configuration
- vitaGL 960x544 bring-up renderer
- PS Vita controller polling
- Start+Select exit path for hardware bring-up
- Japanese localization foundation with Japanese as the default Vita locale
- English fallback catalog for development/testing
- Localization regression tests

### Localization

The project now has a built-in localization service under `include/i18n/` and `src/i18n/`.

- Default locale: **Japanese**
- English remains available as a development fallback
- Game/UI code uses stable translation keys instead of hard-coded Japanese strings
- Missing Japanese entries fall back to English rather than rendering an empty label
- The catalog already covers core FTL UI terms such as crew, weapons, shields, engines, oxygen, drones, hull, resources, map, beacon, sector, event, store, save/load, combat results, and touch controls

This is the localization foundation; the remaining UI renderer work will connect these strings to the actual Vita HUD/menu screens. Japanese text rendering itself will be added as part of the Vita UI layer so UTF-8 Japanese is displayed correctly on hardware.

### Not yet implemented
The current Vita scene is an early playable-systems prototype and is **not yet the complete FTL game**.

The remaining work is being done in dependency order:
1. Validate real user-supplied `ftl.dat` contents and Japanese resource paths
2. PNG/texture loading from `ftl.dat`
3. Blueprint/database layer
4. Ship rooms, doors and systems
5. Crew/weapons/projectiles and combat simulation
6. Sector/beacon/event flow
7. FTL UI, Japanese text rendering and touch controls
8. audio
9. save/load
10. mod support

FTL game assets are not distributed by this repository. The runtime expects a user-supplied `ftl.dat` and never embeds it in the repository.

- Vita: `ux0:data/wormhole/ftl.dat`
- Host: set `FTL_DAT_PATH` to your local `ftl.dat`, or place it at `./ftl.dat`

For example:

```sh
FTL_DAT_PATH=/path/to/your/ftl.dat ./build/vita_wormhole_prototype
```

The archive reader supports both the Tachyon prototype `PKG\\n` container and the vanilla FTL `.dat` layout. The latter uses a little-endian file-slot table followed by per-file size, filename length, UTF-8 filename and file body.

## Vita build

Install VitaSDK and its vitaGL dependency, then configure with the VitaSDK toolchain:

```sh
cmake -S . -B build-vita -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" -DBUILD_TESTS=OFF
cmake --build build-vita --parallel
```

The CMake project generates the SELF/VPK packaging targets when the Vita toolchain is active.

## Host validation

```sh
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Source reference

The C++ port is being reconstructed from the open-source Tachyon implementation. The port intentionally keeps desktop-only Swing/GLFW tooling out of the first Vita runtime.
