#!/usr/bin/env bash
#
# Build and publish an OpenGD77_DK1DA-Mod release on GitHub, attaching the firmware binaries.
#
# Requires the GitHub CLI (`gh`), authenticated with push access to the repo (`gh auth login`).
# Release notes are taken from the matching "## <version>" section of CHANGELOG.md.
# If the release already exists it is reused; assets of the same name are replaced.
# The tag must already exist on GitHub (git push --tags).
#
# Usage:  scripts/github-release.sh <tag> [platform ...]
#   platform: MDUV380 | DM1701 | all   (default: all)
# Examples:
#   scripts/github-release.sh v0.1.0            # build + attach both binaries
#   scripts/github-release.sh v0.1.0 MDUV380    # just the MD-UV380 binary
#
set -euo pipefail

TAG="${1:?usage: $0 <tag> [MDUV380|DM1701|all]}"; shift || true
VER="${TAG#v}"
REPO="${GITHUB_REPO:-DK1DA/OpenGD77_DK1DA-Mod}"

cd "$(dirname "${BASH_SOURCE[0]}")/.."

# Resolve requested platforms (default / "all" -> both).
PLATFORMS=("$@")
if [ ${#PLATFORMS[@]} -eq 0 ] || [ "${PLATFORMS[0]}" = "all" ]; then
	PLATFORMS=(MDUV380 DM1701)
fi
bin_for() { case "$1" in MDUV380) echo OpenMDUV380 ;; DM1701) echo OpenDM1701 ;; *) return 1 ;; esac; }

# Sanity check: the on-screen version should match the tag.
FW_VER="$(grep -oE '"[0-9]+\.[0-9]+\.[0-9]+"' MDUV380_firmware/application/include/dk1daModVersion.h | head -1 | tr -d '"')"
[ "$FW_VER" = "$VER" ] || echo "WARNING: dk1daModVersion.h is $FW_VER but tag is $VER — bump it before releasing?"

# Build each platform and stage its binary (clean builds overwrite build/).
STAGE="$(mktemp -d)"; trap 'rm -rf "$STAGE"' EXIT
for P in "${PLATFORMS[@]}"; do
	OUT="$(bin_for "$P")" || { echo "Unknown platform '$P' (use MDUV380, DM1701 or all)"; exit 1; }
	echo "==> Building $P"
	./build-firmware.sh "$P" >/dev/null
	cp "MDUV380_firmware/build/${OUT}.bin" "$STAGE/${OUT}.bin"
done

# Release notes = the CHANGELOG section for this version.
NOTES="$(awk -v v="## ${VER}" '$0==v{f=1} f&&/^## /&&$0!=v{exit} f{print}' CHANGELOG.md)"
[ -n "$NOTES" ] || NOTES="OpenGD77_DK1DA-Mod ${TAG}"
NOTES_FILE="$STAGE/notes.md"; printf '%s\n' "$NOTES" > "$NOTES_FILE"

# Everything before v1.0.0 is flagged as a pre-release.
if [ "${VER%%.*}" = "0" ]; then PRERELEASE=true; else PRERELEASE=false; fi

# Create the release, or reuse it if it already exists.
if gh release view "$TAG" -R "$REPO" >/dev/null 2>&1; then
	gh release edit "$TAG" -R "$REPO" --prerelease="$PRERELEASE" --notes-file "$NOTES_FILE" >/dev/null
	echo "==> Reusing existing release $TAG (prerelease=$PRERELEASE)"
else
	gh release create "$TAG" -R "$REPO" --verify-tag --title "OpenGD77_DK1DA-Mod $TAG" \
		--notes-file "$NOTES_FILE" --prerelease="$PRERELEASE" --latest="$( [ "$PRERELEASE" = true ] && echo false || echo true )" >/dev/null
	echo "==> Created release $TAG (prerelease=$PRERELEASE)"
fi

# Attach the staged binaries, replacing existing assets of the same name.
gh release upload "$TAG" -R "$REPO" --clobber "$STAGE"/*.bin
for f in "$STAGE"/*.bin; do echo "==> Attached $(basename "$f")"; done

echo "==> Done -> https://github.com/$REPO/releases/tag/$TAG"
