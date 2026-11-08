# GD-77 R20260131 feature migration

Baseline: `72825f2`, branch `feature/gd77-ptt-talk-permit-mdc1200-v2`.
User instruction markdown was untracked on entry and remains untouched/uncommitted.
Historical UI reference: W9YBZ/OpenGD77 `57aed1c76ae4fd95363ec36e9c2d59c591f5bc8b`, read-only clone outside this repository.
No old commits, whole driver files or old MDC encoder are imported.

## Findings and implementation order

1. Commit architecture/protocol evidence before implementation.
2. Keep existing settings ABI and menu indices. Append GD-77-only menu entries;
   use existing confirmation/cancel/QuickKey mechanism. Add int16_t local melody.
3. Add nonblocking 150 ms FM RF tail before existing APRS/TX termination chain.
4. Independently test pure MDC framing and a sample-based physical layer on host;
   leave RF MDC disabled until the physical route and external decode are proven.
5. Record reproducible tests and hardware acceptance matrix.

FM `trxSetTX()` enables transmission and synchronously calls `trxActivateTx()`;
`txPAEnabled` is the FM PA flag; `trxIsTransmitting` is primarily DMR state.
Register 0x30 TX enable readback is required for new FM effects. Exclude VOX-enabled
sessions conservatively, satellites, APRS PTT mode, errors and TOT. Retain release
latch until cleanup. All new behavior is PLATFORM_GD77 only.
Local melody uses HR-C6000 -> speaker mux, not AT1846 tone selection. Gate MIC
modulation while playing to prevent acoustic retransmission. Existing sound
arbitration, silent/no-beep policy and DMR melody remain in force.

## Storage proof (before writes are implemented)

Actual headers compiled on host with PLATFORM_GD77, CPU_MK22FN512VLL12,
SDK_OS_FREE_RTOS, HAS_GPS from main.h: sizeof(settingsStruct_t)=116,
UNUSED_1 offset=60, UNUSED_2=62, roaming=108, lastTalkerOnScreenTimer=113.
No pointer/long members; fixed-width fields and 32-bit enum alignment. Target
compile-time assertions must enforce these values (including enum ABI).
Later ARM verification: default AAPCS roaming enum is 1 byte, structure 112,
lastTalker offset 108; host/`-fno-short-enums` enum is 4 bytes, structure 116,
lastTalker offset 113. All 57 member offsets/sizes match baseline under both
ABIs (see validation/settings-abi.log). Both slot offsets remain 60/62.
The GNU build recipe defines HAS_GPS globally to match main.h and avoid
translation-unit-dependent structure definitions; it does not change enum ABI.
The earlier host result alone was not sufficient evidence of target size.

Settings occupy [0x604B,0x60BB) for AAPCS, [0x604B,0x60BF) for full enums; slot bytes [0x6087,0x608B) already belong to
that exact settings write. This allocates no EEPROM/Flash address. Repository
search of UNUSED_1/2 found only declarations and default-zero assignments in
settings.c; similarly named Codeplug fields are separate structures.
No base field is renamed/reordered/extended. Keep magic 0x4784. Encode ID in
UNUSED_1; UNUSED_2 = tag/version 0xA8 | mode (bits 0..1) | permit (bit 2),
plus CRC-8/ATM in high byte over ID little endian and flags. Reject zero ID,
invalid mode/tag/CRC. Invalid old slot loads Off/On/nonzero low16(DMR ID) in
RAM; persist only through existing settings save, never on load. An explicit
menu edit creates the record; validated ID never tracks later DMR ID changes.
Old firmware ignores slots; restoring official defaults zeroes them and loses
only these features. No power-fail atomicity is claimed: CRC rejects torn slot
updates; existing settings block has its original write semantics.

## Build and hardware boundary

No .project/.cproject, top-level Makefile or source manifest is tracked.
arm-none-eabi-gcc is absent; only host gcc found initially. Further inspection found codec_cleaner.Linux
present without executable permission; placeholder sections were initially absent.
A copied cleaner ran with -C only in /tmp; no donor was acquired. The toolchain was subsequently extracted into /tmp. All 128 ARM units compile;
full newlib link still fails for both baseline and feature at the fixed codec
boundary. Final linked space numbers and radio behavior remain Blocked/Not Run.
Add reproducible syntax checks using actual GD-77 headers and host tests;
these do not constitute an ARM firmware build. Do not flash, reset storage,
push or change main. Restricted AMBE donor is never fetched or committed.

## MDC risk

User reports actual legacy RF does not decode. Root cause remains unproven.
Old preamble/sync/opcode/CRC/FEC/bit order/timing are not evidence. Independent
reference and golden generation precede new encoder; no RF implementation is
accepted merely because it compiles. Global PIT stays at 1 ms. Per-symbol I2C
switching is rejected without timing/phase evidence. See protocol document.
