# OpenGD77
Firmware for DMR transceivers using the NXP MK22 MCU, AT1846S RF chip and HR-C6000 DMR chipset.  
Including the Radioddiy GD-77 and GD-77S, the TYT MD-760 and TYT MD-730, the Baofeng DM-1801, DM-1801A, DM-860, RD-5R and DM-5R Tier2.

# Project status

The firmware is relatively stable and provides DMR and FM audio transmission and reception, as well as a DMR hotspot mode.  
However it does not support some core functionality that the official firmware supports, including sending and receiving of text / SMS messages

The firmware source code does not contain a AMBE codec required for DMR operation.  
This functionality is provided by the official firmware which is merged with the OpenGD77 by the firmware loader


# User guide

See https://github.com/LibreDMR/OpenGD77_UserGuide


# Credits
Originally conceived by Kai DG4KLU.  
Further development by Roger VK3KYY, latterly assisted by Daniel F1RMB, Alex DL4LEX, Colin G4EML and others.

Current lead developer and source code gatekeeper is Roger VK3KYY


# Copyright

 The firmware is copyright of the OpenGD77 developers. See individual source files for copyright information.

## MCU SDK and API code:   
   Copyright (c) 2015, Freescale Semiconductor, Inc.  
   Copyright 2016-2021 NXP  
   All rights reserved.
   
## FreeRTOS Kernel V10.4.3
   Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  
   All Rights Reserved.


# License

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived
   from this software without specific prior written permission.

4. Use of this source code or binary releases for commercial purposes is strictly forbidden. This includes, without limitation,
   incorporation in a commercial product or incorporation into a product or project which allows commercial use.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# Special thanks

Thanks to those who have assisted the project including :

CT1HSN
CT4TX 
DG3GSP
DG4KLU
DJ0HF
DL4LEX
EA3BIL
EA3IGM
EA5SW
EB3AM
EW1ADG
F1CXG
F1RMB
G4ELM
IK0NWG
IU4LEG
IZ2EIB
JE4SMQ
JG1UAA
OH1E
OK2HAD
ON1HK
ON7LDS
OZ1MAX
PU4RON
SQ6SFO
SQ7PTE
TA5AYX
VK3KYY
VK4JWT
VK7JS
VK7ZCR
VK7ZJA
ZL1XE

# GD-77 R20260131 feature migration

This branch adds three FM options in Sound Options:

* **PTT Tone**: Off, an end-of-transmission beep, or experimental MDC1200 POST-ID.
* **MDC1200 ID**: a persistent four-digit hexadecimal unit ID for the experimental sender.
* **Talk Permit**: the local three-tone transmit indication, using the existing audio controls.

The independent MDC encoder has host test vectors and reference-decoder checks.
Actual RF decoding remains **Not Run**; selecting MDC1200 requires the explicit
experimental build option and menu selection. PTT Tone defaults to Off.

After the SPI Flash timing correction, the user confirmed that the GD-77 boots
normally with the firmware built from `1236a41`. The earlier `8f64d48` image
failed with FLASH MEM ERROR and is retained only as diagnostic history.
The user also confirmed CPS E2026.05.26.01 read/write, settings save, and FM
receive/transmit speech. However, key beeps, local Talk Permit and the PTT RF
beep were absent. DMR voice has not been tested.

The new audio diagnostic build checks task creation, OpenMusic, FIFO and PCM
transfer results and adds read-only Audio pages after the credits in Firmware
Info. Four checked CPS packages isolate the base code, PTT/Talk Permit, the full
experiment and audio units without LTO. Their hardware audio/RF results remain
**Not Run**; the exact radio failure is not yet established. Start with
`artifacts/gd77-audio/ptt-permit/OpenGD77-PTT-TalkPermit.zip` for local beep tests.
See [audio diagnosis, package hashes and acceptance steps](docs/AUDIO_DIAGNOSTICS.md),
the [migration report](docs/FEATURE_MIGRATION_REPORT.md), and the
[RF acceptance procedure](docs/RF_ACCEPTANCE.md).

The GNU build's `--link` automatically compiles, validates the image and creates
a CPS firmware ZIP containing `OpenGD77.bin` and matching language files:

```sh
python3 tools/build_gd77.py --out artifacts/gd77-flash-fix \
  --experimental-mdc-rf --lto --link --donor-zip '/path/to/official-GD77-donor.zip'
```

An ARM GNU toolchain and newlib-nano are required; `--cc` and `--newlib` can select
an extracted toolchain. See [complete build instructions](docs/BUILD_GD77.md).
The generated package is `artifacts/gd77-flash-fix/OpenGD77-MDC1200-EXP.zip`.

In the OpenGD77 CPS Firmware Loader, select **GD-77**, select the official
**GD-77_V4.3.6.sgl** separately as the required donor, then select the generated
firmware ZIP. `--donor-zip` extracts and validates the V4.3.6 donor from the supplied
official archive; it is not included in the firmware ZIP or this repository.
The CPS merges the donor codec during upload. The build never flashes a radio.
Codeplug, settings ABI, calibration format and CPS protocol are preserved;
calibration readback and the new builds' hardware regression remain pending.

The user subsequently reported that the supplied audio candidates still did not
work. This experiment is retained as history; development restarts from the
official R20260131 source/project on `feature/gd77-talk-permit-official-r20260131`,
with Talk Permit as the only feature. No candidate here is marked audio-validated.
