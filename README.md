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

### Not yet implemented
The current Vita scene is a renderer/input smoke test. It is **not yet the FTL game**.

The remaining work is being done in dependency order:
1. PNG/texture loading from `ftl.dat`
2. Blueprint/database layer
3. Ship rooms, doors and systems
4. Crew/weapons/projectiles and combat simulation
5. Sector/beacon/event flow
6. FTL UI and touch controls
7. audio
8. save/load
9. mod support

FTL game assets are not distributed by this repository. Put your legally obtained `ftl.dat` at:

`ux0:data/wormhole/ftl.dat`

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
