#pragma once
// CSG node tree: the resolved geometric program, independent of any kernel.
// The Evaluator produces this from the AST; the Kernel turns it into geometry.
#include <memory>
#include <string>
#include <vector>

enum class NodeKind {
  Cube, Sphere, Cylinder,          // 3D primitives
  Square, Circle, Polygon,         // 2D primitives (planar faces in z=0)
  LinearExtrude, RotateExtrude,    // 2D -> 3D
  Translate, Rotate, Scale, Mirror,// transforms
  Union, Difference, Intersection, // booleans
  Fillet,                          // B-Rep edge rounding (beyond OpenSCAD)
  Attach,                          // seat children onto a queried face (frames/datums)
  Group                            // implicit union of children
};

struct Node {
  NodeKind kind;
  std::vector<std::shared_ptr<Node>> children;
  explicit Node(NodeKind k) : kind(k) {}
  virtual ~Node() = default;
};
using NodePtr = std::shared_ptr<Node>;

struct CubeNode : Node {
  double x = 1, y = 1, z = 1;
  bool center = false;
  CubeNode() : Node(NodeKind::Cube) {}
};

struct SphereNode : Node {
  double r = 1;
  int fn = 0;  // resolution hint (0 = auto)
  SphereNode() : Node(NodeKind::Sphere) {}
};

struct CylinderNode : Node {
  double h = 1, r1 = 1, r2 = 1;
  bool center = false;
  int fn = 0;
  CylinderNode() : Node(NodeKind::Cylinder) {}
};

// ---- 2D primitives (built as planar faces in the z=0 plane) ----
struct SquareNode : Node {
  double x = 1, y = 1;
  bool center = false;
  SquareNode() : Node(NodeKind::Square) {}
};

struct CircleNode : Node {
  double r = 1;
  int fn = 0;  // fn>=3 -> regular n-gon (OpenSCAD hexagon idiom); else exact circle
  CircleNode() : Node(NodeKind::Circle) {}
};

struct PolygonNode : Node {
  std::vector<std::pair<double, double>> points;  // single closed path
  PolygonNode() : Node(NodeKind::Polygon) {}
};

// ---- 2D -> 3D ----
struct LinearExtrudeNode : Node {  // straight prism along +Z
  double height = 1;
  bool center = false;
  LinearExtrudeNode() : Node(NodeKind::LinearExtrude) {}
};

struct RotateExtrudeNode : Node {  // revolve the profile about the Z axis
  double angle = 360;  // degrees
  RotateExtrudeNode() : Node(NodeKind::RotateExtrude) {}
};

struct TranslateNode : Node {
  double x = 0, y = 0, z = 0;
  TranslateNode() : Node(NodeKind::Translate) {}
};

// Either Euler angles (axisAngle=false: rotate([x,y,z]) degrees, applied X then
// Y then Z) or axis-angle (axisAngle=true: rotate(angle, [ax,ay,az])).
struct RotateNode : Node {
  bool axisAngle = false;
  double x = 0, y = 0, z = 0;          // Euler degrees
  double angle = 0, ax = 0, ay = 0, az = 1;  // axis-angle
  RotateNode() : Node(NodeKind::Rotate) {}
};

struct ScaleNode : Node {
  double x = 1, y = 1, z = 1;
  ScaleNode() : Node(NodeKind::Scale) {}
};

struct MirrorNode : Node {  // reflect across plane through origin with this normal
  double x = 1, y = 0, z = 0;
  MirrorNode() : Node(NodeKind::Mirror) {}
};

struct BooleanNode : Node {  // Union / Difference / Intersection / Group
  explicit BooleanNode(NodeKind k) : Node(k) {}
};

// Round edges of the (union of) children. The selector is the declarative query
// over which the fillet applies: rung 1 is All (no selection); later rungs add
// Convex / datum-relative / predicate selection without changing this node shape.
struct FilletNode : Node {
  double r = 1.0;
  enum class Sel { All, Convex, Concave } sel = Sel::All;
  FilletNode() : Node(NodeKind::Fillet) {}
};

// Assembly by datum frames -- the constructive dual of fillet's selection query.
// The FIRST child is the parent; the rest are seated onto the parent's `on` face by
// coinciding datum frames (centroid + outward normal), derived exactly from the
// B-Rep faces. `on`/`from` name a face by its outward-normal direction word
// (top|bottom|left|right|front|back); `from` defaults to the opposite of `on`, so a
// child seats bottom-onto-top by default. Faces are re-queried against the freshly
// built solids each rebuild -- a pure query, never a stateful pick.
struct AttachNode : Node {
  std::string on = "top";  // which parent face provides the frame
  std::string from;        // which child face mates (empty => opposite of `on`)
  AttachNode() : Node(NodeKind::Attach) {}
};
