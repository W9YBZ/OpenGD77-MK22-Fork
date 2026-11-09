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


# Official R20260131 restart: Talk Permit only

Development on `feature/gd77-talk-permit-official-r20260131` starts from the official
R20260131 source and restores the official `.project` and `.cproject` files that
were missing from this fork. The previous three-feature/audio experiment is kept
on its original branch as unsuccessful diagnostic history.

The only feature added here is **Sound Options → Talk Permit: On / Off**, for
local three-tone indication at an authorized manual FM TX start. It uses the
unchanged official sound backend, temporarily disconnects the TX microphone,
and restores it after playback or cancellation. DMR and special TX paths retain
their original behavior. Existing settings layout, CPS, Codeplug and calibration
formats are unchanged; a previously unused settings bit stores the option.

MCUXpresso IDE **24.12.148** is now installed locally with its bundled compiler
and Redlib. Both the restored official baseline and the Talk Permit firmware
have linked and passed image/vector/codec-slot/RAM checks and CPS ZIP validation.
The native workflow uses the official GD-77 preset, including its original
per-file settings and `firmware.ld`; no newlib-nano/LTO recipe is substituted.

Local packages:

* `artifacts/native-official-baseline/OpenGD77-Official-Base.zip` — test original
  key beeps, boot and CPS first.
* `artifacts/native-talk-permit-trigger-fix/OpenGD77-TalkPermit-PTTFix.zip` —
  current Talk Permit candidate; fixes the synthetic menu-entry event that
  prevented playback. Sound Options → Talk Permit On/Off.
* `artifacts/native-talk-permit/OpenGD77-TalkPermit.zip` — superseded: user
  confirmed key beeps work, but Talk Permit did not play.

Both need the official `GD-77_V4.3.6.sgl` selected separately as donor in CPS.
The ZIP contains OpenGD77.bin and 18 matching language files, without the donor.
The preceding Talk Permit package has user-confirmed working key beeps; its
Talk Permit playback failed. Hardware acceptance of the PTTFix build remains
**Not Run**. No radio was flashed, reset or erased by the build tools. The rebuilt baseline is not byte-identical to the
published release and still requires its own hardware acceptance.

One-command native build and ZIP packaging in this workspace:

```sh
python3 tools/build_native_gd77.py --out artifacts/native-talk-permit-trigger-fix \
  --name OpenGD77-TalkPermit-PTTFix \
  --donor-zip '/home/yuhui/Downloads/2021-01-26 GD-77 CPS & Firmware Changelog - Ham Version.zip'
```

See [build commands, hashes and acceptance steps](docs/TALK_PERMIT_RESTART.md),
[official source provenance](docs/OFFICIAL_SOURCE.json), and
[native build evidence](docs/validation/talk-permit/native/summary.json).

See [PTT trigger fix, evidence and acceptance](docs/TALK_PERMIT_PTT_TRIGGER_FIX.md)
for the latest candidate; earlier build reports describe the superseded ZIP.
