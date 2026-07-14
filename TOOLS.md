# Tools & Workflows

Helper scripts and tools for building, flashing, screenshotting and translating
the OpenGD77 firmware for the **MD-UV380 / DM-1701** (STM32F405).

All paths below are relative to the repository root.

---

## 1. Prerequisites

| Need | Install |
| --- | --- |
| Docker | Docker Desktop (the build runs in a `linux/amd64` container; no host toolchain needed) |
| Python 3 | Preinstalled on macOS |
| `pyusb` + `libusb` | flashing — `brew install libusb && python3 -m pip install --user pyusb` |
| `pyserial` + `pillow` | screenshots — `python3 -m pip install --user pyserial pillow` |
| `gcc` | only if you build the language `.gla` files on the host (the Docker build does it for you) |

---

## 2. Building the firmware

Builds entirely in Docker with the **Arm GNU Toolchain 14.2** (GCC 14) — the version
the official OpenGD77 firmware uses. (Ubuntu's stock GCC 10 miscompiles the DMR codec
path; see the git history.)

```bash
./build-firmware.sh            # MD-UV380 / RT-3S  -> build/OpenMDUV380.bin  (default)
./build-firmware.sh DM1701     # DM-1701 / RT-84   -> build/OpenDM1701.bin
```

Output: `MDUV380_firmware/build/Open<platform>.bin` (the flashable, FM-only image; the DMR
codec is merged in at flash time — see below). The build is always clean (`rm -rf build`),
so building one platform overwrites the other's output.

Each build finishes with a **memory-usage summary** — used, size and free bytes for the
FLASH (code), RAM and CCMRAM regions — so the remaining headroom for new features is
visible at a glance.

---

## 3. Flashing

The radio must be in **DFU / firmware-update mode**: power **off**, then power **on**
while holding **PTT + SK1** (the button above PTT). The screen stays blank and the top
LED glows steady. Connect USB.

### 3a. `./flash.sh` (recommended wrapper)

Wraps `opengd77_stm32_firmware_loader.py` with sensible defaults (model, language, donor).

```bash
./flash.sh                    # MD-UV380, German 2nd language, saved DMR donor, build/OpenMDUV380.bin
./flash.sh -L none            # English only
./flash.sh -L French          # a different secondary language
./flash.sh -m DM-1701 -L Italian
./flash.sh -f /path/to/other.bin   # flash a specific image (e.g. a known-good bin)
./flash.sh -s ~/Downloads/official_V26.45.bin   # (re)register the DMR codec donor
./flash.sh --list-languages   # list available languages
./flash.sh -l                 # list attached DFU devices (verify the radio is seen)
```

- **Language:** the firmware always has English as language #1; `-L <Name>` injects one
  secondary language. The `.gla` files are auto-built on first use.
- **DMR donor:** a valid image only flashes DMR audio if a **codec donor** is registered.
  This is the official **V26.45** firmware (SHA-256 `d8a653…cf11f`); pass it once with `-s`
  and the path is remembered in `~/.gd77firmwareloader.ini`. Without a donor the radio is
  **FM-only**.

### 3b. `opengd77_stm32_firmware_loader.py` (the underlying loader)

`MDUV380_firmware/tools/opengd77_stm32_firmware_loader.py` — what `flash.sh` calls.

```bash
cd MDUV380_firmware/tools
python3 opengd77_stm32_firmware_loader.py -l                 # list DFU devices
python3 opengd77_stm32_firmware_loader.py -m MD-UV380 \
    -s /path/to/official_V26.45.bin \                        # register donor (once)
    -L ../application/include/user_interface/languages/src/German.gla \
    -f ../build/OpenMDUV380.bin
```

Models: `MD-UV380`, `DM-1701`, `MD-9600`, `MD-2017`, `MD-380`.

---

## 4. Grabbing a screenshot

`MDUV380_firmware/tools/gd-77_screen_grabber.py` is the **universal** OpenGD77 screen
grabber (the "gd-77" name is historical). It talks over the USB **CDC serial port**,
auto-detects the radio model and reads the MD-UV380's 160×128 RGB565 framebuffer into a PNG.

Power the radio on **normally** (not DFU mode), navigate to the screen you want, and
connect USB.

**Easiest — the `./screenshot.sh` helper** (auto-detects the serial port and sets up a
self-contained Python venv with `pyserial` + `pillow` on first run):

```bash
./screenshot.sh                 # -> screenshots/screenshot-<timestamp>.png
./screenshot.sh vfo-7seg        # -> screenshots/vfo-7seg.png
./screenshot.sh -d /dev/cu.usbmodemXXXX [name]   # explicit port
```

**Or call the grabber directly** (`-o` takes the filename **without** `.png`):

```bash
python3 MDUV380_firmware/tools/gd-77_screen_grabber.py \
    -d /dev/cu.usbmodemXXXX \
    -o ~/Desktop/screenshot
```

Options:

| Flag | Meaning |
| --- | --- |
| `-d, --device <port>` | serial port (macOS: `/dev/cu.usbmodem…`; find with `ls /dev/cu.usbmodem*`) |
| `-o, --output <name>` | output PNG, without the `.png` extension |
| `-s, --scale <n>` | scale the image n× (e.g. `-s 2`) |
| `-f, --foreground` / `-b, --background` | colours for mono radios (ignored on the colour MD-UV380) |

**Finding the port:** with the radio unplugged run `ls /dev/cu.usbmodem*`, plug it in, and
see which entry appears. Requires the radio running normally (CDC active), not DFU mode.

---

## 5. Language (`.gla`) files

Secondary-language plugin files are generated from the language `.h` sources by a small
host tool. `flash.sh` does this automatically, but to build them manually:

```bash
cd MDUV380_firmware/application/include/user_interface/languages/src
gcc -w -I../ -o languages_builder languages_builder.c
./languages_builder            # emits Catalan.gla, German.gla, French.gla, … (18 languages)
```

Available: Catalan, Croatian, Czech, Danish, Dutch, Finnish, French, German, Hungarian,
Italian, Polish, PortuguesBrazil, Portuguese, Romanian, Slovenian, Spanish, Swedish, Turkish.

---

## 6. Other tools

- `MDUV380_firmware/tools/codec_cleaner.Linux` — creates the empty codec placeholder
  section during the build (run automatically by `docker/build.sh`).
- `MDUV380_firmware/tools/gd-77_firmware_loader.py` — the GD-77 / DM-1801 / RD-5R (NXP MK22)
  SGL loader. **Not for the MD-UV380** — this radio uses
  `opengd77_stm32_firmware_loader.py` (section 3b). Included only for completeness.

---

## Quick reference

```bash
./build-firmware.sh                                   # build -> MDUV380_firmware/build/OpenMDUV380.bin
./flash.sh -L German                                  # flash (radio in DFU mode: power on holding PTT+SK1)
python3 MDUV380_firmware/tools/gd-77_screen_grabber.py -d /dev/cu.usbmodemXXXX -o shot   # screenshot
```
