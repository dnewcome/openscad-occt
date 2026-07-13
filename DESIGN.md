# oscad — design notes & state of the experiment

A working memo: what this is, *why* it might be worth building, the conceptual
findings that shaped it, what exists today, how it's verified, and an honest read
on viability. Written as a thinking artifact — to decide whether to keep going.

---

## 1. The one-paragraph thesis

`oscad` is a clean-room, **declarative** SCAD-style language whose geometry kernel is
**OpenCASCADE (OCCT)** — a B-Rep (boundary-representation) kernel — instead of a mesh
kernel. The bet: you can keep OpenSCAD's simple, functional, no-state modelling feel
*and* get exact solids, native **STEP** export, and B-Rep features (fillets) that mesh
CSG can't express. So far the bet is holding.

---

## 2. Why it might be worth building (the gap)

Two axes that people conflate:

|                         | **Mesh / CSG**   | **B-Rep**                 |
|-------------------------|------------------|---------------------------|
| **Imperative / stateful** | JSCAD-ish      | CadQuery, build123d       |
| **Declarative / functional** | **OpenSCAD** | *← the empty quadrant*    |

- **CadQuery / build123d** are imperative: a fluent chain threading hidden state
  (a "current workplane", a "selection stack"). They transcribe the GUI feature-tree
  workflow into code (`moveTo` / `lineTo` / extrude / select-face / fillet).
- **OpenSCAD** is declarative: the model is a pure *expression* — referentially
  transparent, nameable, composable, no "current" anything. But its kernel is mesh.

The bottom-right quadrant — **declarative × B-Rep** — is essentially empty. Filling it
is the project.

**Why it's empty (the load-bearing reason).** Declarative means referential
transparency over your *inputs*. B-Rep's power is operating on *derived* geometry —
faces/edges that are *outputs* of earlier ops (the edge where this cut meets that
face). Referring to an output is what breaks referential transparency. OpenSCAD dodged
the whole problem by never creating referenceable derived features (mesh-CSG has
none — you re-derive everything from coordinates). To get B-Rep's power declaratively
you must re-introduce derived-feature reference *without* re-introducing state. That's
the hard part, and it's why nobody nailed this quadrant.

**Why the Python tools are complex** isn't fundamental: PythonOCC/OCP are
auto-generated 1:1 mirrors of OCCT's C++ API. The fair comparison to OpenSCAD is
build123d, which is close to OpenSCAD ergonomics — but lives in Python because of the
quadrant problem above, not because OCCT demands it.

---

## 3. Core design decisions

1. **Clean-room, not a fork.** OpenSCAD's `Geometry`/`GeometryVisitor` backend
   abstraction is *mesh-shaped* (`PolySet`/`Polygon2d` are the lingua franca). A B-Rep
   backend can't honestly implement that interface for all node types, so bolting OCCT
   onto OpenSCAD fights the abstraction. A fresh, small interpreter is the right shape.
2. **Declarative functional CSG tree.** The model is a pure expression. The front end
   (lexer/parser/evaluator) is kernel-agnostic and produces a pure-data node tree; only
   the kernel/export layers touch OCCT.
3. **Exact B-Rep, *not* F-Rep/SDF.** The maximally-declarative paradigm (signed
   distance fields: union = `min`, fillet = smooth-`min`) is gloriously simple but the
   *least* B-Rep — you mesh an implicit field and lose exactness/STEP. Explicitly out
   of scope. We stay exact.
4. **Selection as a pure query, never a stateful pick.** B-Rep feature ops (fillet,
   later shell/draft) take a *predicate over topology* (`edges="convex"`), evaluated
   against the freshly-built solid. This is the bridge that makes derived-feature
   reference declarative.
5. **Modules are values (shape + frames), not macros — and the language is a strict
   superset.** OpenSCAD `module`s are textual macros that emit geometry into the ambient
   transform; they return nothing, so a subassembly can't publish *where its features
   are*. We instead make an instantiated module a **part value** = `(shape, named
   frames)`. Called as a bare statement it emits its shape exactly as OpenSCAD does — the
   frames ride along, invisible — so every valid OpenSCAD program in the implemented
   subset keeps its exact meaning. Frames and the `attach`/`place` operators are
   *additive, opt-in* syntax old files never touch (the TypeScript-over-JavaScript move).
   Copying OpenSCAD's macro-module would foreclose all of this, so we design
   modules-as-values from the start.

---

## 4. Conceptual findings (the parts worth remembering)

**Fillet isn't "non-CSG" because of selection — it's surface closure.** Boolean CSG
has a closure property: every face of the result is a *trimmed piece of an input
primitive's face*. Booleans only cut existing surfaces; they never invent a new one. A
fillet introduces a surface that is *not a face of any input* — the rolling-ball blend
(a canal surface). That's the real obstruction. Corollary gradient:
- straight, constant-radius edge → blend is a cylinder + sphere corners → *expressible*
  in pure CSG (and `minkowski(cube, sphere(r))` rounds all convex edges this way);
- curved / variable-radius / smooth-corner blends → canal surface, not any primitive →
  genuinely not boolean-CSG. Needs offset + sweep machinery (which OCCT has).

**Selection in a *procedural* model is softer than in a GUI — and more robust.** A
predicate ("convex edges", "edges on the top face") re-evaluates against freshly-built
geometry on every rebuild. There's no stale interactive pick to break, so it sidesteps
much of CAD's notorious *topological-naming problem*. Declarative selection can be
*more* parametrically stable than the feature tree it competes with.

**Local origins / assembly are the *constructive dual* of selection.** OpenSCAD's
most-felt limitation — no local origin for a subassembly, no way to mate part B onto a
feature of part A without replaying A's internal transform arithmetic at the call site —
shares a root cause with the fillet-selection problem: geometry is opaque and transforms
flow only top-down, so you can never *address a derived feature*. Selection asks "which
sub-topology do I **operate on**?"; a datum/frame asks "which sub-topology do I **measure
a coordinate system from, and mate to**?" Both are pure queries over B-Rep topology.
B-Rep makes the datum *exact*: a planar face carries a real plane + normal + centroid, so
its frame is derivable — where BOSL2's anchors are recomputed from each primitive's
*declared bounding dimensions* because a mesh has no faces to anchor to. Mating is then
one `gp_Trsf` coinciding two frames, re-resolved on every rebuild — declarative assembly
by geometric predicate, the positioning face of the same query model as selection.

**Total vs partial operations.** `hull`/`minkowski`/`offset` are *total* — always
defined, never fail. B-Rep `fillet`/`loft`/`blend` are *partial* — they fail on tight
radii, coincident faces, awkward profiles. Partiality is what breaks the simple
declarative mental model, and it's the deepest reason simple DSLs historically stuck to
mesh. We confront it head-on by **failing loud** when an op can't complete, never
emitting a silently-wrong shape.

**`hull` is a mesh-native idiom B-Rep CAD abandoned.** 3D convex hull is absent from
Fusion / SolidWorks / Onshape (and OCCT has no hull primitive). Where "hull" appears
it's a 2D-sketch convenience. Reasons: exact hull of *curved* solids needs tangent
canal surfaces (hard); MCAD substitutes loft/fillet; hull is topologically destructive
(fights the feature tree). But hull of *polyhedral* inputs stays exact (planar faces),
and 2D hull is trivial — so a SCAD-on-OCCT tool could offer hull *exactly where
exactness is achievable* and fall back to facets only for curved inputs (which is what
OpenSCAD does anyway). Potential differentiator nobody else has.

**The `$fn` tension.** OpenSCAD's whole resolution model assumes curves are polygonal
approximations (`circle($fn=6)` is *intentionally* a hexagon). We keep curves exact by
default (a real circle, a real sphere), but honor `$fn≥3` for `circle` as an exact
regular n-gon, because the hexagon idiom is load-bearing in real `.scad` code. Sphere
and cylinder stay exact regardless of `$fn` (documented divergence).

---

## 5. Architecture

```
.scad source
  → Lexer        src/Lexer.*      tokens
  → Parser       src/Parser.*     AST            (src/Ast.h)
  → Evaluator    src/Evaluator.*  CSG node tree  (src/Node.h, values in src/Value.h)
  → Kernel       src/Kernel.*     OCCT TopoDS_Shape
  → Export       src/Export.*     BRepMesh→STL  |  STEPControl_Writer→STEP
```

Front end is kernel-agnostic (pure-data node tree). Only `Kernel.cc` and `Export.cc`
include OCCT — so a second kernel, or a "dump the tree" mode, could be added without
touching the language layer. ~1k lines total.

---

## 6. What works today

| Area          | Supported |
|---------------|-----------|
| 3D primitives | `cube`, `sphere`, `cylinder` (+cones via `r1`/`r2`) |
| 2D primitives | `square`, `circle` (`$fn≥3` → exact n-gon), `polygon` |
| 2D → 3D       | `linear_extrude(height, center)`, `rotate_extrude(angle)` |
| Transforms    | `translate`, `rotate` (Euler + axis-angle), `scale`, `mirror` |
| Booleans      | `union`, `difference`, `intersection` — 2D (faces-with-holes) and 3D |
| **Features**  | **`fillet(r, edges="all"\|"convex"\|"concave")`** — exact B-Rep edge rounding |
| **Assembly**  | **`attach(on=…[, from=…]) { parent; child… }`** — seat children onto a queried face by exact datum frames |
| Language      | numbers, vectors, strings, arithmetic, `name = expr;`, named/positional args, `$fn` |
| Output        | binary **STL** (tessellated) + **STEP** (exact B-Rep) |

Evidence the thesis holds:
- `cube(20) - sphere(12)` → STEP carries the bite as an exact `SPHERICAL_SURFACE`; a
  tapered cylinder as a `CONICAL_SURFACE`; a torus as a `TOROIDAL_SURFACE`. All valid
  single solids (`BRepCheck_Analyzer` = valid; round-trip through OCCT's STEP reader).
- `fillet(2) cube(20)` stays exactly 20 mm (in-place, unlike a grow-the-part
  Minkowski) and exports with 12 exact `CYLINDRICAL_SURFACE` edge blends + 8
  `SPHERICAL_SURFACE` corner patches.
- On a step solid (convex outer edges + one reentrant concave edge),
  `edges="convex"` and `edges="concave"` **exactly partition** the filletable edges
  (21 + 1 = 22 blends) — declarative selection working in code.
- `attach(on="top") { cube([40,40,10]); cylinder(h=12,r=8); }` seats the boss so its
  cap centroid lands on the plate-top centroid with a **0.0e0 gap** (double precision) —
  the constructive dual of selection, working in code. Positioning carries no global
  coordinate arithmetic: bump the plate to 25 mm thick and the boss follows to z=25
  automatically. Partial, like fillet: a cylinder onto a wall (no planar side face)
  fails loud rather than faking the seat.

---

## 7. Verification — differential, not golden

We verify against OpenSCAD's own ~580-file `.scad` corpus by **differential testing**
vs the reference `openscad` binary on **kernel-invariant properties** (bounding box).
Their golden PNG/STL are kernel-specific and not reused; meshes won't match
byte-for-byte, but invariants must. Curved surfaces legitimately diverge (OCCT exact
vs OpenSCAD faceted) — a small delta is expected, a large one is a real bug.

```
tests/fetch-corpus.sh    # corpus -> tests/corpus/ (git-ignored; OpenSCAD is GPL)
tests/verify-corpus.sh   # differential bbox gate + feature-completeness scoreboard
```

Current scoreboard:
```
in-scope files        : 81     # use only currently-supported features
  matched reference   : 50     # bbox agrees (16 OCCT-exact divergence on curves)
  unimplemented gaps  : 31     # features not yet built (fail loud)
  bbox MISMATCH       : 0
```

The harness doubles as a completeness meter: each milestone shrinks the `UNSUP`
feature list and the in-scope/match counts climb (53→81, 32→50 across M1→M2). It has
already paid for itself, catching real bugs:
- `cylinder(r=1, d=10)` honored `r` instead of `d` (OpenSCAD: diameter wins).
- `circle($fn=6)` came out round (`$fn` passed as a per-call arg wasn't read).
- Four "mismatches" that were unimplemented extrude options (`twist`/`scale`/`start`)
  silently emitting wrong shapes — now they fail loud.

---

## 8. Viability — honest read

**The sweet spot (genuinely unserved).** "Existing `.scad` / SCAD-language model →
exact STEP." Nothing does this. OpenSCAD can't emit exact STEP; build123d can but
you'd hand-rewrite in Python. For the large class of real parts that are CSG of
primitives (brackets, plates, enclosures), the OpenSCAD model fits perfectly *and*
OCCT hands you exact STEP. We do this today.

**A second, forward-looking angle: codegen.** A minimal grammar is a liability for a
human who wants fillets, but an *asset* when the code is generated. Producing valid
SCAD is trivial; producing valid build123d Python is much harder. "Simple generative
grammar in → exact manufacturable STEP out" is a combination only a SCAD-on-OCCT tool
gives you.

**The ceiling (where the simple-DSL dream has historically died).** The moment a user
wants the *reason* to prefer OCCT — selective fillets, shells, lofts on named faces —
the language must grow selection. We're betting selection-as-query climbs that ladder
gracefully (and `fillet(edges="convex")` is evidence it can). But far enough up,
arbitrary selection meets the topological-naming problem. How far real parts actually
need to climb is the open question.

**The real technical risk: boolean robustness on coincident geometry.** OCCT's exact
booleans are fragile exactly where scripted CSG is profligate — coincident faces,
tangencies, flush walls. We've only stress-tested clean analytic cases. This is the
80% that decides "genuinely useful" vs "neat demo," and we have not yet pushed on it.

**How to test viability cheaply (not yet done):** take a real part you'd want as STEP —
ideally pure CSG, no fillet — and run it through (a) oscad, (b) OpenSCAD (no exact
STEP), (c) build123d (exact, but hand-rewrite). If oscad is meaningfully less effort
for a part you care about, the niche is real. If every part you care about wants a
fillet on a specific face, the ceiling is closer than hoped.

---

## 9. Roadmap / open questions

Near-term, by leverage:
- **Datum-relative fillet selection** (`edges on top face`) — next rung of the
  declarative-query ladder; the real test of the thesis.
- **`hull()`** — exact for polyhedral/2D inputs, faceted fallback for curved; the
  idiom MCAD lacks.
- **Frames / datums + `attach`/`place`, and user `module`s as *values*** — the fix for
  OpenSCAD's no-local-origin gripe (§3.5, §4). Build order that de-risks the novel part
  first: **(1) DONE** — `attach(on=…[, from=…]) { parent; child… }` derives a frame from a
  queried face and coincides two solids with one `gp_Trsf` (exact, 0-gap, verified in
  STEP); a transform-module surface, so it works on inline children *today*, before user
  modules land. Remaining: (2) user `module`/`function` + `children`, an instantiated
  module returning a part-value (so a named `bracket()` can publish frames); (3) `frame`
  declarations inside modules → real named subassemblies; plus `spin=`/offset controls
  and datum disambiguation when several faces face the same way.
- **User `module` / `function`, `for`, `if`** — unlocks a large fraction of the
  remaining corpus (item above makes modules carry frames from day one).
- **`twist` / `scale` `linear_extrude`, `start` `rotate_extrude`** — currently fail
  loud; need `ThruSections`/sweep machinery.
- **Stronger verification invariants** — bbox is joined by `tests/step_check` (reads a
  STEP, runs `BRepCheck_Analyzer` → `brep_valid`/solid/shell counts); all six examples
  pass. Still want volume checks. Note the distinction it surfaced: **B-Rep validity ≠
  STL watertightness** — a valid solid with *spherical* fillet corners meshes to a
  non-watertight STL (OCCT tessellates faces independently → T-junctions where three
  blends meet at a corner; a single ring/toroidal fillet has none). Healing the STL
  export (stitched/shared-edge mesh) is future work; the exact B-Rep is unaffected.

Strategic questions to sit with:
1. How far up the selection ladder do *your* real parts actually need to go?
2. Is the value "my old `.scad` → STEP", or "generated SCAD → STEP", or both?
3. Where does OCCT's boolean fragility bite first on real (coincident-face) models —
   and is failing loud + a documented workaround an acceptable answer?
4. Does a no-fillet, pure-CSG-to-STEP tool already justify itself, with fillets as
   upside — or only with the feature layer?

---

*State as of the M2 milestone. Repo: https://github.com/dnewcome/openscad-occt*
