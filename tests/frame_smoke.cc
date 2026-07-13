// frame_smoke -- proof of the "frames / datums" mechanic at the kernel level.
//
// Thesis (DESIGN.md §3.5, §4): a datum is the *constructive dual* of selection.
// Just as fillet(edges="convex") picks a sub-topology to OPERATE on, an attach/place
// picks a face to MEASURE a coordinate frame from, and mates two solids by coinciding
// two frames with a single gp_Trsf. This program proves the mechanic exactly, on bare
// primitives, with no language change:
//
//   1. pick the base cube's TOP face by a normal query  (dual of edge selection)
//   2. pick the child cylinder's BOTTOM face the same way
//   3. derive an exact gp_Ax3 frame from each (centroid + outward normal + in-plane X)
//   4. compute the mate transform: coincide the two frames, normals OPPOSED (face-to-face)
//   5. apply, fuse, and verify: valid single solid, faces provably coincident, STEP out
//
// The point is exactness: the mate lands the child's cap centroid on the base's top-face
// centroid to within kernel tolerance -- something a bbox-anchored (mesh) model can only
// approximate. If this holds, wiring a surface syntax (attach/place) on top is plumbing.

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <STEPControl_Writer.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>
#include <Standard_Failure.hxx>

#include <cmath>
#include <cstdio>
#include <stdexcept>

// A datum frame derived from a face: where it is, and how it's oriented.
struct Frame {
  gp_Pnt origin;   // face centroid (area centre of mass)
  gp_Dir normal;   // outward normal (accounts for face orientation)
  gp_Dir xdir;     // an in-plane reference axis
  bool   valid = false;
};

// Derive an exact frame from a PLANAR face. Non-planar faces return {valid=false}.
static Frame frameOfFace(const TopoDS_Face& f) {
  Frame fr;
  BRepAdaptor_Surface surf(f, /*restriction*/ true);
  if (surf.GetType() != GeomAbs_Plane) return fr;  // only planar datums here

  gp_Pln pln = surf.Plane();
  gp_Dir n   = pln.Axis().Direction();
  if (f.Orientation() == TopAbs_REVERSED) n.Reverse();  // make it point OUT of the solid

  GProp_GProps props;
  BRepGProp::SurfaceProperties(f, props);

  fr.origin = props.CentreOfMass();
  fr.normal = n;
  fr.xdir   = pln.Position().XDirection();  // guaranteed perpendicular to n
  fr.valid  = true;
  return fr;
}

// Query: the planar face of `s` whose outward normal is closest to `want`.
// This is the positioning-side analogue of fillet's edge predicate -- a pure query
// over topology, re-resolved against the freshly built solid.
static Frame pickFaceByNormal(const TopoDS_Shape& s, const gp_Dir& want) {
  Frame best;
  double bestDot = -2.0;
  for (TopExp_Explorer ex(s, TopAbs_FACE); ex.More(); ex.Next()) {
    Frame fr = frameOfFace(TopoDS::Face(ex.Current()));
    if (!fr.valid) continue;
    double d = fr.normal.Dot(want);
    if (d > bestDot) { bestDot = d; best = fr; }
  }
  return best;
}

// The mate: rigid transform that lands `child` so its frame coincides with `base`'s.
// opposed=true -> the two outward normals end up antiparallel (a face-to-face seat).
static gp_Trsf mateTransform(const Frame& base, const Frame& child, bool opposed) {
  gp_Dir targetN = opposed ? gp_Dir(base.normal.Reversed()) : base.normal;
  // Right-handed source/target coordinate systems built the same way => the displacement
  // between them is a proper rigid motion (no reflection).
  gp_Ax3 from(child.origin, child.normal, child.xdir);
  gp_Ax3 to  (base.origin,  targetN,      base.xdir);
  gp_Trsf t;
  t.SetDisplacement(from, to);
  return t;
}

int main() {
  try {
    // Base: a 40x40x10 plate sitting on z=0. Child: a boss to seat on its top face.
    TopoDS_Shape base  = BRepPrimAPI_MakeBox(40.0, 40.0, 10.0).Shape();
    TopoDS_Shape child = BRepPrimAPI_MakeCylinder(8.0, 12.0).Shape();  // r=8, h=12, base at z=0

    Frame baseTop    = pickFaceByNormal(base,  gp_Dir(0, 0,  1));
    Frame childBottom = pickFaceByNormal(child, gp_Dir(0, 0, -1));
    if (!baseTop.valid || !childBottom.valid)
      throw std::runtime_error("could not resolve a planar datum face");

    printf("base top    frame: origin (%.3f %.3f %.3f)  normal (%.2f %.2f %.2f)\n",
           baseTop.origin.X(), baseTop.origin.Y(), baseTop.origin.Z(),
           baseTop.normal.X(), baseTop.normal.Y(), baseTop.normal.Z());
    printf("child bottom frame: origin (%.3f %.3f %.3f)  normal (%.2f %.2f %.2f)\n",
           childBottom.origin.X(), childBottom.origin.Y(), childBottom.origin.Z(),
           childBottom.normal.X(), childBottom.normal.Y(), childBottom.normal.Z());

    gp_Trsf t = mateTransform(baseTop, childBottom, /*opposed=*/true);
    TopoDS_Shape placed = BRepBuilderAPI_Transform(child, t, /*copy*/ true).Shape();

    // Verify coincidence: the child's bottom-cap centroid must land on the base top centroid.
    Frame seated = pickFaceByNormal(placed, gp_Dir(0, 0, -1));
    double gap = seated.origin.Distance(baseTop.origin);
    printf("seated child cap centroid: (%.6f %.6f %.6f)\n",
           seated.origin.X(), seated.origin.Y(), seated.origin.Z());
    printf("mate gap (should be ~0):   %.3e\n", gap);

    // Fuse and validate as a single B-Rep solid.
    TopoDS_Shape asm_ = BRepAlgoAPI_Fuse(base, placed).Shape();
    BRepCheck_Analyzer chk(asm_);
    printf("fused assembly valid:      %s\n", chk.IsValid() ? "YES" : "NO");

    STEPControl_Writer w;
    w.Transfer(asm_, STEPControl_AsIs);
    if (w.Write("frame_smoke.step") != IFSelect_RetDone)
      throw std::runtime_error("STEP write failed");
    printf("wrote frame_smoke.step\n");

    bool pass = gap < 1e-6 && chk.IsValid();
    printf("\n%s\n", pass ? "PASS: exact face-to-face mate from a topology query."
                          : "FAIL");
    return pass ? 0 : 1;
  } catch (const Standard_Failure& e) {
    printf("OCCT error: %s\n", e.GetMessageString());
    return 2;
  } catch (const std::exception& e) {
    printf("error: %s\n", e.what());
    return 2;
  }
}
