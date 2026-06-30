#pragma once
// CSG node tree: the resolved geometric program, independent of any kernel.
// The Evaluator produces this from the AST; the Kernel turns it into geometry.
#include <memory>
#include <vector>

enum class NodeKind {
  Cube, Sphere, Cylinder,          // 3D primitives
  Translate, Rotate, Scale, Mirror,// transforms
  Union, Difference, Intersection, // booleans
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
