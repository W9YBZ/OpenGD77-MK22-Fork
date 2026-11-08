# GD-77 GNU release build and CPS firmware file

The GNU release links, checks the BIN and automatically produces the CPS `.zip`
in one invocation of `--link`. The previous 8f64d48 image failed hardware boot
with **FLASH MEM ERROR**, reported by the user; retain it for diagnosis, not installation.
The new SPI timing/ID-diagnostic candidate requires hardware retest. RF decoding
and CPS roundtrip remain **Not Run**. MDC remains Off in settings by default.

## Reproduce in this workspace

```sh
PATH=/tmp/gd77-toolchain/root/usr/bin:$PATH python3 tools/build_gd77.py \
  --cc /tmp/gd77-toolchain/root/usr/bin/arm-none-eabi-gcc \
  --newlib /tmp/gd77-toolchain/root/usr \
  --out artifacts/gd77-flash-fix \
  --experimental-mdc-rf --lto --link \
  --donor-zip '/home/yuhui/Downloads/2021-01-26 GD-77 CPS & Firmware Changelog - Ham Version.zip'
```

No second packaging command is needed. `--link` succeeds only after the ZIP
CRC, BIN and matching languages have passed validation. Stale ZIP/manifest files
are removed before a new link build, so a failed build cannot leave a previous
package looking current. The standalone packager remains available for re-packing
an existing checked build: `python3 tools/package_gd77_zip.py --artifacts DIR`.

Select `artifacts/gd77-flash-fix/OpenGD77-MDC1200-EXP.zip` in CPS. Its root
contains exactly one `OpenGD77.bin` plus the 18 matching `.gla` files produced
by the repository language builder, following `package_bin_with_languages/package.bat`.
English is embedded in the BIN. The BIN SHA256 is unchanged by packaging; all
ZIP member CRCs and language headers/version/size are checked. The donor remains
outside the firmware ZIP and is selected separately in CPS.

The raw `artifacts/gd77-flash-fix/OpenGD77-MDC1200-EXP.bin` remains alongside ELF, map,
commands, layout verification, SHA256 manifest, README and `GD-77_V4.3.6.sgl`.
The output directory is Git-ignored; no donor or generated binary belongs in Git.
The supplied donor ZIP is read-only; only its exact V4.3.6 member is extracted, with ZIP CRC,
length, SGL signature and the supplied member's SHA256 checked. The other stock
versions, CPS installers and frequency-unlock file are not used.

Use the existing **OpenGD77 CPS E2026.05.26.01 → Extras → Firmware Loader**,
select **GD-77**, choose `GD-77_V4.3.6.sgl` as the **donor**, and the generated
`OpenGD77-MDC1200-EXP.zip` as the OpenGD77 firmware package. CPS performs the AMBE merge
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

## FLASH MEM ERROR investigation and retest

The original binary reached the boot failure branch because `SPI_Flash_init()`
rejected its external flash part ID. This is before calibration initialization
and before MDC TX. It is not evidence of erased calibration or a missing donor.
The previous bitbang driver sampled MISO during the low phase with no explicit
settling interval, relying on compiler scheduling. ARM disassembly confirms it
read immediately after the MOSI write. This is a timing risk; the actual failing
radio ID/waveform is not yet available, so hardware causation is unconfirmed.

The candidate uses explicit mode-0 edges, samples during the high phase, and
adds 16 compiler-proof NOPs to each half-cycle (at least 133 ns at 120 MHz), plus
CS setup/hold delays. CS is reclaimed high before asserting it. The basis is the
[Winbond W25Q80BV datasheet rev C1](https://www.winbond.com/upload/technical-support/f0f72951-b845-42ea-9010-faaeab26872f.pdf),
sections 6.1.1 and 8.6–8.9 (Read ID fall-to-output-valid maximum 8 ns).
No ID is newly accepted and no erase/reset/write command is added. Failure now
shows `FLASH ERR XXXX`, preserving the rejected part ID for diagnosis.

`test_spi_flash.c` includes the actual driver and models delayed MISO, mode-0
edges, setup/hold and a complete 0x9f transaction. It checks five accepted and
three rejected IDs, manufacturer readback and absence of write/erase commands.
This is a transport model, not a measurement of the radio or a flash-write test.
The slower shared transport must still be checked for UI/scan/voice performance.

On the candidate, verify cold power-on and repeated restarts. If failure persists,
record the exact `FLASH ERR XXXX` and whether a full power cycle changes it.
Do not clear Codeplug/settings or restore calibration to address this ID error.
If bench access is available, capture CS=A19, CLK=E5, MOSI=E4, MISO=E6, decode
mode 0 / MSB first and compare the response to 9f with the displayed ID. Only
normal boot plus CPS readback/backup comparison can close hardware acceptance.
