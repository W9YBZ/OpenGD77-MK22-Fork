# MDC1200 protocol validation — experimental RF available, RF decode Not Run

Follow-up: [MDC1200_RF_EXPERIMENT.md](MDC1200_RF_EXPERIMENT.md) supersedes the
initial absence of a backend below. There is now an opt-in PIT/I2C IRQ sender.
It does not establish hardware phase continuity: a reset-phase channel model
FAILS external decoding. Production/default RF remains disabled. Prior host
golden/decoder evidence below is unchanged.

## Independent sources

* Matthew Kaufman, [mdc-encode-decode](https://github.com/atmatthewat/mdc-encode-decode),
  commit `45fc9095fbde8bf6a6988c7dc0de64a4ec0ffe12`. GPL-2.0-only source,
  commercial alternate licensing offered by author. External host oracle only;
  do not link GPL library into this noncommercial-licensed firmware.
* [sdrtrunk MDCMessage](https://github.com/DSheirer/sdrtrunk/blob/master/src/main/java/io/github/dsheirer/module/decode/mdc1200/MDCMessage.java)
  and MDCMessageProcessor, pinned `a4ec04a4998b4759cae5a28c6ac83d3b07f2d09d`: separate GPL-3.0-or-later decoder implementation;
  BOT/EOT lives in argument high bit, opcode 1 ANI. Its isValid() has CRC TODO,
  so it cannot alone establish CRC integrity.
* [EAS Station documentation](https://kr8mer.github.io/eas-station/reference/protocols/MDC1200/)
  conflicts: describes post-ID as opcode 0/argument 0x80. Do not treat this
  as resolved by copying a preset. Candidate here is opcode 1/argument 0,
  consistent with sdrtrunk ANI/EOT interpretation; commercial receiver/RF
  confirmation is Unverified.

## Layer audit

Evidence levels: external software agreement is stronger than new-code self
roundtrip, but weaker than known-good RF/commercial equipment. No RF capture
or device is available in this session.

| Layer | Reference-derived candidate | Evidence |
|---|---|---|
| Data | op, arg, ID high, ID low, CRC low, CRC high, zero tail | Kaufman encoder/decoder |
| POST-ID | 01 00 IDhi IDlo | sdrtrunk event interpretation; RF Unverified |
| CRC | init 0, reflected 0x8408 (normal 0x1021), xorout FFFF; first 4 bytes | Kaufman oracle |
| FEC | 56 systematic bits plus 56 parity; K=7 taps 0,2,5,6; zero state; data bytes LSB first | Kaufman oracle |
| Interleave | input n -> output (n%7)*16+n/7; output bytes MSB first | two decoders agree |
| Leader | seven 55 bytes, then sync 07 09 2A 44 6F | Kaufman encoder |
| Differential/modem | change from previous logical bit ->1800 Hz, same ->1200 Hz, previous=0 | Kaufman modem |
| Rate | 1200 symbols/s; phase-continuous 1200/1800 Hz (MSK/FFSK) | Kaufman modem |
| Length | 26 bytes / 208 symbols / 173.333 ms, excluding RF settling | Kaufman encoder |
| TX level, RF filters, deviation, settling | Unverified | requires hardware |

## Reproduced golden and cross-decode evidence

`tests/feature_migration/generate_golden.py` compiled the pristine external
encoder at the pinned commit before writing the new codec. It exports the
original CRC states, 26-byte frame, inverse-mapped FEC block, and physical tone
decisions observed from the original sample generator's xorb state. Expected
values are frozen in `tests/feature_migration/mdc_golden.json` and reproducible
without invoking the new codec. New code lives in the separate GPL host lab,
`tools/mdc1200`, never in a firmware link.

| Unit | op/arg | CRC (host numeric) | CRC bytes on air before FEC |
|---|---|---|---|
| 0001 | 01/00 | F2CD | CD F2 |
| 1234 | 01/00 | 32C2 | C2 32 |
| FFFF | 01/00 | 13FC | FC 13 |

All complete bytes, systematic/parity intermediates, four prefix CRC states,
208 packed MSB-first tone decisions and frame lengths are in the JSON. Prefix
CRC states precede the final FFFF xor. First acquisition tone is 1800 Hz, as
initialized by the external reference; later tone decisions use bit transitions.

Actual command:

```
python3 tests/feature_migration/test_mdc.py --reference /tmp/mdc-reference --output /tmp/mdc-evidence
```

Pass (host only): all three goldens at every layer; 1000 independent random
CRC comparisons; null/short/oversize/capacity guards; PCM block independence
for 1/160/257-sample blocks; analytic continuous-phase check; original Kaufman
48000 Hz demodulator independently reports `(1,0,0001)`, `(1,0,1234)`,
`(1,0,FFFF)` with CRC accepted. The new codec shares no CRC/FEC function with
that separately compiled decoder. One on-air bit corruption is corrected by
the original FEC; a valid-FEC payload with changed ID and stale CRC is rejected.
Results/checksums: `docs/validation/mdc-host-results.json`.
These are synthesized PCM tests, NOT captured RF or commercial POST-ID acceptance.
Kaufman reports raw op/arg; EOT event interpretation comes from the separately
audited sdrtrunk source. The conflicting preset remains a hardware acceptance
item, not a reason to silently change the vector.

## Physical transmitter redesign and explicit RF block

The host modulator emits phase-continuous signed PCM at 48 ksample/s, exactly
40 samples/symbol. An 80-state phase table advances by 2 for 1200 Hz and by 3
for 1800 Hz. A block boundary never resets phase or symbol position. Total
8320 samples = 173.333 ms. Streaming state is 30 bytes; a future 160-sample
DMA half-buffer would be 320 bytes (640 bytes for two halves), with a 3.333 ms
refill deadline. No IRQ masking, PIT changes, scheduler sleeps, I2C tuning or
sample-rate conversion is part of sample generation. These are computed
budgets, not measured radio timing.

A viable future RF backend needs a proven analog audio input with hardware
sample clock/DMA, e.g. a verified C6000 DAC-to-AT1846 MIC route. Current OpenMusic
C6000 output is proven by source only to reach the local speaker. Existing
MK22 DAC is PA control and MUST NOT be repurposed as audio. No schematic/bench
proof shows an independent DAC route to RF on this radio. Per-symbol AT1846
I2C writes cannot establish continuous phase or a bounded symbol start; thus
there is deliberately NO live MDC RF backend, nor a hidden enable macro.
RF integration is Blocked by physical path/timing evidence and license boundary.

Before implementing a backend, measure timer sample clock against a logic
analyzer/reference and refill jitter under display/USB/GPS load; require no
underruns across a full frame, mean baud within 0.1% of 1200 and symbol-boundary
error below 1% of 833.333 us. These are proposed engineering acceptance limits,
not published MDC tolerances. Snapshot every changed register (CSS, filters,
gain, routing), mute MIC, apply measured settle windows, send exactly one frame,
then one cleanup path restores snapshots for completion/cancel/deadline/failure.
TX deadline must include settling and frame budget and immediately honor TOT.
Host PCM amplitude 12000 is NOT an RF deviation setting. Determine deviation
using a calibrated receiver on both wide/narrow channels; do not write calibration.

`FM_FEATURE_TRACE(event,value)` is a compile-time, default-no-op measurement
hook for RF beep start/stop/restore failure. For future MDC, instrument frame
hash/op/arg/ID, DMA start/end/underrun, clock counts and route changes outside
ISR; use GPIO/logic analyzer for symbol boundaries, never printf per symbol.
The encoder and deterministic WAV/checksums already isolate frame/baud issues
from future board audio-path, transient and RF level problems.
No old algorithm output is used as a golden. Existing 1 ms PIT cannot schedule
833.333 us symbols. At 400 kbit/s a 4-byte addressed I2C write consumes at least
90 us plus start/stop/software/locking; no worst-case bound or phase continuity
is proven. New RF MDC remains disabled; sample/DMA design is documented later.
