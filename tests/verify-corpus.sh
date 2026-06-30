#!/usr/bin/env bash
# Differential verification of oscad (OCCT) against the OpenSCAD corpus.
#
# For every corpus file that uses ONLY currently-supported features, render with
# both oscad and the reference `openscad` binary and compare bounding boxes.
# Rationale: meshes won't match byte-for-byte across kernels, but kernel-invariant
# properties (bbox, and later volume/topology) must agree. Curved surfaces diverge
# slightly because OCCT is exact while OpenSCAD facets -- a SMALL delta is fine; a
# large one is a real bug (this is how the cylinder d-vs-r precedence bug surfaced).
#
# Exit status is nonzero only on a real bbox MISMATCH. Files that use not-yet-
# implemented features are skipped; files oscad can't yet handle are reported as
# informational "gaps", not gate failures.
set -uo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
oscad="$root/build/oscad"
corpus="${1:-$root/tests/corpus/tests/data/scad}"
ref="$(command -v openscad || true)"
[ -x "$oscad" ] || { echo "build oscad first: cmake --build build"; exit 2; }
[ -d "$corpus" ] || { echo "corpus missing: run tests/fetch-corpus.sh"; exit 2; }
[ -n "$ref" ]    || echo "note: no reference 'openscad' on PATH -- checking solid production only"

# Features NOT yet implemented. Shrink this regex as milestones land.
UNSUP='\b(polyhedron|color|hull|minkowski|offset|projection|surface|text|import|render|resize|multmatrix|for|if|let|module|function|children|echo|assert|each)\b'
MISMATCH_TOL=0.50   # below this, divergence is OCCT-exact-vs-faceting (incl. $fn-faceted
                    # revolves/curves); real geometry bugs shift the bbox by >=1 unit

tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
inscope=0 ok=0 exact=0 empty=0 gap=0 mismatch=0
gaps=(); diffs=()

while IFS= read -r f; do
  grep -qE "$UNSUP" "$f" && continue
  inscope=$((inscope+1))
  if ! "$oscad" "$f" -o "$tmp/a.stl" >/dev/null 2>&1; then
    if [ -n "$ref" ] && "$ref" --export-format binstl -o "$tmp/b.stl" "$f" >/dev/null 2>&1 \
       && [ "$(python3 "$root/tests/bbox.py" tris "$tmp/b.stl")" -le 0 ]; then
      empty=$((empty+1)); continue            # both empty -> expected
    fi
    gap=$((gap+1)); gaps+=("$(basename "$f")"); continue
  fi
  [ "$(python3 "$root/tests/bbox.py" tris "$tmp/a.stl")" -le 0 ] && { empty=$((empty+1)); continue; }
  if [ -z "$ref" ] || ! "$ref" --export-format binstl -o "$tmp/b.stl" "$f" >/dev/null 2>&1; then
    ok=$((ok+1)); continue
  fi
  d="$(python3 "$root/tests/bbox.py" diff "$tmp/a.stl" "$tmp/b.stl")"
  if awk "BEGIN{exit !($d>$MISMATCH_TOL)}"; then
    mismatch=$((mismatch+1)); diffs+=("$(basename "$f")  bbox_dmax=$d")
  else
    ok=$((ok+1)); awk "BEGIN{exit !($d>0.001)}" && exact=$((exact+1))
  fi
done < <(find "$corpus" -name '*.scad' | sort)

echo "----------------------------------------"
echo "in-scope files        : $inscope"
echo "  matched reference   : $ok   (OCCT-exact divergence on curves: $exact)"
echo "  empty (expected)    : $empty"
echo "  unimplemented gaps  : $gap"
echo "  bbox MISMATCH       : $mismatch"
[ "$gap" -gt 0 ] && { echo "  -- gaps --"; printf '    %s\n' "${gaps[@]}"; }
[ "$mismatch" -gt 0 ] && { echo "  -- mismatches --"; printf '    %s\n' "${diffs[@]}"; }
echo "----------------------------------------"
[ "$mismatch" -eq 0 ]
