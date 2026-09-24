# Porting notes from Tachyon

The upstream Tachyon tree contains 224 source files below `xyz/znix/xftl`:
213 Kotlin and 11 Java.

## Completed slices

### Data layer
The C++ prototype supports two archive layouts:

Tachyon's reconstructed `PKG\\n` container:
- 16-byte header
- 20-byte entries
- 3-byte name offsets
- big-endian integer fields
- uncompressed entry validation
- direct file reads

Vanilla FTL `.dat`:
- little-endian file-slot count
- 32-bit file offsets
- per-file 32-bit size and filename length
- UTF-8 filename followed by raw file data

The vanilla layout is independently documented by the CC0 Kaitai Struct specification and existing `ftldat` implementations.

It also implements Tachyon's BXML wire format:
- document-local string table
- 7-bit unsigned varints
- element names
- attributes
- text nodes
- child elements
- end-of-element markers

### Vita platform slice
The repository now has the first real Vita-facing runtime layer:
- VitaSDK CMake/toolchain detection
- SELF/VPK packaging definitions
- vitaGL initialization at 960x544
- double-buffered/vblank-synchronized presentation
- basic rectangle/line drawing through vitaGL
- SceCtrl digital-button polling
- Start+Select exit path for bring-up
- host-side regression tests for BXML, ftl.dat, and input edge detection
- GitHub Actions host build/test validation

The current Vita screen is intentionally a bring-up scene, not the FTL UI.

## Next slices

A. PNG/texture loading from `ftl.dat`
B. Ship/layout/systems data model
C. InGameState simulation
D. UI and touch mapping
E. audio
F. save/load and mod support

Desktop-only conveniences such as Swing, GLFW and developer tooling stay out of the first playable Vita build.
