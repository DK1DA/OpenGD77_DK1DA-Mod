# Changelog

All notable changes to this fork of the OpenGD77 firmware (MD-UV380 / DM-1701).

This project is still in pre-release iteration, so changes are collected as a
single running list until the first versioned release.

## Unreleased

_Nothing yet._

## 0.10.0

- **SMS recipient from contacts** — when composing a message, press **GREEN** in
  the recipient field to pick the destination from your private-call DMR
  contacts (or type a DMR ID manually and press **DOWN** to move to the text).
- **Contacts list shows everything** — Menu → Contacts now lists all contact
  types together under the title "Contacts" instead of defaulting to the group
  filter; **HASH** still cycles the per-type filters (all / groups / private /
  all-call).

## 0.9.0

- **Pride theme** — the status/header bar and the menu title bar are now painted
  in the top rainbow-stripe colour so they blend into the background instead of
  showing as white boxes.

## 0.8.0

- **Three new theme presets** (Display Options → "Theme"): **Red** (black-on-red
  day / red-on-black night); **Neon 80s**, an 80s-synthwave palette that maps
  several neon colours (cyan, magenta, violet, green, yellow) across the
  individual UI elements over a dark background; and **Pride**, which paints the
  background as a rainbow pride flag with black text and white menu/header boxes
  kept readable over the stripes.
- **Build:** each build now prints a per-region memory-usage summary
  (FLASH / RAM / CCMRAM used, size and free) so the remaining headroom is
  visible after every build.

## 0.7.1

- **Fix:** the radio **crashed** when navigating the Messages menu with voice
  prompts enabled. Menu strings added after the voice-prompt pack was built map
  to a table-of-contents slot that doesn't exist; the player read past the end
  of the table. Such strings now play silence instead (also hardens the
  pre-existing strings sitting at the end of the table).

## 0.7.0

- **DMR text messages (SMS)** — full ETSI/Motorola-TMS-compatible messaging:
  compose on the keypad (multi-tap, up to 160 characters), inbox and sent box
  (8 messages each, persisted to flash across power cycles), 10 editable
  quick-text templates, an incoming-message popup with chime and one-key
  reply, and delivery confirmation (waits for the ETSI response PDU / ACK
  with a resend prompt on timeout). Transmits in the Motorola TMS format
  (UDP port 4007) understood by BrandMeister, hotspots and most OEM radios;
  receives both TMS and the Anytone/"DMR standard" format (port 5016) and
  answers delivery-report requests from other radios. Reachable from the
  main menu ("Messages") or a **long-press on GREEN** in channel/VFO mode.
  Options: "Wait for ACK" and "My ID only" (accept only messages addressed
  to your own DMR ID). Voice PTT is locked out while a message transmits,
  and hotspot mode is unaffected.
- **Removed the main-menu icons** — the top-level menu entries are plain text again.

## 0.6.0

- **Theme presets** — selectable on the radio in Display Options → "Theme":
  **Custom** (CPS-editable, default), **BASIC 80s** (Commodore 64 blue/light-blue),
  **Terminal** (green-on-black night / black-on-green day), and **Vertexic**
  (orange, black-on-orange day / orange-on-black night). The choice is stored
  in the codeplug custom-data area (migration-safe; defaults to Custom), and CPS
  theme edits only ever change Custom.
- **Fix:** green LED **flickering** between the red flashes while muted during a DMR
  QSO. The DMR RX handler re-asserts the green LED as soon as it reads it off, racing
  the main-loop mute clear; the green LED is now **held off at the driver** for the
  whole mute (and released on un-mute), so only the red flash shows.
- **`screenshot.sh`** — a one-command wrapper around the OpenGD77 screen grabber
  that auto-detects the serial port and sets up a self-contained Python venv
  (pyserial + pillow) on first run. Saves to `screenshots/`.

## 0.5.0

- **Fix:** un-muting during an active QSO left the LED **off** instead of returning
  to **green**. On un-mute the green RX LED is now restored from the current receive
  state (active DMR slot or FM/digital signal), so an in-progress call lights green
  again.
- **`MANUAL.md`** — a user-facing feature & usage guide covering both inherited
  OpenGD77 features and OpenGD77_DK1DA-Mod additions, each tagged 🟢 [OpenGD77_DK1DA-Mod] or ⚪
  [OpenGD77]. Includes the spectrum scope (VFO sweep, hold `#`), channel add/delete,
  mute, dimming, and a button reference.
- **Documentation website** (`website/`) — MkDocs Material config that renders the
  markdown into a static, searchable HTML site (client-side search, dark/light
  theme). Built in Docker via `./website/build-docs.sh`; the public URL is set with
  `SITE_URL=…`. For self-hosting on Forgejo.

## 0.4.0

- **On-radio channel add/delete.** The Channel quick menu (top button) has two new
  entries:
  - **new channel** — creates a blank 145.500 MHz FM channel in the first free
    slot, adds it to the current zone, and opens Channel Details to edit it. The
    zone is reloaded so the channel is immediately navigable.
  - **Delete Channel** — deletes the selected channel after a confirmation, and
    scrubs it out of every zone that referenced it. Refuses to delete the last
    remaining channel, and respects the read-only/APRS channel lock.
  - The new menu strings are **translated** (German, French, Italian, Spanish,
    Portuguese, Dutch, Danish, Swedish, Catalan, Finnish, Czech; English fallback
    for the rest).
- **`flash.sh`** now always rebuilds the language (`.gla`) packs, so edits to the
  language files are picked up instead of a stale pack being re-flashed.
- **Release script** now flags every release before v1.0.0 as a **pre-release**
  (and corrects the flag on re-runs).

## 0.3.0

- **Fix a second voice-prompt crash** on the Screen Dim menu row: the item *label*
  ("Screen Dim") is a raw literal, and the shared menu code voices labels via
  `voicePromptsAppendLanguageString()`, which crashes on any pointer outside the
  language table. That function now ignores non-language pointers, so a stray
  literal is skipped instead of hard-faulting. (The dim value is still announced.)

## 0.2.0

- **Fix crash** opening Display Options with **voice announcements on**: the Screen
  Dim value ("100%"/"50%"/…) is a plain literal, but the shared menu code fed it to
  `voicePromptsAppendLanguageString()`, which derives the prompt index from the
  string's offset within the language table — a foreign pointer produced a garbage
  index and crashed. The value now goes through `voicePromptsAppendString()`. Also
  moved the dim-level high bit off a low legacy option bit (bit 2 → 29) and clamped
  the level so it can never index past the option arrays.
- **7-segment frequency font:** inactive segments are now off (background) instead
  of faintly lit — no dim "ghosting". "MHz" now matches the frequency colour, and
  there's 1px more space between the RX and TX frequency lines.
- **Screen dimming:** added a **15%** idle-dim level (Off / 100 / 50 / 30 / 15).
- **DM-1701 build support** — the Docker build is now parameterized by platform;
  `./build-firmware.sh DM1701` produces `OpenDM1701.bin` (Baofeng DM-1701 /
  Retevis RT-84). Not officially tested yet.
- **Release script** (`scripts/forgejo-release.sh`) now builds and attaches a
  single platform or all of them, and reuses an existing release.
- README: added "What's different from OpenGD77" and a dedicated "Flashing"
  section.

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
