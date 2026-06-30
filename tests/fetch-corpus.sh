#!/usr/bin/env bash
# Fetch the OpenSCAD .scad test corpus into tests/corpus/ (git-ignored).
#
# We use OpenSCAD's models as verification INPUTS only. OpenSCAD is GPL-2.0+, so
# the corpus is kept OUT of this repo's tree (downloaded on demand, git-ignored)
# to preserve clean-room separation and avoid license entanglement. Their golden
# PNG/STL outputs are kernel-specific and NOT reused; we verify differentially
# against the reference `openscad` binary instead (see verify-corpus.sh).
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
dest="$root/tests/corpus"
if [ -d "$dest/.git" ]; then
  echo "corpus already present at $dest (delete it to re-fetch)"; exit 0
fi
rm -rf "$dest"
git clone --no-checkout --depth 1 --filter=blob:none \
  https://github.com/openscad/openscad.git "$dest"
git -C "$dest" sparse-checkout set tests/data/scad
git -C "$dest" checkout
echo "corpus ready: $dest/tests/data/scad ($(find "$dest" -name '*.scad' | wc -l) files)"
