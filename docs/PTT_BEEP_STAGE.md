# Next stage: manual FM PTT Beep

## Design boundary

Continue from user-confirmed Talk Permit PTTFix source `7f10977`, using the same
native MCUXpresso 24.12.148 / Redlib build. Preserve that package for rollback.
Only add the next feature: a nominal 1000 Hz / 150 ms RF tail on normal manual FM
PTT release. Keep the main loop responsive; do not change the global PIT.
MDC1200 encoding/modulation is outside this stage. A reserved MDC menu selection
must be visibly unavailable and emit no tail until independently validated.

The official settings have one existing 32-bit options bank, with only bits
0..23 assigned (bit 2 is used by Talk Permit). Allocate bank-0 bits 24 and 25
for PTT mode (0 Off, 1 Beep, 2 reserved MDC, 3 invalid -> Off). They are inside
the existing first 30 usable bits; no size, field, magic, address or EEPROM
allocation changes. Official defaults zero the bank, so PTT Tone defaults Off;
existing settings/menu copy/save/cancel behavior retains the new bits. Original
R20260131 ignores these bits. No background migration or reset is required.

Arm only from a real manual PTT event on an established ordinary FM TX. Exclude
VOX-enabled, satellite, APRS-on-PTT, DMR, disallowed TX, errors and TOT. On normal
release lock out re-key until cleanup. Snapshot the tone generator/selector and
calibrated tone-gain registers, disconnect MIC, select AT1846S Tone1 (the same
RF source used by native 1750), and keep RF keyed for 150 ms. Use actual
millisecond timestamps and tick-based progress, with no blocking sleep or
per-sample output. Restore the snapshot before native TX-to-RX cleanup.

Check I2C ownership, status and physical readback. Failed setup/restore must
end RF and latch a TX error; never proceed with an unconfirmed MIC restore.
Keep ownership of failed snapshots for exit/next-entry cleanup. The official
register driver can cache failed writes: readback must detect that condition
and refuse subsequent TX rather than trusting cached success. Do not replace
the stable Flash driver or import earlier experimental audio/GNU changes.

Append the PTT menu entry after existing Talk Permit, retain existing QuickKey
indices and native save/cancel logic. Keep language-file ABI intact.

## Validation approach

Host tests use the actual feature helpers with fake registers, including
synthetic initialization vs real PTT, one tail only, 149/150 ms boundary, time
wrap, rapid re-press, TOT/errors, excluded modes, read/write/readback failures,
MIC and register restore, and settings bit isolation. Re-run Talk Permit and
57-field target ABI comparison. Build native firmware; inspect flash margin,
image and CPS ZIP; no automatic flashing.

Actual RF frequency/duration/deviation and independent receiver audibility are
Not Run until measured. Human acceptance must compare PTT Tone Off / Beep and
reserved MDC, verify no tail for excluded paths, and check CPS/settings/FM
speech/Talk Permit continue to work.

## Implementation and results

Source commit: `0c7b4409f9b3fc3858f0dcb6190d4599f25f20c4`. Firmware changes are limited to:

- `firmware/include/functions/settings.h`: GD77-only bank-0 bits 24/25 and
  bounded PTT mode getters/setters. No settings structure/storage change.
- `firmware/source/user_interface/menuSoundOptions.c`: appended PTT Tone item,
  cyclic left/right selection, existing save/cancel/QuickKey machinery. New
  title uses a string prompt; Off/Beep uses existing language entries. `MDC N/A`
  is a reserved, non-transmitting choice, not a claim of MDC functionality.
- `firmware/source/user_interface/uiTxScreen.c`: share the existing checked
  register helper with Talk Permit; one-shot tail state armed on real PTT;
  native DTMF/1750 cleanup precedes tail setup, and tail completion precedes
  native RF-off. Existing release latch prevents rapid re-key. The Tone1
  selector, frequency, calibrated gain and single-tone mode registers
  (`0x3a`, `0x35`, `0x41`, `0x79`) are saved/restored with readback. Calibrated
  tone gain follows the actual TX band. No local amp/mux, `0x57` feedback,
  CSS, power, filter or calibration storage writes are added.

The exact Talk Permit melody and backend remain unchanged. The generic helper
preserves its prior I2C checks/readback behavior; the previously fixed synthetic
initialization boundary is retained. No earlier experimental MDC, Flash driver,
GNU/newlib or audio task changes were imported.

Software checks:

- Host UBSan and host O2/LTO tests: PASS (firmware itself does not use LTO).
- Real PTT helper state tests: one-shot, 149/150 ms boundaries, time wrap,
  rapid re-press deadline, exclusions, invalid/reserved mode, Talk Permit early
  release coexistence, setup/read/readback/restore failures and cached-success
  failure detection: PASS. Tests fake the hardware registers and clock; they
  do not simulate the scheduler, modulation circuitry or the entire UI.
- Real settings helpers: Off default, range bounds, dirty flag, mode bit
  isolation and settings snapshot/cancel representation: PASS. Actual EEPROM
  persistence, menu button interaction and CPS roundtrip remain Not Run.
- GD77/GD77S/DM1801/RD5R syntax and pinned official archive difference audit:
  PASS. Native audio/Flash drivers, linker and project files remain unchanged.
- Bundled target compiler ABI comparison: all 57 settings fields, size and
  alignment match the official baseline, in both tested enum modes: PASS.
- Native GD77 build: 0 errors, 4 existing official sound-worker warnings.
- ELF/BIN vectors, IRQs, codec placeholders, RAM/flash layout, corrupted-image
  rejection, ZIP CRC, 18 language files and donor identity: PASS.

Build command:

```sh
python3 tools/build_native_gd77.py --out artifacts/native-ptt-beep \
  --name OpenGD77-TalkPermit-PTTBeep \
  --donor-zip '/home/yuhui/Downloads/2021-01-26 GD-77 CPS & Firmware Changelog - Ham Version.zip'
```

Artifact: `artifacts/native-ptt-beep/OpenGD77-TalkPermit-PTTBeep.zip`

| Item | Result |
| --- | --- |
| Source SHA | `0c7b4409f9b3fc3858f0dcb6190d4599f25f20c4` |
| ZIP bytes | 229246 |
| ZIP SHA256 | `c31e11843eb79036acdc044547b01a2b88ec9a7637da3b38518eb32b91faa6e9` |
| BIN bytes | 502768 |
| BIN SHA256 | `59a4bbe161981cc1b583d05c6af535b3dc086319aedf049a222deb99f3b07130` |
| Pre-codec free space | 1976 bytes (PTTFix: 2836; cost: 860 bytes) |
| Upload headroom | 1040 bytes |

CPS must select `GD-77_V4.3.6.sgl` separately as donor before loading the ZIP.
The donor copy is beside the ZIP locally and is not inside the ZIP or Git.
No automatic radio flashing, EEPROM reset, Codeplug or calibration erase.

Evidence: [manifest](validation/ptt-beep/native/manifest.json),
[host tests](validation/ptt-beep/native/host-tests.log),
[native build](validation/ptt-beep/native/build.log),
[ABI comparison](validation/ptt-beep/native/settings-abi.log),
[image mutation checks](validation/ptt-beep/native/image-mutations.log),
[official archive audit](validation/ptt-beep/native/official-diff.log).

## Human acceptance — all Not Run for the new PTT Beep image

1. Flash manually through CPS with the separate donor. Verify boot, key beeps,
   Talk Permit, normal FM RX/TX speech and CPS read/write. Do not reset settings.
2. On an allowed ordinary FM test channel, disable VOX/APRS-on-PTT; use a legal
   frequency and suitable RF setup. With PTT Tone=Off, release manual PTT:
   expect immediate native TX-to-RX and no added RF tail.
3. Set PTT Tone=Beep and save. Use an independent receiver to confirm one RF
   tone at release. Capture its audio to measure approximately 1000 Hz and
   150 ms, and check modulation/deviation. A local speaker sound alone is not
   a pass. Repeat on VHF/UHF and narrow/wide FM as available.
4. Check short PTT taps (including release during Talk Permit), repeated
   transmissions, rapid release/re-press, and release after keypad/1750 use.
   Expect one tail at most, no stuck PA/UI, and normal MIC speech on the next TX.
5. Verify TOT and TX-error/RX-only/disallowed-TX paths produce no extra tail.
   Test VOX, APRS and satellite exclusions only where the setup is available;
   untested paths remain Not Run. DMR behavior also needs hardware regression.
6. Check PTT Tone save/reboot persistence and menu cancel restoration; it must
   not alter Talk Permit or other sound options. `MDC N/A` must add no RF tail.
7. Recheck Talk Permit on an independent receiver: the three local start notes
   should not leak into RF. The prior user success confirmed local playback,
   not this independent RF-isolation measurement.

If hardware regression appears, use the retained user-confirmed PTTFix ZIP
(with the same donor) to isolate this stage; published official R20260131 is
the known original fallback. No image checker or host test proves RF success.

## Hardware feedback / superseding repair

The user reports this original PTT Beep candidate failed: Beep selected, no
received tail, and later PTT blocked (red TX LED off / error beep). Selecting
Off after the fault did not restore repeated TX. Its software checks remain
valid, but RF audibility and repeated-TX acceptance are **FAIL (user reported)**.
See [re-key repair and diagnostics](PTT_REKEY_FIX.md) for the new candidate;
its actual hardware behavior is Not Run.
