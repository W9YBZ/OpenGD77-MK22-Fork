# Native build evidence

The native baseline and Talk Permit binaries were built using the original
MCUXpresso GD-77 preset with IDE 24.12.148, bundled Arm GNU 13.2.1 and Redlib.
summary.json and per-image manifests record the exact packaged hashes.
New hardware boot/audio/CPS/RF acceptance remains NOT RUN.

Committed text logs retain all warnings/results, with trailing whitespace
normalized for Git. Raw compiler logs remain in the ignored local artifact
folders. No installer, donor, AXF/BIN or other private binary is committed.
installation.json contains only sanitized installer provenance, not the signed
download URL or its authentication query parameters.

IDE workspace logs also reported missing SDK part-support/generic cm4 fallback.
The checked active GD-77 preset kept the explicit MK22 defines, hard-float ABI,
Redlib and manual firmware.ld. project-audit.json records the one default-value
initialization performed by the IDE; image validation checks the actual memory
and interrupt/vector layout. No SDK/debug device pack or radio driver was installed.
