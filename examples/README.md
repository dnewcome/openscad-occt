# Examples — why a B-Rep kernel matters

Four real, machinable parts. Each is a single `.scad` file that produces an **exact
STEP solid** (named analytic surfaces — cylinders, cones, tori — not triangles) plus a
mesh STL for preview. Two of them use **declarative fillets** that OpenSCAD cannot
express at all.

Build any of them:

```sh
../build/oscad bracket.scad -o bracket.stl --step bracket.step
```

The headline: open these `.step` files in FreeCAD / Fusion / SolidWorks or hand them to
a machinist's CAM, and they are *real solids with real faces*. OpenSCAD can only give
you a faceted STL — a bag of triangles with no surface a CAM tool can offset, no
cylinder a drill cycle can recognize.

| part | OpenSCAD output | oscad STEP output |
|------|-----------------|-------------------|
| pulley | 420 triangles, no surfaces | **8 exact surfaces** (2 cones, 3 cylinders, 3 planes) |
| hex standoff | 48 triangles, no surfaces | **10 exact surfaces** (1 cylinder, planar faces) |
| bracket | *can't express* (`fillet` unknown) | valid solid, 26 cylindrical + 12 spherical fillet faces |
| flange | *can't express* (`fillet` unknown) | valid solid incl. an exact **toroidal** weld fillet |

---

## bracket — declarative convex-edge fillet

![bracket](img/bracket.png)

```scad
fillet(1.5, edges="convex") union() {
  cube([40, 34, thickness]);
  translate([0, 2, 0]) cube([thickness, 30, 28]);
}
// ... minus three mounting holes
```

**Why it's great.** One line — `fillet(1.5, edges="convex")` — rounds *every outer
edge* and leaves the inner corner and the bolt holes sharp. The "which edges" is a
**pure query over the topology**, re-evaluated against the freshly-built solid, not an
interactive pick that breaks when you change a dimension. OpenSCAD has no `fillet` at
all; CadQuery/build123d can fillet but only by imperatively selecting faces. The STEP
carries 26 exact cylindrical edge-blends + 12 spherical corner patches — `valid=YES`.

**Honest footnote** (right in the file): the upright is inset 2 mm so its faces aren't
*coincident* with the base's. Exact B-Rep booleans/fillets are fragile on flush faces —
the first version failed loudly, and insetting is the same fix you'd make in any CAD
package. This is the real edge of the kernel, shown not hidden.

---

## flange — declarative concave (weld) fillet

![flange](img/flange.png)

```scad
fillet(3, edges="concave") union() {
  cylinder(h = 5,  r = 25);   // base disk
  cylinder(h = 25, r = 10);   // riser tube
}
// ... minus a bore and four bolt holes
```

**Why it's great.** `edges="concave"` selects *only* the reentrant junction where the
tube meets the base and rounds it — the classic **fillet weld**. The result is an exact
`TOROIDAL_SURFACE` in the STEP. Convex and concave selectors exactly partition a solid's
edges, so you address inside corners and outside corners independently, declaratively.
No mesh kernel gives you an exact weld fillet; no other DSL gives it to you this simply.

---

## pulley — exact surfaces of revolution

![pulley](img/pulley.png)

```scad
rotate_extrude()
  polygon([[3,0],[20,0],[20,4],[14,9],[20,14],[20,18],[3,18]]);
```

**Why it's great.** One revolved profile defines the whole part — and the V-groove walls
come out as **true `CONICAL_SURFACE`s**, the rim and bore as **true cylinders**. The bore
is free (the profile's inner radius). OpenSCAD would hand you 420 triangles approximating
those cones; oscad hands a lathe/CAM 8 exact surfaces it can actually cut to.

---

## hex standoff — the `$fn` idiom, exact

![hex standoff](img/hex_standoff.png)

```scad
difference() {
  linear_extrude(20) circle(r = 5, $fn = 6);    // exact hexagon
  translate([0,0,-1]) cylinder(h = 22, r = 1.6);// bore
}
```

**Why it's great.** `circle($fn=6)` is honored as an **exact regular hexagon** (six
planar faces), matching OpenSCAD's hexagon idiom — but extruded and bored it exports as a
clean exact STEP spacer (6 hex faces + 2 caps + 1 cylindrical bore), not a faceted
approximation. The SCAD muscle memory carries over; the output is manufacturing-grade.
