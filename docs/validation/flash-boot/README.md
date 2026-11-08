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

Candidate hardware boot, CPS readback, flash-write/performance and RF tests:
NOT RUN. SPI timing is corrected and failure ID exposed, but the failing radio's
JEDEC response and waveform are not available, so root cause is not confirmed.
No radio was connected, flashed or erased by these commands. Donor and firmware
binary material remain under ignored artifacts/, outside Git.
