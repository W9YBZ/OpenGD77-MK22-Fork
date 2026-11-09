# Talk Permit PTT trigger fix — 2026-11-09

User feedback on the previous native Talk Permit package: original key beeps
work, but enabling Talk Permit produces no tone. This confirms that package's
basic local beep path works; DMR, MIC gating and RF isolation are not thereby
validated.

## Proven software cause

`menuSystemPushMenuFirstRun()` constructs a synthetic event with `buttons=0`
and passes it to `menuTxScreen(..., true)`. The previous implementation called
`talkPermitStart()` from this initialization phase, but its first guard required
`BUTTON_PTT`. Thus normal menu entry always returned without starting a melody.
The old host tests bypassed the menu boundary and supplied PTT as already held.

The TX first-run now only arms a one-shot request after analog TX setup. The
first regular menu tick consumes it using main's real PTT state. A released PTT
consumes the request without playback; there is no later delayed/repeated tone.
The PTT-down event itself must not cancel playback. Only keypad/SK2 activity
preempts it before existing DTMF/1750 handling; PTT release, TOT, exit, errors
and the 250 ms deadline still restore the microphone.

The added regression exercises synthetic entry (no PTT), real PTT-down with
`hasEvent=true`, exactly-once playback, key/SK2 preemption, release-before-start
and exit cancellation. Source boundary assertions check the actual official
menu constructor and the TX initialization/regular-tick placement. These are
host tests with fake radio registers, not a radio or scheduler simulation.

## Scope and validation

Only `uiTxScreen.c` changes in firmware for this fix. Official sound workers,
SPI Flash driver, linker and managed build options remain unchanged. Settings
layout is unchanged (all 57 fields compared using the bundled target compiler,
both enum modes). The existing melody, microphone snapshot/restore, local PCM
route, option storage, CPS, Codeplug and calibration formats are unchanged.

Host tests pass with UBSan and with host O2/LTO; syntax passes for GD77, GD77S,
DM1801 and RD5R. The firmware uses the native MCUXpresso 24.12.148 / Redlib
GD-77 preset, without LTO. Firmware source commit: `7f109777b793e7c9e64766b96aebe8bee468f625`.

## Manual acceptance — Not Run for this new image

1. Select the separately supplied official donor in CPS and load the PTTFix ZIP.
   Check boot, original key beep and saved settings. No reset/erase is needed.
2. On an ordinary allowed FM channel, set Prompt=Beep, Talk Permit=On, allow
   channel beeps, disable VOX and APRS-on-PTT. Press and hold PTT: expect exactly
   three local 940 Hz notes, 40 ms each, with 10 ms gaps. Release and repeat.
3. With Talk Permit=Off, repeat: no three-tone indication. Save/reboot and check
   the setting persists.
4. Using an independent receiver on an authorized test frequency, confirm the
   local tone is absent from RF and microphone speech resumes after the tone.
   Check quick release, repeated PTT, TOT, SK2/1750 and keypad/DTMF restoration.
5. Stop and report any missing key beep, stuck TX, muted microphone or RF tone.

The menu-event bug is proven from code and covered by regression. Successful
speaker playback, MIC restore on hardware, CPS roundtrip and RF isolation of
this image remain **Not Run** until tested on the radio. No automatic flashing.

## Built artifact

- CPS ZIP: `artifacts/native-talk-permit-trigger-fix/OpenGD77-TalkPermit-PTTFix.zip`
- ZIP bytes: 228628; SHA256: `3bedbd64832118c63284aa26ba8bb283c5c4c538dd641e0a664d2d40c99775ec`
- BIN bytes: 502768; SHA256: `599c22b10308b014d7fc99f5303d4ba5ae221bcbcb88d329fbb78f43bdd8f5c7`
- Link: 0 errors, 4 pre-existing official sound-worker warnings.
- Pre-codec free space: 2836 bytes; upload headroom: 1040 bytes.
- Native image/vector/codec/RAM checks, corrupt-image mutation checks,
  18 language files, ZIP CRC and donor identity checks: PASS.
- Evidence: [manifest](validation/talk-permit/ptt-trigger-fix/manifest.json),
  [host tests](validation/talk-permit/ptt-trigger-fix/host-tests.log),
  [native build](validation/talk-permit/ptt-trigger-fix/build.log),
  [image mutations](validation/talk-permit/ptt-trigger-fix/image-mutations.log).

## User acceptance update

The user reported success with the PTTFix ZIP on 2026-11-09. Mark local Talk
Permit playback as **PASS (user reported)**, together with the earlier key-beep
confirmation. The brief success report does not separately establish CPS
roundtrip, option persistence, MIC restoration fault paths or absence of the
local tone in an independent RF receiver; those detailed checks remain Not Run.
The PTTFix ZIP is retained as the working rollback candidate for the next step.
