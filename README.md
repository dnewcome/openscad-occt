# oscad — OpenSCAD on the OpenCASCADE (OCCT) kernel

A clean-room, minimal [OpenSCAD](https://openscad.org)-style interpreter that uses
**OpenCASCADE Technology (OCCT)** as its CAD kernel instead of a mesh kernel
(CGAL/Manifold). Because OCCT is a **B-Rep** (boundary-representation) kernel,
geometry stays exact end-to-end: primitives are analytic surfaces, booleans are
exact, and models export to **STEP** as real solids — not tessellated meshes.

Why OCCT:
- **STEP/IGES export** — exact solids for real CAD interoperability.
- **Boolean robustness** — exact B-Rep booleans, no mesh coplanarity / non-manifold artifacts.

## Status — Milestone 1 (core CSG) ✅

| Area        | Supported |
|-------------|-----------|
| Primitives  | `cube`, `sphere`, `cylinder` (incl. cones via `r1`/`r2`) |
| Transforms  | `translate`, `rotate` (Euler + axis-angle), `scale`, `mirror` |
| Booleans    | `union`, `difference`, `intersection` (+ implicit top-level union) |
| Features    | `fillet(r)` / `round(r)` — exact B-Rep edge rounding of **all** edges (beyond OpenSCAD) |
| Language    | numbers, vectors, arithmetic (`+ - * /`), `name = expr;`, named/positional args, `$fn` (parsed) |
| Output      | binary **STL** (tessellated) and **STEP** (exact B-Rep) |

`fillet(r)` is the first rung of a *declarative feature* layer: selection expressed
as a pure query over topology rather than a stateful pick. Rung 1 is "all edges"
(no selection). `fillet(2) cube(20)` stays exactly 20mm (in-place, unlike a
grow-the-part Minkowski) and exports as STEP with exact `CYLINDRICAL_SURFACE` edge
blends + `SPHERICAL_SURFACE` corner patches. Fillet is *partial* — too large an `r`
for some face fails, and we surface that rather than emit a wrong shape. Next rungs:
`edges="convex"`, then datum-relative selection.

Validated: outputs are valid single solids (`BRepCheck_Analyzer` = valid, round-trip
through OCCT's STEP reader), bounding boxes match reference OpenSCAD, and curved
surfaces are carried *exactly* (e.g. a sphere bite exports as a `SPHERICAL_SURFACE`,
a tapered cylinder as a `CONICAL_SURFACE`).

### Not yet (future milestones)
2D shapes & extrudes (`square`/`circle`/`polygon`, `linear_extrude`/`rotate_extrude`),
user modules/functions, `for`/`if`, `hull`/`minkowski`/`offset`, `import`, text,
per-feature `$fn` honoring, and a GL preview GUI.

## Architecture

```
.scad source
  → Lexer        src/Lexer.*      tokens
  → Parser       src/Parser.*     AST            (src/Ast.h)
  → Evaluator    src/Evaluator.*  CSG node tree  (src/Node.h, values in src/Value.h)
  → Kernel       src/Kernel.*     OCCT TopoDS_Shape
  → Export       src/Export.*     BRepMesh→STL  |  STEPControl_Writer→STEP
```

The front-end (lexer/parser/evaluator) is kernel-agnostic: it produces a pure-data
CSG node tree. Only `Kernel.cc` and `Export.cc` touch OCCT, so a second kernel or a
"dump tree" mode could be added without disturbing the language layer.

## Build

Requires CMake ≥ 3.16, a C++17 compiler, and the OCCT 7.x dev packages:

```sh
sudo apt-get install -y libocct-foundation-dev libocct-modeling-data-dev \
                        libocct-modeling-algorithms-dev libocct-data-exchange-dev
cmake -B build -S .
cmake --build build
```

## Usage

```sh
oscad input.scad                      # -> input.stl
oscad input.scad -o out.stl           # STL only
oscad input.scad --step out.step      # STEP only
oscad input.scad -o out.stl --step out.step
oscad input.scad -o out.stl --deflection 0.05   # finer tessellation
```

Run the sample suite:

```sh
./tests/run.sh
```

## Verification against the OpenSCAD corpus

We verify differentially against OpenSCAD's own ~580-file `.scad` test corpus and
the reference `openscad` binary. Their golden PNG/STL outputs are kernel-specific
and *not* reused; instead we compare kernel-invariant properties (bounding box;
later volume/topology) between `oscad` (OCCT) and `openscad`. Curved surfaces
diverge slightly because OCCT is exact while OpenSCAD facets — a small delta is
expected; a large one is a real bug (this is how the `cylinder` d-vs-r precedence
bug was caught).

```sh
./tests/fetch-corpus.sh      # downloads corpus to tests/corpus/ (git-ignored, GPL)
./tests/verify-corpus.sh     # differential bbox gate; nonzero exit on a real mismatch
```

The corpus is fetched on demand and git-ignored (OpenSCAD is GPL-2.0+; keeping it
out of this tree preserves clean-room separation). The runner auto-skips files that
use not-yet-implemented features and reports the rest as a scoreboard:

```
in-scope files        : 53      # use only currently-supported features
  matched reference   : 32      # bbox agrees with openscad (8 OCCT-exact on curves)
  unimplemented gaps  : 21      # modifiers * ! # %, include/use, nan/inf, ...
  bbox MISMATCH       : 0
```

As each milestone lands, shrink the `UNSUP` feature regex in `verify-corpus.sh`
and the in-scope count grows — the corpus doubles as a feature-completeness
scoreboard.
