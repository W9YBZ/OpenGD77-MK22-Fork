# GD-77 GNU release build and CPS firmware file

The GNU release now links and exports a checked `.bin`. The earlier link-failure
logs are retained as history. RF decoding, hardware boot and CPS roundtrip remain
**Not Run**. The MDC sender is experimental and remains Off in settings by default.

## Reproduce in this workspace

```sh
PATH=/tmp/gd77-toolchain/root/usr/bin:$PATH python3 tools/build_gd77.py \
  --cc /tmp/gd77-toolchain/root/usr/bin/arm-none-eabi-gcc \
  --newlib /tmp/gd77-toolchain/root/usr \
  --out artifacts/gd77-mdc1200-exp \
  --experimental-mdc-rf --lto --link \
  --donor-zip '/home/yuhui/Downloads/2021-01-26 GD-77 CPS & Firmware Changelog - Ham Version.zip'
```

Output is `artifacts/gd77-mdc1200-exp/OpenGD77-MDC1200-EXP.bin` plus ELF, map,
commands, layout verification, SHA256 manifest, README and `GD-77_V4.3.6.sgl`.
The output directory is Git-ignored; no donor or generated binary belongs in Git.
The ZIP is read-only; only its exact V4.3.6 member is extracted, with ZIP CRC,
length, SGL signature and the supplied member's SHA256 checked. The other stock
versions, CPS installers and frequency-unlock file are not used.

Use the existing **OpenGD77 CPS E2026.05.26.01 → Extras → Firmware Loader**,
select **GD-77**, choose `GD-77_V4.3.6.sgl` as the **donor**, and the generated
`OpenGD77-MDC1200-EXP.bin` as the OpenGD77 firmware. CPS performs the AMBE merge
before upload. Do not choose the stock donor as the firmware to flash. The `.bin`
retains empty codec slots and the ELF is a debug artifact, not a donor-merged SWD
image. No flashing tool is invoked by the build.

The V4.3.6 donor/CPS merge workflow is documented in the
[OpenGD77 user guide](https://github.com/LibreDMR/OpenGD77_UserGuide/blob/master/OpenGD77_User_Guide.md).
This task does not change the CPS protocol, bootloader, EEPROM/Codeplug layout or
calibration content. Generated builds and donor material are local artifacts;
combined-distribution licensing is not resolved by compiling this experiment.

## Root causes and fixes

* `-nostartfiles` still allowed GCC to add full libc automatically, despite the
  linker GROUP asking for newlib-nano. `-nostdlib` now uses only that explicit
  library group; compile-time headers also select nano's reentrancy layout.
* Release flags use `-Os -DNDEBUG` and explicit `--lto`. Startup is compiled without
  LTO so SystemInit/main cannot be inlined into the 0x4000..0x4400 startup window.
  Calibration and calibration-menu units are also kept outside LTO so the native
  `firmware.ld` policy of putting their code after codec2 can be honored.
* `firmware_newlib.ld` now explicitly places `.upper_text`, calibration code and
  ARM exception metadata in the existing post-codec window. Codec addresses remain
  **0x4400** (0xF00 bytes) and **0x54000** (0x28090 bytes). No fixed boundary,
  bootloader or memory size was moved to hide an overlap.
* The codec assembly sections now explicitly carry the ALLOC flag. Without it,
  absolute output addresses alone do not reserve loadable flash; a superficially
  successful link could still place application code over the codec addresses.
  Linker ASSERTs and the post-link checker reject that condition.
* The reserved 4 KB stack is represented as a real NOLOAD reservation. A GNU-only
  bounded `_sbrk` uses the existing reserved 4 KB heap, instead of libnosys starting
  allocation after the upper-bank BSS without enforcing the intended heap limit.
  Firmware source/layout/settings interfaces otherwise retain their baseline ABI.

## Validation gates

`check_gd77_image.py` parses actual ELF32 program/section headers and symbols,
checks ARM hard-float ABI, vector/SP/reset entry, ALLOC/load addresses, codec slot
content/size/address, every flash/RAM interval, heap/stack separation, upload end
before 0x7F000, and byte-for-byte ELF → BIN contents. Both codec slots must still
be FF placeholders for the standard CPS donor-merge workflow. A BIN becomes the
final output filename only after these checks pass.

The experimental IRQ handlers must appear in the actual vector table.
`mdcRFDiagnostics` is retained explicitly for the RF bench debugger despite LTO.
Linker assertions fail before export if a fixed region or stack is exceeded.
`test_image_check.py` verifies rejection of corrupted vectors/data/codec bytes,
truncation, appended content, missing ALLOC flags and codec overlap.
`test_newlib_heap.c` checks OOM, shrinking and extreme signed increments with UBSan.

All compiler flags and per-file LTO exceptions are in `commands.json`; command
`i` is `common + jobs[i]`. Source SHA/dirty status and `SOURCE_DATE_EPOCH` are in
`manifest.json`. GCC build-date/time macros use the commit epoch for repeatability.
MDC-on and MDC-off builds have both been fully linked and checked. Successful
linking and image checks do not replace hardware acceptance in
[MDC1200_RF_EXPERIMENT.md](MDC1200_RF_EXPERIMENT.md).
