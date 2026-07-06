#!/usr/bin/env bash
#
# flash.sh — flash the OpenGD77 firmware to an MD-UV380 / DM-1701 in DFU mode,
# optionally merging a secondary language and the DMR (AMBE codec) donor firmware.
#
# The radio's language slot #1 is always English. This script injects ONE
# secondary language (slot #2) via the loader's -L option. Re-flash with a
# different language to change it.
#
# Usage:
#   ./flash.sh                     # defaults: model MD-UV380, language German
#   ./flash.sh -L French           # flash with French as the secondary language
#   ./flash.sh -m DM-1701 -L Italian
#   ./flash.sh -L none             # English only, no secondary language
#   ./flash.sh -s /path/donor.bin  # (re)register the DMR codec donor firmware
#   ./flash.sh --list-languages    # show available languages and exit
#   ./flash.sh -l                  # list attached DFU devices and exit
#
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FW_DIR="$REPO_DIR/MDUV380_firmware"
LOADER="$FW_DIR/tools/opengd77_stm32_firmware_loader.py"
FIRMWARE="$FW_DIR/build/OpenMDUV380.bin"
LANG_SRC="$FW_DIR/application/include/user_interface/languages/src"

# ---- defaults -------------------------------------------------------------
MODEL="MD-UV380"     # or: DM-1701, MD-9600, MD-2017, MD-380
LANGUAGE="German"    # a language name (see --list-languages), or "none"
DONOR=""             # official V26.45 firmware; empty = use the saved one

usage() { sed -n '3,25p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit "${1:-0}"; }
lc() { printf '%s' "$1" | tr '[:upper:]' '[:lower:]'; }   # lowercase (bash 3.2 safe)

# ---- parse args -----------------------------------------------------------
while [[ $# -gt 0 ]]; do
	case "$1" in
		-m|--model)          MODEL="$2"; shift 2 ;;
		-L|--lang|--language) LANGUAGE="$2"; shift 2 ;;
		-s|--source|--donor) DONOR="$2"; shift 2 ;;
		-f|--firmware)       FIRMWARE="$2"; shift 2 ;;
		-l|--list)           python3 "$LOADER" -l; exit $? ;;
		--list-languages)
			echo "Available languages (pass to -L):"
			( cd "$LANG_SRC" && ls *.gla 2>/dev/null | sed 's/\.gla$//' ) \
				|| ls "$LANG_SRC"/../*.h 2>/dev/null | xargs -n1 basename | sed 's/\.h$//'
			echo "  none   (English only)"
			exit 0 ;;
		-h|--help)           usage 0 ;;
		*) echo "Unknown option: $1" >&2; usage 1 ;;
	esac
done

# ---- sanity checks --------------------------------------------------------
[[ -f "$LOADER" ]]   || { echo "!! Loader not found: $LOADER" >&2; exit 1; }
[[ -f "$FIRMWARE" ]] || { echo "!! Firmware not found: $FIRMWARE  (run ./build-firmware.sh first)" >&2; exit 1; }

# ---- resolve / build the language file ------------------------------------
LANG_ARGS=()
if [[ -n "$LANGUAGE" && "$(lc "$LANGUAGE")" != "none" ]]; then
	# Build the .gla files once if they're missing.
	if ! ls "$LANG_SRC"/*.gla >/dev/null 2>&1; then
		echo "==> Building language (.gla) files..."
		( cd "$LANG_SRC" && gcc -w -I../ -o languages_builder languages_builder.c && ./languages_builder >/dev/null )
	fi
	# Case-insensitive match of the requested name to an actual <Name>.gla file.
	GLA=""
	for f in "$LANG_SRC"/*.gla; do
		base="$(basename "$f" .gla)"
		if [[ "$(lc "$base")" == "$(lc "$LANGUAGE")" ]]; then GLA="$f"; break; fi
	done
	if [[ -z "$GLA" ]]; then
		echo "!! Unknown language '$LANGUAGE'. Available:" >&2
		( cd "$LANG_SRC" && ls *.gla | sed 's/\.gla$//' | sed 's/^/   /' ) >&2
		exit 1
	fi
	echo "==> Secondary language: $(basename "$GLA" .gla)"
	LANG_ARGS=(-L "$GLA")
else
	echo "==> Secondary language: none (English only)"
fi

# ---- donor firmware (DMR / AMBE codec) ------------------------------------
DONOR_ARGS=()
if [[ -n "$DONOR" ]]; then
	[[ -f "$DONOR" ]] || { echo "!! Donor firmware not found: $DONOR" >&2; exit 1; }
	echo "==> DMR donor (this run): $DONOR"
	DONOR_ARGS=(-s "$DONOR")
elif grep -qi 'sourcestm32firmware *= *..*' "$HOME/.gd77firmwareloader.ini" 2>/dev/null; then
	saved="$(grep -i 'sourcestm32firmware' "$HOME/.gd77firmwareloader.ini" | cut -d= -f2- | xargs)"
	echo "==> DMR donor: using saved donor -> $saved"
else
	echo "==> DMR donor: NONE registered — firmware will flash as FM-only."
	echo "   (pass -s /path/to/official_V26.45.bin once to enable DMR)"
fi

# ---- flash ----------------------------------------------------------------
echo "==> Model:    $MODEL"
echo "==> Firmware: $FIRMWARE"
echo "==> Make sure the radio is in DFU mode (power on holding PTT + SK1)."
echo
set -x
exec python3 "$LOADER" -m "$MODEL" \
	"${DONOR_ARGS[@]+"${DONOR_ARGS[@]}"}" \
	"${LANG_ARGS[@]+"${LANG_ARGS[@]}"}" \
	-f "$FIRMWARE"
