# Isolated MDC1200 host lab (GPL-2.0-only)

This directory contains the independent encoder/sample generator. Normal GD-77
builds do not link it. The user's later request added `--experimental-mdc-rf`,
which explicitly includes the codec in a GD-77 RF lab build; see
[the RF experiment](../../docs/MDC1200_RF_EXPERIMENT.md). This is not a resolution
of combined-distribution licensing compatibility. The firmware's noncommercial license and GPL library
terms must not be represented as a single compatible license. These files are
explicitly GPL-2.0-only because the algorithm audit consulted GPL reference source;
this is a conservative licensing boundary, not a legal opinion. Normal firmware builds keep this module separate; the opt-in lab exception is
described above. Combined redistribution compatibility is not claimed. No AMBE binary is included.

`mdcEncodePostID` builds candidate `01 00` ANI/EOT. See
[protocol evidence](../../docs/MDC1200_PROTOCOL_VALIDATION.md) for the conflicting
`00 80` claim. Encoding/PCM tests do not prove a commercial radio will accept it.
`mdcPCMRead` streams blocks at 48000 samples/s: exactly 40 samples/symbol,
continuous phase, 1200/1800 Hz. It has no hardware dependencies, sleeps, interrupt
changes, float math or I2C transactions. The optional board adapter is experimental and has no RF decode evidence.

Reproduce host verification (external reference checkout stays outside repo):

```sh
git clone https://github.com/atmatthewat/mdc-encode-decode /tmp/mdc-reference
git -C /tmp/mdc-reference checkout 45fc9095fbde8bf6a6988c7dc0de64a4ec0ffe12
python3 tests/feature_migration/test_mdc.py --reference /tmp/mdc-reference --output /tmp/mdc-evidence
python3 tests/feature_migration/generate_golden.py /tmp/mdc-reference > /tmp/golden.json
cmp tests/feature_migration/mdc_golden.json /tmp/golden.json
```

The decoder adapter compiles the external original decoder separately. It neither
uses the new CRC nor new FEC. Golden generation never calls this new codec.
