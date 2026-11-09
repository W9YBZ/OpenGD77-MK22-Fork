# MDC1200 host-only protocol lab (GPL-2.0-only)

This lab implements a fixed-size pure C single-packet encoder. It does not
operate any radio, modify settings, supply an RF sender or enter the MCUXpresso
build. Firmware Sound Options continues to show MDC N/A. Keep using the
user-confirmed Tone35Fix ZIP; this phase needs no new firmware image.

The new source was written after the independently generated goldens were
committed. It is based on auditing the external Kaufman implementation, not the
old unreliable OpenGD77 MDC implementation. Its GPL-2.0-only license applies to
this host lab; see COPYING. No combined firmware licensing compatibility is
claimed, and no GPL library is linked into the current radio image.

## Reproduce

From the repository root (Python 3 and GCC; no Python packages required):

```sh
python3 host/mdc1200/fetch_references.py --out artifacts/mdc1200-protocol/references
python3 tests/mdc1200/generate_vectors.py \
  --references artifacts/mdc1200-protocol/references \
  --out artifacts/mdc1200-protocol/regenerated-vectors.json
cmp tests/mdc1200/golden_vectors.json artifacts/mdc1200-protocol/regenerated-vectors.json
python3 tests/mdc1200/run.py \
  --references artifacts/mdc1200-protocol/references \
  --out artifacts/mdc1200-protocol/results
```

Only the fetch step uses the network. Nine original external files are pinned
by URL/commit/SHA256 in sources.json and checked again before every test run.
They are downloaded to ignored artifacts, not vendored or edited in Git.

## API and evidence

`mdc_encode_single(op,arg,id,&result)` produces CRC prefix states, information
bytes, systematic/parity FEC, 26 logical frame bytes and 208 packed tone choices.
`mdc_encode_post_candidate(id,&result)` selects provisional ANI/EOT `01/00` and
rejects ID zero without silently mapping it to another station. The generic
encoder deliberately permits ID zero as a raw protocol boundary case.
`mdc_crc16()` supports empty input; `mdc_copy_frame()` guards output capacity.
All operations have fixed storage and no firmware/RTOS/I2C dependencies.

Tone bits describe the audited reference modem: 1=1800 Hz, 0=1200 Hz, first
acquisition tone seeded high. This is not an AT1846 register stream. There is
no symbol scheduler, PCM generator or RF modulation backend in the encoder.
The tests alone synthesize ideal continuous-phase PCM and pass it to the
separately compiled original sample decoder. Generated WAVs are synthetic test
fixtures, not captured RF or a calibrated radio stimulus. Their amplitude is
arbitrary, and they include silence outside the nominal frame.

Goldens are generated solely by the original external encoder, with intermediate
FEC recovered by the independently audited inverse mapping. The new encoder is
never used to generate expected bytes. Original CRC/block/sample decoders are
separate shared objects and do not call new CRC/FEC routines. A test probe uses
new FEC only to construct deliberate stale-CRC negative fixtures, not goldens.

SDRTrunk flag rules are audited from its pinned original source; its Java
receiver is not run, and its CRC TODO is explicitly not a validation result.
The `00/80` preset found in another project conflicts with its ANI/EOT flags;
commercial POST-ID interpretation and real RF decoding remain unverified.

See [protocol evidence and results](../../docs/MDC1200_PROTOCOL_VALIDATION.md).
