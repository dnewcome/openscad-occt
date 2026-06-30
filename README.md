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
| Language    | numbers, vectors, arithmetic (`+ - * /`), `name = expr;`, named/positional args, `$fn` (parsed) |
| Output      | binary **STL** (tessellated) and **STEP** (exact B-Rep) |

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
