# PTT re-key repair / diagnostic candidate

## Hardware failure reported

The first PTT Beep ZIP (`c31e11843eb79036acdc044547b01a2b88ec9a7637da3b38518eb32b91faa6e9`)
failed user acceptance: PTT Tone=Beep, first TX works but the receiver hears no
tail. Later PTT produces an error beep, without RF or the red LED. Switching
PTT Tone to Off afterwards does not restore repeated TX. This does not establish
whether Off from a fresh boot works. Do not use that image as a stable build.

## Proven software problem and remaining uncertainty

`radioWriteReg2byte()` records a requested value in its register cache even when
`I2C_MasterTransferBlocking()` returns an error. A subsequent write of that same
value returns success from the cache without another physical transfer. Thus a
failed restore can keep the selector in the tone route while retries claim
success; physical readback keeps failing and the retained snapshot prevents the
next TX entry. Merely switching the PTT mode cannot prove a safe MIC restore.

The new host test includes the actual driver source, injects an I2C failure and
proves that a second native write performs zero bus transfers while claiming
success. This is a verified software defect and a failure path matching the
persistent TX block. **The user's initial failed register/transaction and RF
cause are still unconfirmed.** Without radio readback, do not claim this is the
entire hardware root cause or that the tone is now audible.

## Changes

- `firmware/source/hardware/AT1846S.c` and its header: add GD77-only
  `radioWriteReg2byteFresh()`. It rejects busy/bank-register requests, bypasses
  the cache and invalidates failed writes. The existing native implementation
  is left unchanged; the archive audit strips the new helper and requires the
  remainder to match the official driver exactly.
- `firmware/source/user_interface/uiTxScreen.c`: checked FM audio writes now use
  the fresh helper. MIC and PTT snapshot restores retry once, with physical
  readback, rather than trusting a cache hit. Persistent failures still turn
  off PA and keep snapshot ownership; they are not silently discarded.
- `firmware/include/user_interface/menuSystem.h` and
  `firmware/source/user_interface/menuSoundOptions.c`: a sticky runtime error
  code in the existing PTT Tone menu identifies the last failed step/register.
  No log is printed in an interrupt/audio sample and no CPS command is added.
- Tests exercise the actual driver, transient recovery, acknowledged writes with
  wrong physical readback, persistent failures, and 100 consecutive Off plus
  100 consecutive Beep sessions without resetting registers/cache each time.

The stable Flash driver, original sound task/backend, settings layout/storage,
Codeplug, calibration data, CPS protocol, global PIT and MDC code are unchanged.
No automatic flash, reset or data erase. User-confirmed PTTFix remains the
known working local-Talk-Permit rollback candidate.

## Diagnostic display

Normally the menu still shows Off/Beep/MDC N/A. After a failure it shows e.g.
`B E3A/4` in the value: B=Beep, O=Off, M=reserved MDC; 3A=hex register; final
number is the step. It records the latest failure and remains until power-cycle,
even if a retry subsequently succeeds. The prefix keeps the selected mode
visible and left/right still changes it normally.

| Step | Meaning |
| --- | --- |
| 1 | I2C bus already owned |
| 2 | Physical write failed |
| 3 | Physical read failed |
| 4 | Write acknowledged but full register readback mismatched |

A persistent mismatch is retained as a fault, including reserved/volatile-bit
possibilities; no guessed comparison mask suppresses it. The code provides
evidence for the next investigation if this candidate still fails.

## Software validation and build

UBSan and host O2/LTO tests, four-platform syntax, pinned archive audit and
57-field target settings ABI comparison: PASS. The native GD77 image uses the
unchanged MCUXpresso 24.12.148 / Redlib preset without LTO. Link: 0 errors and 4
pre-existing official sound-worker warnings. Image/vector/IRQ/codec/RAM checks,
corruption rejection and CPS ZIP CRC/language/donor checks: PASS.

Host tests use fake I2C/registers/clock and helper-level UI control. They do not
simulate the RF chip, scheduler, complete UI or actual RF modulation.

```sh
python3 tools/build_native_gd77.py --out artifacts/native-ptt-rekey-fix \
  --name OpenGD77-TalkPermit-PTTBeep-RekeyFix \
  --donor-zip '/home/yuhui/Downloads/2021-01-26 GD-77 CPS & Firmware Changelog - Ham Version.zip'
```

| Item | Result |
| --- | --- |
| Source commit | `50eb7cd6c4866ada8ed2d3dbb90c368316079483` |
| ZIP | `artifacts/native-ptt-rekey-fix/OpenGD77-TalkPermit-PTTBeep-RekeyFix.zip` |
| ZIP bytes | 229415 |
| ZIP SHA256 | `b08f4a40c8927ec241990293f74b277023802a12ca49b57911a2c0a6ddc42346` |
| BIN bytes | 502768 |
| BIN SHA256 | `6aee066a0b6b50633cc58cc1f2e72b6dcc04b8c6a12145d2c397e53dc3de9c29` |
| Pre-codec free space | 1784 bytes |
| Upload headroom | 1040 bytes |

Select the donor separately in CPS. Neither donor nor generated images are in
Git. Evidence is in [the native manifest](validation/ptt-rekey/native/manifest.json),
[host tests](validation/ptt-rekey/native/host-tests.log),
[build log](validation/ptt-rekey/native/build.log),
[image mutations](validation/ptt-rekey/native/image-mutations.log),
[ABI log](validation/ptt-rekey/native/settings-abi.log) and
[official audit](validation/ptt-rekey/native/official-diff.log).

## Manual acceptance — Not Run on this candidate

1. Manually load this ZIP via CPS with the separate donor; retain user-confirmed
   PTTFix for rollback. Check boot, key beeps, Talk Permit and FM speech.
2. First test PTT Tone=Off across several independent manual FM transmissions.
3. Then set Beep and check repeated TX and one tail per release using an
   independent receiver. Measure nominal 1000 Hz / 150 ms from recorded audio.
4. If the first release or next TX fails, open Sound Options and record the
   exact PTT Tone fault value (e.g. `B E79/4`) before power-cycling. Also note
   whether Off worked before the Beep attempt. Do not infer RF success from a
   local beep. Keep persistent-failure protection enabled.
5. Check rapid re-press, short PTT, TOT and 1750/DTMF interactions, setting
   persistence, CPS read/write and Talk Permit RF isolation as available. Any
   untested item remains Not Run; DMR/VOX/APRS/satellite hardware regression also
   remains Not Run.
