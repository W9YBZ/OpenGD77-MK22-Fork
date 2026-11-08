# Flash boot investigation evidence

Old source 8f64d48 / ZIP SHA256 927a4e751f0e77c635785771ad41827f348262faee64dbe952ce4287d063fae4:
user reports FLASH MEM ERROR after flashing/restart. Hardware boot = FAIL.
The previous layout checks did not prove hardware compatibility.

Candidate source/hash are in manifest.json and package-manifest.json. The build
command automatically generated the ZIP after layout validation. Rejected images
are covered by image-mutations.log. host-tests.log includes a timed flash model
against the actual driver; syntax.log additionally checks the changed shared SPI
source for all four platforms. spi-transfer-disassembly.log is from the candidate
ELF. Existing compiler warnings are retained in the logs.

Hardware update, 2026-11-08: the user confirmed normal boot with the latest
provided candidate (source 1236a41). Boot = PASS, USER REPORTED. The feedback is
recorded separately in hardware-feedback.json; original build-time manifests
are unchanged. CPS readback, flash-write/performance, FM audio and RF tests
remain NOT RUN. No JEDEC response/waveform was supplied, so the precise electrical
root cause is not independently measured.
No radio was connected, flashed or erased by these commands. Donor and firmware
binary material remain under ignored artifacts/, outside Git.
