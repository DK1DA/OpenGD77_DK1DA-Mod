#!/usr/bin/env bash
#
# Create a Forgejo release for a tag and attach the built firmware binary.
#
# Requires a Forgejo API token in the FORGEJO_TOKEN env var
# (Forgejo -> Settings -> Applications -> Generate New Token, scope: write:repository).
# The release notes are taken from the matching "## <version>" section of CHANGELOG.md.
#
# Usage:  FORGEJO_TOKEN=xxxxx scripts/forgejo-release.sh v0.1.0
#
set -euo pipefail

TAG="${1:?usage: FORGEJO_TOKEN=... $0 <tag, e.g. v0.1.0>}"
VER="${TAG#v}"
API="https://git.dk1da.de/api/v1/repos/DK1DA/OpenGD77_DK1DA-Mod"
BIN="MDUV380_firmware/build/OpenMDUV380.bin"
: "${FORGEJO_TOKEN:?set FORGEJO_TOKEN (Forgejo -> Settings -> Applications, scope write:repository)}"

cd "$(dirname "${BASH_SOURCE[0]}")/.."
[ -f "$BIN" ] || { echo "Missing $BIN - run ./build-firmware.sh first"; exit 1; }

# Release body = the CHANGELOG section for this version.
NOTES="$(awk -v v="## ${VER}" '$0==v{f=1} f&&/^## /&&$0!=v{exit} f{print}' CHANGELOG.md)"
[ -n "$NOTES" ] || NOTES="OpenGD77_DK1DA-Mod ${TAG}"

# Create the release (JSON body built safely via python).
BODY="$(python3 -c 'import json,sys; print(json.dumps({"tag_name":sys.argv[1],"name":"OpenGD77_DK1DA-Mod "+sys.argv[1],"body":sys.argv[2],"draft":False,"prerelease":False}))' "$TAG" "$NOTES")"
RESP="$(curl -fsS -X POST "$API/releases" -H "Authorization: token $FORGEJO_TOKEN" -H "Content-Type: application/json" -d "$BODY")"
ID="$(printf '%s' "$RESP" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("id",""))')"
[ -n "$ID" ] || { echo "Failed to create release:"; echo "$RESP"; exit 1; }
echo "Created release $TAG (id=$ID)"

# Attach the firmware binary.
curl -fsS -X POST "$API/releases/$ID/assets?name=OpenMDUV380.bin" \
	-H "Authorization: token $FORGEJO_TOKEN" -F "attachment=@$BIN" >/dev/null
echo "Attached $BIN"
echo "Done -> https://git.dk1da.de/DK1DA/OpenGD77_DK1DA-Mod/releases/tag/$TAG"
