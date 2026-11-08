# MDC1200 protocol validation — experimental, RF Blocked

## Independent sources

* Matthew Kaufman, [mdc-encode-decode](https://github.com/atmatthewat/mdc-encode-decode),
  commit `45fc9095fbde8bf6a6988c7dc0de64a4ec0ffe12`. GPL-2.0-only source,
  commercial alternate licensing offered by author. External host oracle only;
  do not link GPL library into this noncommercial-licensed firmware.
* [sdrtrunk MDCMessage](https://github.com/DSheirer/sdrtrunk/blob/master/src/main/java/io/github/dsheirer/module/decode/mdc1200/MDCMessage.java)
  and MDCMessageProcessor: separate GPL-3.0-or-later decoder implementation;
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

Golden vectors and actual cross-decode results will be added in stage 4.
No old algorithm output is used as a golden. Existing 1 ms PIT cannot schedule
833.333 us symbols. At 400 kbit/s a 4-byte addressed I2C write consumes at least
90 us plus start/stop/software/locking; no worst-case bound or phase continuity
is proven. New RF MDC remains disabled; sample/DMA design is documented later.
