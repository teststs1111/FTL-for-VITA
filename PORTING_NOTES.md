# Porting notes from Tachyon

The upstream Tachyon tree contains 224 source files below `xyz/znix/xftl`:
213 Kotlin and 11 Java.

## Completed slice

### Data layer
The C++ prototype now implements the uncompressed `ftl.dat` container format used by Tachyon:
- `PKG\\n` header
- 16-byte header
- 20-byte entries
- 3-byte name offsets
- big-endian integer fields
- uncompressed entry validation
- direct file reads

It also implements Tachyon's BXML wire format:
- document-local string table
- 7-bit unsigned varints
- element names
- attributes
- text nodes
- child elements
- end-of-element markers

### Next slices

A. VitaGL renderer backend and PNG texture loading
B. Vita input/touch mapping
C. Ship/layout/systems data model
D. InGameState and UI
E. audio/save/mod support

Desktop-only conveniences such as Swing, GLFW and developer tooling stay out of the first playable Vita build.
