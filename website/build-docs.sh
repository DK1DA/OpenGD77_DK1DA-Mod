#!/usr/bin/env bash
#
# Build the OpenGD77_DK1DA-Mod documentation website (MkDocs Material) entirely in Docker.
# Source markdown lives in the repo root; this assembles it into _build/docs and
# renders a static, self-contained HTML site with client-side search into
# website/_build/site.
#
# Usage:  ./website/build-docs.sh          # build
#         ./website/build-docs.sh serve    # live-reload preview on http://localhost:8000
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
SRC="$HERE/_build/docs"
IMAGE="squidfunk/mkdocs-material:9"

# ---- assemble the docs source tree from the root markdown -----------------
rm -rf "$HERE/_build"
mkdir -p "$SRC/images"

cp "$HERE/home.md"      "$SRC/index.md"
cp "$ROOT/MANUAL.md"    "$SRC/MANUAL.md"
cp "$ROOT/TOOLS.md"     "$SRC/TOOLS.md"
cp "$ROOT/CHANGELOG.md" "$SRC/CHANGELOG.md"
cp "$ROOT"/docs/images/*.png "$SRC/images/" 2>/dev/null || true

# MANUAL.md links back to README.md (a repo file, not a site page) -> point it at
# the site home instead.
sed -i.bak 's#(README.md)#(index.md)#g' "$SRC/MANUAL.md" && rm -f "$SRC/MANUAL.md.bak"

# Optional public URL (sitemap / canonical only). Set SITE_URL to override the
# placeholder in mkdocs.yml — the site works at any URL regardless.
ENVARGS=()
if [[ -n "${SITE_URL:-}" ]]; then
	echo "==> Site URL: $SITE_URL"
	ENVARGS=(-e "SITE_URL=$SITE_URL")
fi

# ---- build (or serve) -----------------------------------------------------
if [[ "${1:-}" == "serve" ]]; then
	echo "==> Serving docs on http://localhost:8000 (Ctrl-C to stop)"
	docker run --rm -it -p 8000:8000 "${ENVARGS[@]+"${ENVARGS[@]}"}" -v "$HERE":/docs "$IMAGE" serve -a 0.0.0.0:8000
else
	echo "==> Building docs site"
	docker run --rm "${ENVARGS[@]+"${ENVARGS[@]}"}" -v "$HERE":/docs "$IMAGE" build
	echo
	echo "==> Done. Static site (upload the *contents* of this folder):"
	echo "    $HERE/_build/site"
fi
