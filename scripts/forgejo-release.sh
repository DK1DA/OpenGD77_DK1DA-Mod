#!/usr/bin/env bash
#
# Build and publish a OpenGD77_DK1DA-Mod release on Forgejo, attaching the firmware binaries.
#
# Requires a Forgejo API token in FORGEJO_TOKEN
# (Forgejo -> Settings -> Applications -> Generate New Token, scope: write:repository).
# Release notes are taken from the matching "## <version>" section of CHANGELOG.md.
# If the release already exists it is reused; assets of the same name are replaced.
#
# Usage:  FORGEJO_TOKEN=xxx scripts/forgejo-release.sh <tag> [platform ...]
#   platform: MDUV380 | DM1701 | all   (default: all)
# Examples:
#   scripts/forgejo-release.sh v0.1.0            # build + attach both binaries
#   scripts/forgejo-release.sh v0.1.0 MDUV380    # just the MD-UV380 binary
#
set -euo pipefail

TAG="${1:?usage: FORGEJO_TOKEN=... $0 <tag> [MDUV380|DM1701|all]}"; shift || true
VER="${TAG#v}"
API="https://git.dk1da.de/api/v1/repos/DK1DA/OpenGD77_DK1DA-Mod"
: "${FORGEJO_TOKEN:?set FORGEJO_TOKEN (Forgejo -> Settings -> Applications, scope write:repository)}"

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

# Create the release, or reuse it if it already exists.
ID="$(curl -sS "$API/releases/tags/$TAG" -H "Authorization: token $FORGEJO_TOKEN" | python3 -c '
import json,sys
try:
    d=json.load(sys.stdin); print(d.get("id","") if isinstance(d,dict) else "")
except Exception:
    print("")')"
# Everything before v1.0.0 is flagged as a pre-release.
if [ "${VER%%.*}" = "0" ]; then PRERELEASE=true; else PRERELEASE=false; fi

if [ -z "$ID" ]; then
	BODY="$(python3 -c 'import json,sys; print(json.dumps({"tag_name":sys.argv[1],"name":"OpenGD77_DK1DA-Mod "+sys.argv[1],"body":sys.argv[2],"draft":False,"prerelease":(sys.argv[3]=="true")}))' "$TAG" "$NOTES" "$PRERELEASE")"
	ID="$(curl -fsS -X POST "$API/releases" -H "Authorization: token $FORGEJO_TOKEN" -H "Content-Type: application/json" -d "$BODY" | python3 -c 'import json,sys;print(json.load(sys.stdin)["id"])')"
	echo "==> Created release $TAG (id=$ID, prerelease=$PRERELEASE)"
else
	# Keep the pre-release flag correct on re-runs.
	curl -fsS -X PATCH "$API/releases/$ID" -H "Authorization: token $FORGEJO_TOKEN" -H "Content-Type: application/json" \
		-d "$(python3 -c 'import json,sys; print(json.dumps({"prerelease":(sys.argv[1]=="true")}))' "$PRERELEASE")" >/dev/null
	echo "==> Reusing existing release $TAG (id=$ID, prerelease=$PRERELEASE)"
fi

# Attach each staged binary, replacing an existing asset of the same name.
ASSETS="$(curl -fsS "$API/releases/$ID/assets" -H "Authorization: token $FORGEJO_TOKEN")"
for f in "$STAGE"/*.bin; do
	name="$(basename "$f")"
	aid="$(printf '%s' "$ASSETS" | python3 -c 'import json,sys; n=sys.argv[1]; print(next((str(a["id"]) for a in json.load(sys.stdin) if a.get("name")==n),""))' "$name")"
	[ -z "$aid" ] || curl -fsS -X DELETE "$API/releases/$ID/assets/$aid" -H "Authorization: token $FORGEJO_TOKEN" >/dev/null
	curl -fsS -X POST "$API/releases/$ID/assets?name=$name" -H "Authorization: token $FORGEJO_TOKEN" -F "attachment=@$f" >/dev/null
	echo "==> Attached $name"
done

echo "==> Done -> https://git.dk1da.de/DK1DA/OpenGD77_DK1DA-Mod/releases/tag/$TAG"
