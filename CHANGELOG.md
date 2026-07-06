# Changelog

All notable changes to this fork of the OpenGD77 firmware (MD-UV380 / DM-1701).

This project is still in pre-release iteration, so changes are collected as a
single running list until the first versioned release.

## Unreleased

- **DM-1701 build support** — the Docker build is now parameterized by platform;
  `./build-firmware.sh DM1701` produces `OpenDM1701.bin` (Baofeng DM-1701 /
  Retevis RT-84). Not officially tested yet.

## 0.1.0

First versioned release of OpenGD77_DK1DA-Mod (fork of OpenGD77 R20260131).

### Features

- **Screen dimming** — optional idle dimming of the backlight to a fraction of
  the configured brightness (Off / 100% / 50% / 30%), in Auto backlight mode.
  Any activity (keys, rotary, RX, TX) restores full brightness. New setting in
  Display Options.
- **Mute** — one-tap audio mute:
  - **SK2 short-tap** toggles mute on the VFO/Channel screens (except while a DMR
    QSO is on screen, where SK2 keeps its "set TX TG to incoming" function).
  - Turning the **volume knob fully down** is also treated as muted.
  - While muted: a bold **"M"** appears in the status bar (left of the battery)
    and the **red LED flashes** (0.5 s every 2 s).

### Display / UI

- **S-meter** redrawn as discrete blocks — 9 green blocks (S1–S9) plus 6 red
  blocks (S9+10…+60), with gaps and a proportionally-filled leading block, and a
  scale label row beneath (S1 S3 S5 S7 S9 / +20 +40 +60).
- **7-segment "LED" frequency font** for the VFO and SK1-held frequency display,
  with faintly-lit inactive segments; digits are wider than the normal font.
- **Main-menu icons** — a small glyph next to each top-level menu entry.
- Layout spacing adjusted so the contact/TG line, the DMR talkgroup/talker and
  the private-call caller ID clear the new S-meter label row.

### Fixes

- **DMR no longer hard-faults.** The AMBE codec is now called through C function
  pointers instead of the old inline assembler (which modern GCC miscompiled),
  fixing the freeze-on-DMR that required a battery pull.
- Fixed the DMR talkgroup/talker and private-call caller ID overlapping the
  S-meter labels.

### Build / Tooling

- **Dockerized build** using the Arm GNU Toolchain 14 (matching the official
  builds); always a clean build. `./flash.sh` helper for flashing with model,
  language and DMR donor.
- Integrated the OpenGD77 **screen grabber** (`gd-77_screen_grabber.py`) and the
  GD-77 loader into the repo.
- **`TOOLS.md`** documents building, flashing, screenshots and language files.

### Project

- **Renamed the project to OpenGD77_DK1DA-Mod** (fork of OpenGD77). New `README.md`
  documenting the fork and its upstream source; original OpenGD77 README kept as
  `README.OpenGD77.md`. Added `LICENSE.md` reproducing the inherited OpenGD77
  license (modified BSD 3-Clause + non-commercial clause; not GPL-compatible).
- On-screen branding renamed to **OpenGD77_DK1DA-Mod** (splash screen + Firmware Info name).
- README notes this iteration is developed/tested only on the **TYT MD-UV380 and
  Retevis RT-3S**, with a contact for testing on other supported radios.
- Firmware-info **credits** simplified to "Marcus DK1DA" and a reference to
  "The original OpenGD77 authors".

### Misc

- Added **Marcus DK1DA** to the firmware-info credits ("Modified Fork by").
