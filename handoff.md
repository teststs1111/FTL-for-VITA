# FTL-for-VITA handoff

## Current direction
- Goal: keep the Vita port as close as practical to the original FTL 1.6.x behavior.
- The runtime uses the original `ftl.dat` as the canonical source of game data/assets; do not replace its contents with synthetic DLC tables when the corresponding data exists in the archive.
- Advanced Edition content is selected from the archive's canonical `dlc*.xml` resources.

## ftl.dat
- The working reference file is the single library file named `ftl.dat`.
- The verified file is 280,573,482 bytes.
- Its format is SIL/Tachyon PKG: magic `PKG\\n`, header size 16, entry size 20, 3219 entries.
- PKG index fields are big-endian. The upper byte of `nameofs_flags` is the compression flag; the lower 24 bits are the pathname offset.
- `FtlDat::readFile()` now handles both stored and deflated entries.

## Build / verification
- `.github/workflows/build.yml` was added for host regression tests and a VitaSDK container build producing `FTL-for-VITA.vpk`.
- Host CMake explicitly links zlib.
- `tests/test_ftl_pkg.cpp` covers stored and deflated PKG entries.
- The real library `ftl.dat` was locally exercised against the corrected reader: 3219 entries opened and canonical blueprint/DLC/sector/localization/Kestrel PNG resources were read successfully.
- Commit `766eda555357dfce3e49e2c660e80ecf8bd4f1a0` has all three workflows successful (Host, Vita, integrated build).
- The Vita artifact from that commit is a valid non-expired `FTL-for-VITA-vpk` artifact (1,445,803 bytes; SHA-256 `ab16c7cf28f5ec49370c29384cf1720181b70131f6dba00d4d2aaac72d282308`).

## Vita startup / runtime
- Startup diagnostics cover process start, graphics, input, audio, archive/content loading, text/PVF initialization, scene asset preparation, and first-frame begin/update/render/end checkpoints.
- The enemy combat ship/runtime is deferred until an actual beacon encounter; startup no longer synthesizes an arbitrary enemy.
- Exploration music OGG decode is deferred until after the first two rendered frames. The pending counter is intentionally `3` because the decrement happens at the beginning of each update.
- Vita text rendering caches up to 16 rendered text textures with LRU replacement to avoid repeated PVF rasterization and GPU texture churn.
- Transient XML/PNG/OGG source bytes are released after parsing/upload/decode where possible.
- There is still no claim of real-device boot success until a physical Vita test reaches the first-frame checkpoints.

## Reporting
- Keep progress reports concise.
- Do not repeatedly report that `ftl.dat` was checked or that CI is being checked.
- When an error is found, fix it before moving on to the next feature.
- Prioritize Vita startup/runtime blockers before adding broad new gameplay features.
- Startup memory: `AssetStore` supports releasing transient binary blobs after texture upload/audio decode; avoid retaining duplicate compressed PNG/OGG data alongside GPU/PCM resources.
- Vita startup memory: combat weapon/drone textures are loaded lazily on first combat use instead of eagerly loading the full blueprint set during startup; texture/audio transient asset bytes are released after upload/decode.
- Commit `ea738c50658c286e9875cb63cd17da7f0a6f6e05` defers player ship runtime initialization and room/crew/hull texture uploads until the player confirms a ship; the initial Ship Select scene now uses canonical blueprint metadata only. Host, Vita, and integrated workflows all succeeded, and the Vita VPK artifact is valid (1,444,212 bytes; SHA-256 `b37f30149c2c64ce54a45de30b001cfb94c96b5ca426ee072bab1d9aff68dd4d`).


## 2026-10-03 Vita fixed-function render state
- Commit `46bff11b438158321d2352069fea69a77d92cc65` initializes the Vita viewport and orthographic projection/modelview state immediately after vitaGL initialization, before the first immediate-mode vertex is submitted.
- This targets the real-device crash boundary observed at `fill_rect_vertex_1_begin`; no gameplay/data/archive behavior is changed.
