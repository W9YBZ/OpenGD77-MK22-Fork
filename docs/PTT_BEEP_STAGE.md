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

## Validation planned

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
