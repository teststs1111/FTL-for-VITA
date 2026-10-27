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


## 2026-10-26 workflow cleanup API failure and retry handling
- Latest scheduled cleanup run `40231218859` (2026-10-25 20:32 UTC), job `121283156184`, failed while deleting older runs from several workflows. The raw job log shows `Deleting run #1462 (id 37121360185)` followed immediately by `unexpected end of JSON input` and exit code 1. This is consistent with an incomplete response while issuing that DELETE, but the log does not establish whether GitHub completed the deletion before the client/network lost or truncated the response. The old script had no retry for DELETE and hid list/JSON errors inside process substitution.
- A separate intermediate PR #8 Host run `40168724001` failed at CMake Configure because commit `1a4832f` registered `tests/test_main_game_shutdown.cpp` before that file existed. The following commit `6ebf8d4` added the test source; the final PR #8 head `451ff10` now passes both Host and PS Vita build/VPK verification (runs `40168749862` and `40168749870`). This was an ordering failure between intermediate commits, not a remaining failure at the latest PR head.
- PR #7 latest head `8de5367` passes Host and PS Vita build/VPK verification (runs `40168649159` and `40168649154`). PRs #7 and #8 remain open drafts and are not merged.
- Branch `auto/retry-workflow-run-cleanup` adds bounded exponential retries for GitHub API reads and deletes, validates JSON before jq parsing, treats a 404 after an uncertain DELETE as an already-removed run, and spaces successful deletions by one second to reduce secondary rate-limit pressure. The cleanup still retains the configured newest 30 completed runs per workflow.
- The workflow now delegates to `scripts/cleanup-old-workflow-runs.sh`; `tests/test_cleanup_old_workflow_runs.sh` exercises malformed JSON retry and the uncertain DELETE/404 path with a mocked GitHub CLI. The test is registered in the integrated Host job. The live scheduled cleanup has not yet exercised the retry path; do not run the destructive cleanup manually merely to test it. No proprietary `ftl.dat` was added.
- Runtime blocker unchanged: historical logs do not prove whether the sector-map/render-loop stall persists. A physical Vita run and a fresh matching runtime dump plus `.psp2dmp` are still required if the stall reproduces.
