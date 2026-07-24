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
| **Green** (hold) | Open **Messages** (DMR SMS) — 🟢 [OpenGD77_DK1DA-Mod] |

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

### Battery display mode — 🟢 [OpenGD77_DK1DA-Mod]
The battery level in the top status bar can be shown three ways, cycled with
◀/▶ at **Menu → Options → Display Options → Battery**:

- **%** — remaining charge as a percentage.
- **V** — the measured battery voltage.
- **Bars** — a battery-shaped icon with up to **four segments**, each worth
  **25 %** of charge. The fill blinks when the battery is low.

The **%** and **V** options are inherited from OpenGD77; **Bars** is the new
addition. In every mode the low-battery warning still blinks the reading.

### Shut-off animation — 🟢 [OpenGD77_DK1DA-Mod]
Powering off collapses whatever is on screen towards the middle — squeezed to a
line, then a dot — like an old CRT switching off, in place of the "Power Off"
message. (Once the animation starts, the power-off is committed.)

### Theme presets — 🟢 [OpenGD77_DK1DA-Mod]
Pick a colour theme on the radio at **Menu → Options → Display Options → "Theme"**
(◀/▶ to change; the screen updates live):

- **Custom** — the theme edited via the CPS or the on-radio colour editor (default).
- **BASIC 80s** — a Commodore-64 look (light blue on blue).
- **Terminal** — a green terminal (green-on-black at night, black-on-green by day).
- **Vertexic** — amber/orange (black-on-amber by day, amber-on-black at night).
- **Red** — black-on-red by day, red-on-black at night.
- **Neon 80s** — an 80s-synthwave palette with several neon colours (cyan,
  magenta, violet, green, yellow) across the UI over a dark background.
- **Pride** — the background is painted as a rainbow pride flag; body text is
  black over the stripes, and the menu title, header and notifications use white
  boxes so they stay readable. (Text drawn directly over the brightest stripes
  has lower contrast than a solid-background theme — that's inherent to a
  rainbow background.)

Editing colours in the CPS only affects **Custom**; the built-in presets are
fixed. Every element (S-meter, frequencies, menus) follows the selected theme.

### Mute indicator — 🟢 [OpenGD77_DK1DA-Mod]
While muted, the **red LED flashes** (0.5 s every 2 s). See *Mute* below.

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
**There are no scan lists.** Unlike the vendor CPS, OpenGD77 scans **the channels
of the currently selected zone** — so a zone *is* the scan list. To scan a
particular set of channels, put them in a zone and select it. The **All Channels**
virtual zone scans every channel in the codeplug.

- **Start/stop:** long-press **UP** on the Channel or VFO screen (**RED** stops).
- **Exclude a channel:** **Menu → Channel details** (or **SK2 + green** on the
  Channel screen) → **Zone Skip** (skipped when scanning a normal zone) and
  **All Skip** (skipped when scanning *All Channels*).
- **Settings:** **Menu → Options → Radio options** → **Scan delay**,
  **Scan dwell**, **Scan mode** (Hold / Pause / Stop) and **Scan On Boot**.

Note: in the codeplug these two skip flags reuse the old "Auto Scan" and "Lone
Worker" bits, so a vendor CPS may label them differently — set them on the radio
to avoid confusion.

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

### Contacts list — 🟢 [OpenGD77_DK1DA-Mod]
**Menu → Contacts** lists all contact types together under the title "Contacts"
(instead of defaulting to the group filter). Press **`#`** to cycle the filter:
all → groups → private → all-call → all.

### DMR reliability fix — 🟢 [OpenGD77_DK1DA-Mod]
OpenGD77_DK1DA-Mod resolves a hard-fault (freeze needing a battery pull) on the DMR path that
affected recent self-built OpenGD77 revisions — the codec is called through C
function pointers instead of inline assembler that modern GCC miscompiled.

### Text messages (SMS) — 🟢 [OpenGD77_DK1DA-Mod]

Send and receive DMR text messages, compatible with BrandMeister, hotspots and
OEM radios (Motorola TMS format on transmit; TMS **and** Anytone/"DMR standard"
decoded on receive).

**Open it** from **Menu → Messages**, or **hold GREEN** on the channel/VFO
screen. The hub offers *New message*, *Inbox*, *Sent*, *Quick text* and
*Options*.

**Composing:** in the recipient field, either type the destination DMR ID with
the number keys and press **DOWN** to move on to the message, or press **GREEN**
to pick a recipient from your private-call DMR contacts (selecting one drops you
straight into the message text). Type up to 160 characters with multi-tap on the
keypad (**LEFT/RIGHT** move the cursor, **SK2+LEFT** deletes), then press
**GREEN** to send. Sending only works on a DMR channel; progress is shown as
notifications ("Sending", "Sent", "Delivered"). With **Wait for ACK** enabled
(default), a missing delivery report pops up a **resend** prompt.

**Receiving:** a popup with a chime shows the sender and a preview —
**GREEN** opens the message, **SK2+GREEN** starts a reply, **RED** dismisses.
The inbox and sent box keep the last 8 messages each and survive power
cycles; unread messages are marked `*`. In a message view, **GREEN**
replies (inbox) or re-edits/resends (sent), **SK2+RED** deletes. The radio
automatically answers delivery-report requests from other radios.

**Quick texts:** up to 8 editable templates (**GREEN** uses one in a new message,
**SK2+GREEN** edits, **SK2+RED** deletes).

**Options:** *Wait for ACK* (delivery confirmation) and *My ID only*
(ignore messages not addressed to your DMR ID). PTT is locked out for the
second or two a message is on air; hotspot mode is unaffected.

---

## About & versioning — 🟢 [OpenGD77_DK1DA-Mod]

**Menu → Firmware info** shows the **OpenGD77_DK1DA-Mod version** (semantic versioning,
`OpenGD77_DK1DA-Mod vMAJOR.MINOR.PATCH`) alongside the git commit ID. Pre-1.0 builds are
published as pre-releases. See [`CHANGELOG.md`](CHANGELOG.md) for the full
history.

---

*Building, flashing and tools are covered in [`TOOLS.md`](TOOLS.md); the project
overview is in [`README.md`](README.md).*
