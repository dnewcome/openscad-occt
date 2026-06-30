#include "Kernel.h"

#include <cmath>
#include <stdexcept>

#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_GTrsf.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRepBuilderAPI_Transform.hxx>

namespace {

constexpr double kDeg2Rad = M_PI / 180.0;

TopoDS_Shape applyTrsf(const TopoDS_Shape& s, const gp_Trsf& t) {
  if (s.IsNull()) return s;
  return BRepBuilderAPI_Transform(s, t, /*copy=*/Standard_True).Shape();
}

// Fuse all children into one shape (skipping null/empty subtrees).
TopoDS_Shape unionOf(const std::vector<NodePtr>& kids);  // fwd

// ---- primitives ----
TopoDS_Shape makeCube(const CubeNode& c) {
  if (c.x <= 0 || c.y <= 0 || c.z <= 0) return {};
  double x0 = c.center ? -c.x / 2 : 0;
  double y0 = c.center ? -c.y / 2 : 0;
  double z0 = c.center ? -c.z / 2 : 0;
  return BRepPrimAPI_MakeBox(gp_Pnt(x0, y0, z0), c.x, c.y, c.z).Shape();
}

TopoDS_Shape makeSphere(const SphereNode& s) {
  if (s.r <= 0) return {};
  return BRepPrimAPI_MakeSphere(s.r).Shape();
}

TopoDS_Shape makeCylinder(const CylinderNode& c) {
  if (c.h <= 0 || (c.r1 <= 0 && c.r2 <= 0)) return {};
  double zbase = c.center ? -c.h / 2 : 0;
  gp_Ax2 axis(gp_Pnt(0, 0, zbase), gp_Dir(0, 0, 1));
  if (std::abs(c.r1 - c.r2) < 1e-12)
    return BRepPrimAPI_MakeCylinder(axis, c.r1, c.h).Shape();
  return BRepPrimAPI_MakeCone(axis, c.r1, c.r2, c.h).Shape();
}

// ---- transforms ----
TopoDS_Shape makeTranslate(const TranslateNode& n) {
  gp_Trsf t;
  t.SetTranslation(gp_Vec(n.x, n.y, n.z));
  return applyTrsf(unionOf(n.children), t);
}

TopoDS_Shape makeRotate(const RotateNode& n) {
  gp_Trsf t;
  gp_Pnt o(0, 0, 0);
  if (n.axisAngle) {
    if (n.ax == 0 && n.ay == 0 && n.az == 0) return unionOf(n.children);  // no axis
    t.SetRotation(gp_Ax1(o, gp_Dir(n.ax, n.ay, n.az)), n.angle * kDeg2Rad);
  } else {
    // Euler: apply X, then Y, then Z  =>  matrix Rz * Ry * Rx.
    gp_Trsf rx, ry, rz;
    rx.SetRotation(gp_Ax1(o, gp_Dir(1, 0, 0)), n.x * kDeg2Rad);
    ry.SetRotation(gp_Ax1(o, gp_Dir(0, 1, 0)), n.y * kDeg2Rad);
    rz.SetRotation(gp_Ax1(o, gp_Dir(0, 0, 1)), n.z * kDeg2Rad);
    t = rz;
    t.Multiply(ry);
    t.Multiply(rx);
  }
  return applyTrsf(unionOf(n.children), t);
}

TopoDS_Shape makeScale(const ScaleNode& n) {
  TopoDS_Shape s = unionOf(n.children);
  if (s.IsNull()) return s;
  gp_GTrsf g;  // identity by default
  g.SetValue(1, 1, n.x);
  g.SetValue(2, 2, n.y);
  g.SetValue(3, 3, n.z);
  return BRepBuilderAPI_GTransform(s, g, /*copy=*/Standard_True).Shape();
}

TopoDS_Shape makeMirror(const MirrorNode& n) {
  if (n.x == 0 && n.y == 0 && n.z == 0) return unionOf(n.children);
  gp_Trsf t;
  t.SetMirror(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(n.x, n.y, n.z)));
  return applyTrsf(unionOf(n.children), t);
}

// ---- booleans ----
TopoDS_Shape makeDifference(const std::vector<NodePtr>& kids) {
  TopoDS_Shape first;
  size_t i = 0;
  for (; i < kids.size(); ++i) {
    first = buildShape(*kids[i]);
    if (!first.IsNull()) { ++i; break; }
  }
  if (first.IsNull()) return first;
  TopoDS_Shape tools;
  for (; i < kids.size(); ++i) {
    TopoDS_Shape s = buildShape(*kids[i]);
    if (s.IsNull()) continue;
    tools = tools.IsNull() ? s : BRepAlgoAPI_Fuse(tools, s).Shape();
  }
  if (tools.IsNull()) return first;
  return BRepAlgoAPI_Cut(first, tools).Shape();
}

TopoDS_Shape makeIntersection(const std::vector<NodePtr>& kids) {
  TopoDS_Shape acc;
  for (const auto& c : kids) {
    TopoDS_Shape s = buildShape(*c);
    if (s.IsNull()) return {};  // intersection with empty is empty
    acc = acc.IsNull() ? s : BRepAlgoAPI_Common(acc, s).Shape();
  }
  return acc;
}

}  // namespace

namespace {
TopoDS_Shape unionOf(const std::vector<NodePtr>& kids) {
  TopoDS_Shape acc;
  for (const auto& c : kids) {
    TopoDS_Shape s = buildShape(*c);
    if (s.IsNull()) continue;
    acc = acc.IsNull() ? s : BRepAlgoAPI_Fuse(acc, s).Shape();
  }
  return acc;
}
}  // namespace

TopoDS_Shape buildShape(const Node& node) {
  switch (node.kind) {
    case NodeKind::Cube:      return makeCube(static_cast<const CubeNode&>(node));
    case NodeKind::Sphere:    return makeSphere(static_cast<const SphereNode&>(node));
    case NodeKind::Cylinder:  return makeCylinder(static_cast<const CylinderNode&>(node));
    case NodeKind::Translate: return makeTranslate(static_cast<const TranslateNode&>(node));
    case NodeKind::Rotate:    return makeRotate(static_cast<const RotateNode&>(node));
    case NodeKind::Scale:     return makeScale(static_cast<const ScaleNode&>(node));
    case NodeKind::Mirror:    return makeMirror(static_cast<const MirrorNode&>(node));
    case NodeKind::Union:
    case NodeKind::Group:        return unionOf(node.children);
    case NodeKind::Difference:   return makeDifference(node.children);
    case NodeKind::Intersection: return makeIntersection(node.children);
  }
  throw std::runtime_error("unhandled node kind");
}
