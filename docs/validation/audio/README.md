# Audio diagnosis evidence

Source for the feature builds: 40f5034; base is repository 72825f2 with explicit
runtime/Flash/EEPROM/AT1846/audio overlays listed and hashed in isolation.json.
summary.json records the four final BIN/ZIP hashes and both flash headrooms.
No donor, BIN, ELF or other restricted firmware payload is committed here.

hardware-feedback.json records the user's earlier-image results. No new radio
operations were performed. New-build boot/audio/RF/CPS tests remain NOT RUN.
Software defects were reproduced through fault injection; the radio's precise
silent-audio cause and any LTO dependence remain unconfirmed.

host.log and host-optimized.log exercise the actual key handler (extracted from
main.c), actual sound implementation and task loop, with mocked RTOS/GPIO/SPI.
They are not a simulation of C6000 analog hardware or the actual scheduler.
syntax.log includes all four supported MK22 platform defines; settings-abi.log
checks both enum ABIs. mdc-unchanged.log is the existing independent reference
regression, not an RF measurement. Original warnings remain in the logs.

Each variant has build/layout/package/isolation records and actual beep task
disassembly. Full variants additionally have mutation rejection tests. The .su
files are compiler stack frames from only the three no-LTO audio units, not a
whole-task worst-case bound or hardware stack watermark.
