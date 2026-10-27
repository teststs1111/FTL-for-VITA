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

## Vita text encoding
- The PVF UTF-8 conversion now validates multibyte sequences and converts valid four-byte UTF-8 code points into UTF-16 surrogate pairs. Invalid or truncated sequences produce a replacement question mark rather than being interpreted as malformed code points.
- This prevents supplementary-plane characters in event labels and other text from becoming a run of question marks. Actual glyph availability still depends on the Vita's selected PVF fonts.
- Source commit: `ac819dd`.

## Audio resampling
- Non-looping OGG and WAV playback now uses linear interpolation for both stereo channels when converting to the Vita's 48 kHz output rate, replacing nearest-neighbor sampling that could add audible stepping/aliasing.
- Looping music still uses the existing memory-bounded mono decode path; stereo streaming remains a separate fidelity improvement.
- Source commit: `80858ac`.

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


## 2026-10-14 runtime follow-up
- Commit `cf4cb7859230d6052be1ac8bd8be8bf0136d5057` first replaced dynamic draw-stage tracking to reduce render-loop file I/O. Follow-up commit `daeac97514c667f4da9635730fc64b8669dec07b` improves this further with a fixed bitmask: each distinct known rectangle/line GL checkpoint is retained once per session, so crash boundaries remain diagnosable without dynamic containers or repeated per-primitive file writes. Frame-loop bookkeeping checkpoints are skipped, but `first_frame_render_complete` is deliberately retained so the next device dump can prove whether a full frame reached completion. Commit `0c34a9cb6ae4a721a972eddcde7de642b4d58fa0` restores that one-shot checkpoint; `7e0e8a4231b55aa244b3472c23ae5203ecac0f06` adds regression coverage.
- Commit `c4b446d29927fcac3771921bdb51c4ab4cef4440` changes sector-map D-pad navigation from stepping through the link list to selecting the nearest connected beacon in the pressed screen direction.
- Commit `234fc8569190b966b7be36d5af9525fc0f2ec994` adds regression coverage for duplicate draw-stage suppression. Host build, integrated build, and Vita build all completed successfully for this commit; the Vita VPK was produced. This is build verification only, not proof of physical-device success.
- Commit `e2cd5fd40f037bef337464ad1e0e3f5f65cc2e80` makes the diagnostics test remove its previous host dump before checking counts, preventing false failures on repeated local runs. Commit `a47a748ec05813c0e02b53340b37d88b3b65aa3a` verifies that duplicate checkpoints are suppressed while distinct GL checkpoints are retained.
- Next priority remains testing the render-loop stall fix on hardware from the dump's last known point (`first_playable_map_ready` followed by repeated rectangle diagnostics), then using the new dump/PSP2DMP to locate the next blocker. Do not claim hardware success until a physical Vita reaches later frame checkpoints.

## 2026-10-14 event text safety follow-up
- Beacon event text was previously truncated with a raw 180-byte string resize. Since event/localized text may contain Japanese UTF-8, this could cut a multibyte character and send malformed text into the glyph renderer.
- Commit `7f8cbd3` moves the truncation boundary back to the start of a character crossing the byte limit. Host, integrated, and Vita workflows are running for this source revision; do not treat them as passed until their conclusions are successful.


## 2026-10-14 beacon event text layout
- Event descriptions are now split into UTF-8-safe lines in the event panel. The renderer does not wrap text itself, so long Japanese descriptions previously ran beyond the panel edge.
- The display shows up to three lines before the event-choice area; the existing 180-byte safety limit remains in place.
- Commit: `f2df575`. Host regression, integrated build, and Vita build results must be checked for this revision.


- Follow-up: long event-choice labels also truncate at a UTF-8 character boundary; the previous 90-byte resize could corrupt Japanese text. Commit: `037e694`.


## 2026-10-21 final-sector music fidelity
- Commit `b4b29491a7eb50c6d2713696ca2d68d35e7e5517` prioritizes the sector-8 Last Stand music before sector-type matching. Previously, a generic sector type such as `Civilian` could select a normal exploration track before the final-sector condition was reached.
- Verify Host, integrated, and Vita workflows for this revision before considering the fix build-validated.


## Autonomous development operating protocol
- A recurring ChatGPT task named `FTL Vita 自律開発` was created on 2026-10-21. It runs daily on a flexible schedule (Asia/Tokyo, target morning) and is intended to make real code/test changes rather than send progress-only reports.
- Each run should inspect the current `main` state and latest Host/Vita/integrated GitHub Actions results before choosing one highest-priority, non-duplicate task. Fix failed builds before expanding features.
- Keep commits small and independently verifiable. After source changes, record meaningful decisions and the next blocker here. Never claim a physical Vita test passed without user-provided device evidence.
- Stop and notify the user when a physical-device observation is the critical next step; give exact steps and the expected diagnostic evidence. Continue unrelated code/test work where possible.
- Platform limitation: the recurring task is a scheduled work session, not a permanently running process; its cadence is daily and timing is flexible. GitHub Actions validates commits but cannot independently author/fix C++ without an AI agent runner and explicitly configured credentials. Do not create self-triggering infinite workflows or use secrets in source.


## 2026-10-25 duplicate shutdown checkpoint after clean marker
- Re-read `runtime-dumps/ftl_runtime_dump.txt` from main (59,546 bytes; 1,038 lines). The latest recorded session began at `2026-10-06 07:33:49` and ended with `stage=graphics_shutdown_complete`, `CLEAN_SHUTDOWN`, then an additional `stage=main_game_shutdown_begin` at the same timestamp. The preceding session ended at `ship_select_render_complete` before the 07:33:49 restart. This log is historical and does not establish current device behavior.
- Source inspection explains the trailing record: `src/main.cpp` explicitly calls `game.shutdown()`, then writes `CLEAN_SHUTDOWN`; `MainGame::~MainGame()` also calls `shutdown()`. The second shutdown emits `main_game_shutdown_begin` after the clean marker because `RuntimeDiagnostics::checkpoint()` starts logging again when `gStarted` is false. This is a reproducible diagnostic-ordering defect, not evidence of a new Vita crash.
- The repository's PSP2DMP filename timestamp is 2026-10-04 08:32:32 UTC, which does not match the 2026-10-06 runtime log; do not correlate them as the same crash without further device evidence.
- Branch `auto/idempotent-main-game-shutdown` adds an early return in `MainGame::shutdown()` when the game is already fully shut down, registers `test_main_game_shutdown`, and tests the explicit shutdown -> clean marker -> destructor sequence with a missing-archive startup-error scene. No proprietary `ftl.dat` is included.
- Source commit: `cf26ca2a383b8a9c5451557a36f16761b6a1341b`; CMake registration: `1a4832f5dba30f744864879d1f3dbf1afd857918`; regression test: `6ebf8d4d1702c45f7f8ff3cb6b1a3b9b0543cc2`. Handoff update follows in this branch. Fresh CI is pending; no PR or device validation is claimed yet.
- PR #7 remains a separate draft for previous-session classification. At its prior head `951a0f9`, Host and PS Vita build/VPK verification all passed (runs `40072090646`, `40072090643`, and `40072086063`). Follow-up exact-value assertions were committed as `cae2a14`; their new CI was still running when checked. Do not merge PR #7 until its latest checks complete and the diff is reviewed.
- Remaining device blocker: verify whether the sector-map/render-loop stall reproduces on physical Vita. If it does, capture a fresh runtime log and matching `.psp2dmp`; the October 6 log is too old to prove the current build's behavior.


## 2026-10-27 post-clean shutdown diagnostic guard
- Re-read the main-branch runtime log (59,546 bytes, 1,038 lines). The 2026-10-06 session ends with `graphics_shutdown_complete`, then `CLEAN_SHUTDOWN`, then a late `main_game_shutdown_begin` checkpoint. PR #8 already makes the duplicate `MainGame::shutdown()` call a no-op, but source review found the logger itself still accepted any late checkpoint after a clean marker and set `gStarted` back to true.
- Extended PR #8 branch `auto/idempotent-main-game-shutdown`: `RuntimeDiagnostics` now ignores checkpoints after `markCleanShutdown()` until a new `startSession()` succeeds. This is a defensive logger-level invariant in addition to the existing idempotent game shutdown.
- Extended `tests/test_runtime_diagnostics.cpp` to simulate a late destructor checkpoint after clean shutdown, assert it is absent and that the log still ends at `CLEAN_SHUTDOWN`, then start a second session and verify checkpoints are accepted again.
- Source and regression-test changes were committed on PR #8's existing branch; fresh Host and PS Vita CI is pending. No merge or physical-device validation was performed. The old October 6 log is historical and does not establish whether the sector-map/render-loop stall persists.
- Remaining device blocker: run the latest PR #8 VPK on a physical Vita through the sector map; if it stalls, collect a fresh `ux0:data/wormhole/ftl_runtime_dump.txt` and the matching `.psp2dmp` from `ux0:data`. No `ftl.dat` was added.
