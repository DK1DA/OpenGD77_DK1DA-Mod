# OpenGD77_DK1DA-Mod — User Manual

Feature and usage guide for **OpenGD77_DK1DA-Mod** (a fork of [OpenGD77](https://opengd77.org))
on the **TYT MD-UV380 / Retevis RT-3S** and **Baofeng DM-1701 / Retevis RT-84**.

## How to read this manual

Each feature is tagged so you can tell what is unique to OpenGD77_DK1DA-Mod and what is
inherited from upstream OpenGD77:

- 🟢 **[OpenGD77_DK1DA-Mod]** — new or noticeably changed in OpenGD77_DK1DA-Mod.
- ⚪ **[OpenGD77]** — inherited from upstream OpenGD77, documented here for
  convenience. For exhaustive coverage of upstream behaviour, see the official
  OpenGD77 user guide.

> Most day-to-day operation (VFO/Channel, FM, DMR, scanning, hotspot, CPS sync) is
> standard OpenGD77. This manual focuses on **how to reach each feature** and marks
> clearly what OpenGD77_DK1DA-Mod adds or changes.

---

## Buttons at a glance

The MD-UV380 / RT-3S has **PTT**, **SK1** (upper side key), **SK2** (lower side
key), the **top / orange** button, and a keypad with **green** (✓ / menu), **red**
(✗ / back), arrows, `0`–`9`, `*` and `#`.

| Control | Action |
| --- | --- |
| **Green** | Menu / confirm |
| **Red** | Back / exit |
| **Top / orange** button | Open the **quick menu** (VFO or Channel) — ⚪ [OpenGD77] |
| **SK1** (hold) | Show frequency / secondary info |
| **SK2** (hold) | Modifier for other keys |
| **SK2** (short tap) | **Toggle mute** on VFO/Channel — 🟢 [OpenGD77_DK1DA-Mod] |
| **SK2** (extra-long) | Monitor / open squelch — ⚪ [OpenGD77] |
| **SK1 + SK2** | Reverse repeater — ⚪ [OpenGD77] |
| **`#`** (hold, VFO) | Toggle the **spectrum scope** — ⚪ [OpenGD77] |
| **SK2 + green** (Channel) | Open **Channel Details** editor — ⚪ [OpenGD77] |

---

## Display & UI

### Segmented S-meter — 🟢 [OpenGD77_DK1DA-Mod]
The signal meter under the top status bar is drawn as discrete blocks: **9 green**
blocks for S1–S9 (4 dB each) and **6 red** blocks for S9+10…+60 (10 dB each), with
a scale-label row beneath (`S1 S3 S5 S7 S9 / +20 +40 +60`). The leading block fills
proportionally.

### 7-segment "LED" frequency font — 🟢 [OpenGD77_DK1DA-Mod]
On the VFO and the SK1-held frequency view, the frequency is rendered in a
7-segment "LED" style (lit segments only). "MHz" matches the frequency colour
(green on RX, red on TX).

### Screen dimming — 🟢 [OpenGD77_DK1DA-Mod]
Optionally dims the backlight after inactivity to **100 / 50 / 30 / 15 %** of the
configured brightness (only in **Auto** backlight mode). Any activity restores full
brightness. Set it in **Menu → Options → Display Options → Screen Dim**.

### Theme presets — 🟢 [OpenGD77_DK1DA-Mod]
Pick a colour theme on the radio at **Menu → Options → Display Options → "Theme"**
(◀/▶ to change; the screen updates live):

- **Custom** — the theme edited via the CPS or the on-radio colour editor (default).
- **BASIC 80s** — a Commodore-64 look (light blue on blue).
- **Terminal** — a green terminal (green-on-black at night, black-on-green by day).
- **Vertexic** — amber/orange (black-on-amber by day, amber-on-black at night).

Editing colours in the CPS only affects **Custom**; the two built-in presets are
fixed. Every element (S-meter, frequencies, menus) follows the selected theme.

### Mute indicator — 🟢 [OpenGD77_DK1DA-Mod]
While muted, a bold **"M"** appears in the status bar (just left of the battery)
and the **red LED flashes** (0.5 s every 2 s). See *Mute* below.

---

## Audio

### One-tap mute — 🟢 [OpenGD77_DK1DA-Mod]
- A **short tap of SK2** on the VFO/Channel screen toggles the speaker mute on/off.
  (Exception: while a DMR QSO is on screen, SK2 keeps its "set TX talkgroup to the
  incoming one" role.)
- Turning the **volume knob fully down** also counts as muted.
- Mute is shown by the **"M"** indicator and the flashing red LED (above).
- Mute is also available in the Channel/VFO **quick menu**.

---

## Operating

### VFO & Channel modes — ⚪ [OpenGD77]
Switch between direct-frequency **VFO** mode and stored **Channel** mode with the
mode toggle. Most operating happens here.

### Spectrum scope (VFO sweep) — ⚪ [OpenGD77]
A real-time signal-strength-vs-frequency graph centred on the VFO frequency.
- **Enter/exit:** in **VFO mode**, **hold `#`** (long press). Hold `#` again to leave.
- **Left / Right:** move the centre frequency across the band.
- **Up / Down:** change the sweep **span / step** (zoom in / out).
- **SK2 + Up/Down:** adjust **gain**; **SK2 + Left/Right:** adjust the
  **noise-floor** reference (how the trace fills the screen).
- The gain / noise-floor / step settings are remembered.
- The scope is **VFO-only** (not available in Channel mode).

### Scanning — ⚪ [OpenGD77]
Channel and VFO scanning with hold/pause behaviour, per the upstream OpenGD77
controls.

### Quick menu — ⚪ [OpenGD77] (with 🟢 [OpenGD77_DK1DA-Mod] additions)
Press the **top / orange** button to open the quick menu for the current screen
(VFO or Channel). It holds filters (FM/DMR), Talkaround, Roaming, Mute, VFO↔Channel
copy, and — on the Channel screen — the OpenGD77_DK1DA-Mod **new channel** / **Delete Channel**
items (see below).

---

## Channels & zones

### Channel Details editor — ⚪ [OpenGD77]
Edit an existing channel's parameters (name, frequencies, tones, power, bandwidth,
timeslot, colour code, groups, …). From the Channel screen, press **SK2 + green**.
Press **green** to save.

### Create & delete channels on the radio — 🟢 [OpenGD77_DK1DA-Mod]
No PC/CPS required. Open the **Channel quick menu** (top/orange button) and scroll
to the bottom:
- **New channel** — creates a blank **145.500 MHz FM** channel in the first free
  slot, adds it to the current zone, and opens **Channel Details** to edit it. The
  new channel is immediately selectable in the zone.
- **Delete Channel** — deletes the selected channel after a confirmation (**green**
  confirms, **red** cancels). It is also removed from every zone that referenced it.
  The last remaining channel can't be deleted, and read-only/APRS channels are
  protected.

> **Back up your codeplug** (via the CPS) before bulk edits — changes are written
> straight to the radio and there is no on-device undo.

### Zones — ⚪ [OpenGD77]
Select zones from **Menu → Zone**. The virtual **All Channels** zone lists every
channel. (On-radio *zone* editing is planned for a future OpenGD77_DK1DA-Mod release.)

---

## DMR

### DMR operation — ⚪ [OpenGD77]
Talkgroups/contacts, timeslots, colour codes, RX groups, and hotspot mode, all per
upstream OpenGD77. The AMBE voice codec is merged in at flash time (see
[`TOOLS.md`](TOOLS.md)).

### DMR reliability fix — 🟢 [OpenGD77_DK1DA-Mod]
OpenGD77_DK1DA-Mod resolves a hard-fault (freeze needing a battery pull) on the DMR path that
affected recent self-built OpenGD77 revisions — the codec is called through C
function pointers instead of inline assembler that modern GCC miscompiled.

---

## About & versioning — 🟢 [OpenGD77_DK1DA-Mod]

**Menu → Firmware info** shows the **OpenGD77_DK1DA-Mod version** (semantic versioning, e.g.
`OpenGD77_DK1DA-Mod v0.4.0`) alongside the git commit ID. Pre-1.0 builds are published as
pre-releases. See [`CHANGELOG.md`](CHANGELOG.md) for the full history.

---

*Building, flashing and tools are covered in [`TOOLS.md`](TOOLS.md); the project
overview is in [`README.md`](README.md).*
