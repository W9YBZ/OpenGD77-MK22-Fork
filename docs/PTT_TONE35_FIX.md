# E35/4: Tone1 command / readback correction

## Evidence and limits

The user reports the RekeyFix image still fails and Sound Options shows
`PTT Tone: B E35/4`. This identifies a successful I2C write followed by a
non-equal read of register 0x35. The diagnostic records the last failed
transaction, so this alone cannot distinguish the original setup failure from
later recovery failures, nor provide the actual returned word.

The earlier cache fix addresses a separately reproduced defect, but did not
resolve the reported hardware failure. It must not be described as a verified
hardware root-cause fix.

The official native implementation provides a different contract for frequency:
`trxSetTone1()` sends Hz x 10 to 0x35 without an echo test, as does the dedicated
APRS `radioWriteTone1Reg()`. Native MK22 1750/APRS cleanup switches the voice
selector to MIC without reading/snapshotting/restoring 0x35. Each later tone
consumer initializes its own frequency before selecting Tone1.

The [AUCTUS AT1846S/AT1846SD programming guide rev. 1.4](https://www.opengd77.com/downloads/datasheets/AT1846S/AT1846S-ProgrammingGuide-1.4.pdf),
DTMF section, printed page 14, specifies the frequency command in Hz x 10.
The downloaded PDF SHA256 is
`9d7dd2c0f63530dbf33e640c1e01224bf790421a39027d9582287506f57a1587`.
It does not establish the byte-identical live echo required by our interlock.
We do **not** claim the register is proven write-only, or invent a readback
conversion/mask. Its precise live-read behavior on this radio remains unknown.
No downloaded vendor PDF is committed to Git.

The proven software error is treating an unvalidated 0x35 read value as both a
restorable frequency command and a mandatory MIC/TX release condition. A fake
non-echoing register reproduces E35/4 and retained snapshot ownership in the
previous source. This model is a counterexample to the assumption, not a
measurement or claimed XOR/conversion rule of the user's chip.

## Correction

Only `firmware/source/user_interface/uiTxScreen.c` changes in firmware:

- Frequency gets a dedicated checked command helper. It uses the previously
  added fresh physical I2C write, still rejects bus ownership/write errors,
  and never reads 0x35 to establish success, save a snapshot or release TX.
- Snapshot/restore the readable MIC selector, gain and mode registers
  (`0x3a`, `0x41`, `0x79`) with the existing strict readback and bounded retry.
  A persistent failure in these controls still disables PA and blocks TX.
- Restore the MIC/route before issuing a zero-frequency idle command. This
  feature explicitly zeros the unselected generator; native MK22 cleanup itself
  only deselects it. A previous live 0x35 read is never written back as a command.
- A failed frequency setup command aborts this TX. A failed idle command is
  diagnosed and ends this TX, but cannot retain MIC snapshot ownership after
  the critical MIC/route restoration was verified. Every future tone consumer
  programs its own frequency, as in the official native architecture.
- Talk Permit melody, sound worker, fresh-write driver helper, settings ABI,
  CPS, Codeplug, calibration, Flash driver, PIT and MDC code remain unchanged.

This is a correction of the register-validation policy, not a generic switch
to ignore readback errors. The actual RF frequency/amplitude/duration still
requires independent receiver acceptance.

## Tests

`python3 tests/talk_permit/check_e35_regression.py` compiles the actual old and
new TX helpers against the same mock register/clock fixture. Old `24e155f`:
`started=0 fault=0435 retained=1 tone_reads=4`. Current: starts successfully,
zero frequency-register reads, restores MIC and releases ownership after 150 ms.
The probe's printed retained flag is captured immediately after start, when
retention in the new code is expected; its exit assertion checks cleanup.

The main suite also verifies 100 consecutive Off and 100 consecutive Beep
sessions with a non-echoing frequency model and retained cache/register state;
actual frequency-write failures; idle-command failures without indefinite MIC
ownership; critical-control mismatch protection; transient/persistent recovery;
release/TOT/exit/re-key/wrap; Talk Permit and settings bit isolation. UBSan,
host O2/LTO and GD77/GD77S/DM1801/RD5R syntax: PASS. All 57 settings fields still
match the baseline with the bundled target compiler in both enum modes.

These are host models and source checks, not a full radio/scheduler simulation.
At build time hardware checks were Not Run. The subsequent user report confirms
repeated PTT, Beep and receiver-audible RF tail work. Measured 1000 Hz/150 ms,
deviation, DMR and CPS roundtrip remain **Not Run**; see the acceptance update below.

## Native build

```sh
python3 tools/build_native_gd77.py --out artifacts/native-ptt-tone35-fix \
  --name OpenGD77-TalkPermit-PTTBeep-Tone35Fix \
  --donor-zip '/home/yuhui/Downloads/2021-01-26 GD-77 CPS & Firmware Changelog - Ham Version.zip'
```

Native MCUXpresso 24.12.148 / Redlib GD77 preset, no LTO. Link: 0 errors, 4
pre-existing official sound-worker warnings. Image/vector/IRQ/codec/RAM,
corruption rejection, ZIP CRC, 18 languages and donor checks: PASS.

| Item | Result |
| --- | --- |
| Firmware source SHA | `9f511d2e98b8788c347254c311b7b6dfeeb19dc3` |
| ZIP | `artifacts/native-ptt-tone35-fix/OpenGD77-TalkPermit-PTTBeep-Tone35Fix.zip` |
| ZIP bytes | 229457 |
| ZIP SHA256 | `b564cb0b88fc60fbc423ac8fae4eaf5353dbdb656ada9c1d211e1e6c0501ebb2` |
| BIN bytes | 502768 |
| BIN SHA256 | `6cb76147fee6e8a50745c880b3acbdef2127b1ac52a7114720b810bc1823c9ed` |
| Pre-codec free space | 1716 bytes |
| Upload headroom | 1040 bytes |

The donor must be selected separately in CPS; it is beside the ZIP locally,
not in Git or inside the ZIP. No automatic flashing/reset/erase was performed.

Evidence: [manifest](validation/ptt-tone35/native/manifest.json),
[old/new regression](validation/ptt-tone35/native/e35-red-green.log),
[host suite](validation/ptt-tone35/native/host-tests.log),
[build](validation/ptt-tone35/native/build.log),
[ABI](validation/ptt-tone35/native/settings-abi.log),
[image mutation tests](validation/ptt-tone35/native/image-mutations.log),
[source audit](validation/ptt-tone35/native/official-diff.log).

## Manual acceptance procedure — user-confirmed subset below

1. Use this newly named ZIP with the usual separate donor. First check original
   key beep, Talk Permit and ordinary FM speech. PTTFix remains the working
   Talk Permit-only rollback candidate.
2. With PTT Tone=Off, verify several complete PTT press/release cycles.
3. With Beep, repeat while listening on an independent receiver: expect one
   nominal 1000 Hz/150 ms tail, then a normal RX return and normal next TX.
4. If it still fails, record the exact fault value before reboot and whether
   Off worked before attempting Beep. `E35/4` is no longer produced by this
   source; if it appears, verify the selected package/source is Tone35Fix.
   Bus/write faults can still show E35/1 or E35/2. Critical MIC/gain/mode errors
   retain protection and their register/step diagnostic.
5. Recheck quick taps, re-press, TOT and 1750/DTMF, saved options/CPS and
   Talk Permit RF isolation as available. Unmeasured RF parameters and
   untested special/DMR paths remain Not Run.

## Hardware acceptance update — 2026-11-09

The user reports: “连续发射Beep 和对端尾音正常”. For the unchanged Tone35Fix ZIP:

| Check | Status |
| --- | --- |
| Consecutive manual PTT transmissions | PASS — user reported |
| PTT Beep operation | PASS — user reported |
| RF tail audible at the receiving radio | PASS — user reported |
| Measured frequency, duration and deviation | Not Run |
| CPS roundtrip and saved-setting persistence on this image | Not Run |
| Talk Permit RF isolation, TOT, quick re-key, DTMF/1750 and special-path regression | Not Run |

No RF capture, repeat count or instrument measurement was supplied, so this
confirms successful operation and audibility without establishing exact waveform
parameters or every exclusion/failure path. The earlier local Talk Permit
success remains separately recorded for PTTFix. No new image is built for this
documentation update: retain Tone35Fix (ZIP SHA256
`b564cb0b88fc60fbc423ac8fae4eaf5353dbdb656ada9c1d211e1e6c0501ebb2`)
as the user-confirmed two-feature rollback reference.

The original build manifest preserves its build-time Not Run values; subsequent
hardware evidence is in [user-acceptance.json](validation/ptt-tone35/user-acceptance.json).
MDC1200 remains reserved/unimplemented in this native branch.
