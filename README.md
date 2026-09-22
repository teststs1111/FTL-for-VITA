# FTL for PS Vita

PS Vita port work for Project Wormhole / Tachyon.

This repository is the working port tree. User-owned FTL assets such as `ftl.dat` are not included.

## Current status
- C++17 migration prototype added
- Vita-independent `ftl.dat` archive reader implemented from Tachyon's format
- Tachyon-compatible BXML reader implemented
- Host build verified with CMake
- Vita-specific renderer/input backend remains next

## Port order
1. VitaGL renderer backend
2. PNG/texture loading
3. Vita controls/touch mapping
4. Ship/layout/systems model
5. InGameState simulation
6. UI
7. audio
8. save/load

The parser intentionally supports the same uncompressed `ftl.dat` subset currently accepted by Tachyon. FTL game assets are not included.
