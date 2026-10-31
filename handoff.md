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
- Source commit: `12653576fb375a888da17d7475e0d1d84c8f8a73`; regression-test commit: `6e863574693da682d67d84f92f062525dd73967d`; handoff commit at the validated head: `5c92d7f7a76b93f7da68936318235c48b6010615`. PR: [#8](https://github.com/teststs1111/FTL-for-VITA/pull/8), still open as a draft and not merged.
- Validation for head `5c92d7f7a76b93f7da68936318235c48b6010615`: Host build run `40462456842` passed; combined run `40462456826` passed both Host regression tests and PS Vita build, including `Verify VPK` and `Upload VPK`. The code-and-test revision also passed all 16 Host tests in run `40462450303`. These results validate compilation/tests and artifact creation only, not physical Vita behavior.
- No merge or physical-device validation was performed. The old October 6 log is historical and does not establish whether the sector-map/render-loop stall persists.
- Remaining device blocker: run the latest PR #8 VPK on a physical Vita through the sector map; if it stalls, collect a fresh `ux0:data/wormhole/ftl_runtime_dump.txt` and the matching `.psp2dmp` from `ux0:data`. No `ftl.dat` was added.


## 2026-10-29 periodic main-loop phase diagnostics
- Re-read the current main runtime dump (59,546 bytes, 1,038 lines). In the 2026-10-06 07:32:34 session, the user selected New Game at 07:33:14; the last recorded update heartbeat was `scene=1` at 07:33:28. Ship-selection render checkpoints then appear at 07:33:42–07:33:44, and a new process session begins at 07:33:49 without a clean-shutdown marker for the prior session. This historical evidence does not prove whether the process was force-restarted or crashed, nor does it identify a current-build defect.
- Source review found that the periodic `heartbeat` is emitted inside `ShipScene::update()` before scene-specific update logic. The outer `MainGame::update()` and `MainGame::render()` wrappers had no paired completion checkpoints; the only begin/complete markers were for the first frame. A future dump therefore could not reliably distinguish a stall inside update from one inside render.
- Extended PR #8 branch `auto/idempotent-main-game-shutdown` with low-frequency phase markers in `MainGame::update()` and `MainGame::render()`: begin and completion are recorded every 300 calls (nominally five seconds at 60 Hz), with matching frame numbers. This adds four records per five seconds, avoiding per-frame file I/O.
- Extended `tests/test_main_game_shutdown.cpp` to run 301 update/render cycles through a missing-archive test scene and assert the exact ordered marker pairs at frames 0 and 300, while retaining the existing idempotent-shutdown assertions.
- Changes are committed together with this handoff update. Fresh Host and Vita/integrated CI must complete before this instrumentation is considered build-validated. No physical Vita result is inferred. PR #8 remains a draft and unmerged; PR #7 and PR #9 are separate branches and were not modified. No `ftl.dat` was added.
- Next device evidence: after running a fresh VPK, collect the runtime dump if the sector-map stall recurs. At a sampled frame, `main_update_begin` without matching `main_update_complete` narrows the stall to update/audio; `main_render_begin` without matching `main_render_complete` narrows it to rendering. These markers are sampled every 300 calls, so a freeze between sample points may remain ambiguous; they are low-overhead breadcrumbs, not a watchdog. If both pairs continue, investigate input/state progression rather than a blocked phase.


- Validation follow-up: code/test commit `43361d06b23dd1f5b1f616e7c01abc7115c3a22a` completed push-triggered integrated Actions run [40832559931](https://github.com/teststs1111/FTL-for-VITA/actions/runs/40832559931) successfully on 2026-10-29. Host regression build/tests passed; PS Vita build, VPK verification, and artifact upload also passed. This validates compilation, tests, and packaging only, not physical Vita execution. The branch remains PR #8 draft and unmerged.


## 2026-10-29 sampled update subphase diagnostics
- The historical 2026-10-06 dump establishes that the Ship Select/render activity was followed by a process restart without a clean-shutdown marker, but it does not prove whether the current build still stalls. PR #8 already added sampled outer `MainGame::update()` and `MainGame::render()` boundaries. A missing `main_update_complete` could still mean either `state_->update()` or `audio_.update()` did not return.
- Added sampled `main_scene_update_begin/complete` markers around `state_->update(dt)` and `main_audio_update_begin/complete` around `audio_.update()`, using the existing every-300-update cadence. The paired outer update and render markers remain unchanged. At a nominal 60 Hz this adds four extra records every five seconds (eight total phase records across update and render), not per-frame logging.
- Extended `tests/test_main_game_shutdown.cpp` to assert the exact ordered scene/audio/outer update markers at sample frames 0 and 300, in addition to the existing render and idempotent-shutdown checks.
- Source and regression-test commit: `02faf05eebbe161b3708de72779e28c35f700a85`. PR: [#8](https://github.com/teststs1111/FTL-for-VITA/pull/8), still a draft and unmerged.
- Standalone Host build/test run [40835436529](https://github.com/teststs1111/FTL-for-VITA/actions/runs/40835436529) passed on this revision. Combined Host/Vita run [40835436231](https://github.com/teststs1111/FTL-for-VITA/actions/runs/40835436231) was still in progress at the time of this entry; verify its final conclusion before treating Vita packaging as passed.
- Remaining blocker: run a fresh PR #8 VPK on a physical Vita through New Game and the sector map. If it stalls, inspect the last sampled pair: missing scene completion points into scene update; scene complete followed by missing audio completion points into audio update; both subphases complete but outer update missing points to code after audio or logging. A matching fresh `ftl_runtime_dump.txt` and `.psp2dmp` are still required for native crash diagnosis. No device success is claimed; no `ftl.dat` was added.


- Validation follow-up: source/test commit `02faf05eebbe161b3708de72779e28c35f700a85` completed combined Host regression and PS Vita build run [40835436231](https://github.com/teststs1111/FTL-for-VITA/actions/runs/40835436231) successfully on 2026-10-29. Both jobs passed; the Vita job completed `Build Vita VPK`, `Verify VPK`, and `Upload VPK`. Separate Host run [40835436529](https://github.com/teststs1111/FTL-for-VITA/actions/runs/40835436529) also passed. These results validate the code/test revision and packaging, not real-device execution. After the source/test revision, subsequent commits updated only `handoff.md`; no source or test code changed after `02faf05eebbe161b3708de72779e28c35f700a85`. The documentation-only pushes triggered additional Actions runs; these do not replace the successful Host/Vita validation already recorded above. PR #8 remains a draft and unmerged.



## 2026-10-30 integration of overlapping runtime-diagnostic PRs
- Fresh repository inspection confirmed that PR #7 (`auto/previous-session-crash-detection`) and PR #8 (`auto/idempotent-main-game-shutdown`) both modify `src/platform/runtime_diagnostics.cpp` and `tests/test_runtime_diagnostics.cpp`. The PR #8 version had the clean-shutdown guard but did not include PR #7's persisted previous-session classifier. Merging the two drafts independently could therefore lose one of the protections.
- Created integration branch `auto/integrated-runtime-diagnostics` from the latest PR #8 head `3d4c15b2b80ac5385bc901f1dc242c67777e5630`. Combined the bounded 64 KiB previous-session scan with the `gCleanShutdown` checkpoint guard. `startSession()` now records the persisted previous-session status and only re-enables logging after the new log file opens successfully.
- Combined regression test covers: initial `unknown`; a prior `clean` session; an unclean session followed by a new start; a newer clean session superseding the older unclean session; history older than the 64 KiB scan window returning `unknown`; late destructor checkpoints being ignored after `CLEAN_SHUTDOWN`; and one-time primitive draw checkpoint suppression.
- PR #8's existing MainGame idempotent shutdown and sampled update/scene/audio/render phase markers remain intact. No main-branch files were changed.
- Commits: combined runtime diagnostic implementation `b6aa278e64d6a4399d691662c0b7ff367dde372e`; combined regression test `021e106f3c8dc059c9305e7010d8b0fd751bd309`.
- Integration review PR: [#10](https://github.com/teststs1111/FTL-for-VITA/pull/10), draft and unmerged. Await Host regression and PS Vita build/VPK verification on this exact combined head before treating it as validated. PRs #7 and #8 remain open drafts; do not merge their overlapping diagnostic files independently without preserving the combined behavior.
- Remaining hardware blocker is unchanged: run the fresh combined PR #10 VPK on a physical Vita through New Game and the sector map. If it stalls, collect a fresh matching `ftl_runtime_dump.txt` and `.psp2dmp`. CI does not prove physical-device startup or sector-map success. No `ftl.dat` was added.



- Validation follow-up (2026-10-30): combined source/test head `021e106f3c8dc059c9305e7010d8b0fd751bd309` completed integrated Actions run [41016519407](https://github.com/teststs1111/FTL-for-VITA/actions/runs/41016519407) successfully. Host regression tests passed; the PS Vita job completed `Build Vita VPK`, `Verify VPK`, and `Upload VPK`. The non-expired artifact was `FTL-for-VITA-vpk` (1,482,422 bytes). Separate Host workflow [41016533359](https://github.com/teststs1111/FTL-for-VITA/actions/runs/41016533359) also passed.
- The current PR #10 head `b8de3c3fa28c80889b3a330076dfa76c0202d721` adds only this handoff documentation after the validated source/test head; no source or test code changed. Its separate Host workflow [41016554354](https://github.com/teststs1111/FTL-for-VITA/actions/runs/41016554354) passed. The combined workflow [41016554377](https://github.com/teststs1111/FTL-for-VITA/actions/runs/41016554377) had Host pass while the documentation-only Vita job was still running at the time of this note. The successful integrated run on `021e106` is the validation evidence for the exact combined code/test revision.
- These CI results verify compilation, regression tests, VPK validation, and artifact upload only. No physical Vita startup, New Game, sector-map rendering, or input success is claimed.


## 2026-10-31 prevent per-frame first-frame diagnostic I/O
- Review of the current Vita loop in `src/main.cpp` found that `RuntimeDiagnostics::checkpoint("first_frame_render_complete")` is outside the `if (firstFrame)` guard and is therefore called on every loop iteration, despite its name and the prior handoff describing it as a one-shot milestone.
- `RuntimeDiagnostics::checkpoint()` opens the append-only dump with `std::ofstream` and flushes each accepted record. Because this stage was not included in the existing one-shot bitmask, it caused synchronous open/write/flush work on every rendered frame (nominally 60 times per second), undermining the low-I/O diagnostics design and potentially adding storage stalls to the Vita render loop.
- Added `first_frame_render_complete` to the existing per-session one-shot checkpoint mask (bit 13). This keeps the first completed-frame evidence while suppressing all later calls without changing the main loop or adding per-frame state.
- Extended `tests/test_runtime_diagnostics.cpp` to emit the stage three times and assert exactly one persisted occurrence.
- Source commit: `1460411e72ad699008f73ec8c6fbd257c6503d02`; regression-test commit: `a0e17286e432f6e4676c64c2c9c2a86ded62d172`. Changes are on `auto/integrated-runtime-diagnostics` / PR #10. Fresh Host regression and PS Vita integrated build are required; results are pending at handoff time.
- This corrects a diagnostic I/O regression, not the underlying sector-map crash. Physical Vita startup and sector-map behavior remain unverified. No `ftl.dat` was added.
