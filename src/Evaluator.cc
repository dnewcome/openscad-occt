#include "Evaluator.h"

#include <cmath>
#include <map>
#include <stdexcept>

#include "Value.h"

namespace {

using Scope = std::map<std::string, Value>;

// ---- expression evaluation ----
Value evalExpr(const Expr* e, const Scope& scope);

Value evalIdent(const IdentExpr* id, const Scope& scope) {
  if (id->name == "undef") return Value{};
  if (id->name == "PI") return Value::makeNumber(M_PI);
  auto it = scope.find(id->name);
  if (it == scope.end())
    throw std::runtime_error("undefined variable: " + id->name);
  return it->second;
}

Value evalUnary(const UnaryExpr* u, const Scope& scope) {
  Value v = evalExpr(u->operand.get(), scope);
  if (u->op == '!') return Value::makeBool(!v.asBool());
  // '-'
  if (v.isVector()) {
    std::vector<Value> r;
    for (auto& x : v.vec) r.push_back(Value::makeNumber(-x.asNumber()));
    return Value::makeVector(std::move(r));
  }
  return Value::makeNumber(-v.asNumber());
}

Value evalBinary(const BinaryExpr* b, const Scope& scope) {
  Value l = evalExpr(b->lhs.get(), scope);
  Value r = evalExpr(b->rhs.get(), scope);

  // vector (+|-) vector, componentwise
  if (l.isVector() && r.isVector() && (b->op == '+' || b->op == '-')) {
    if (l.vec.size() != r.vec.size())
      throw std::runtime_error("vector length mismatch in '" + std::string(1, b->op) + "'");
    std::vector<Value> out;
    for (size_t i = 0; i < l.vec.size(); ++i) {
      double a = l.vec[i].asNumber(), c = r.vec[i].asNumber();
      out.push_back(Value::makeNumber(b->op == '+' ? a + c : a - c));
    }
    return Value::makeVector(std::move(out));
  }
  // vector */ scalar  (and scalar * vector)
  auto scaleVec = [](const Value& vecv, double s, char op) {
    std::vector<Value> out;
    for (auto& x : vecv.vec)
      out.push_back(Value::makeNumber(op == '*' ? x.asNumber() * s : x.asNumber() / s));
    return Value::makeVector(std::move(out));
  };
  if (l.isVector() && r.isNumber() && (b->op == '*' || b->op == '/'))
    return scaleVec(l, r.num, b->op);
  if (l.isNumber() && r.isVector() && b->op == '*')
    return scaleVec(r, l.num, '*');

  double a = l.asNumber(), c = r.asNumber();
  switch (b->op) {
    case '+': return Value::makeNumber(a + c);
    case '-': return Value::makeNumber(a - c);
    case '*': return Value::makeNumber(a * c);
    case '/': return Value::makeNumber(a / c);
  }
  throw std::runtime_error("bad operator");
}

Value evalExpr(const Expr* e, const Scope& scope) {
  switch (e->kind) {
    case Expr::Kind::Number: return Value::makeNumber(static_cast<const NumberExpr*>(e)->value);
    case Expr::Kind::Bool:   return Value::makeBool(static_cast<const BoolExpr*>(e)->value);
    case Expr::Kind::String: return Value::makeString(static_cast<const StringExpr*>(e)->value);
    case Expr::Kind::Ident:  return evalIdent(static_cast<const IdentExpr*>(e), scope);
    case Expr::Kind::Unary:  return evalUnary(static_cast<const UnaryExpr*>(e), scope);
    case Expr::Kind::Binary: return evalBinary(static_cast<const BinaryExpr*>(e), scope);
    case Expr::Kind::Vector: {
      auto* v = static_cast<const VectorExpr*>(e);
      std::vector<Value> out;
      for (auto& el : v->elems) out.push_back(evalExpr(el.get(), scope));
      return Value::makeVector(std::move(out));
    }
  }
  throw std::runtime_error("bad expression");
}

// ---- resolved arguments ----
// Bind a call's positional+named args to a fixed positional parameter list.
struct Args {
  std::map<std::string, Value> by_name;

  bool has(const std::string& k) const { return by_name.count(k) != 0; }
  Value get(const std::string& k) const { return by_name.at(k); }
  Value getOr(const std::string& k, Value def) const {
    auto it = by_name.find(k);
    return it == by_name.end() ? def : it->second;
  }
  double num(const std::string& k, double def) const {
    return has(k) ? get(k).asNumber() : def;
  }
};

Args bindArgs(const CallStmt& call, const std::vector<std::string>& params, const Scope& scope) {
  Args a;
  size_t pos = 0;
  for (const auto& arg : call.args) {
    if (!arg.name.empty()) {
      a.by_name[arg.name] = evalExpr(arg.value.get(), scope);
    } else {
      if (pos >= params.size())
        throw std::runtime_error(call.name + "(): too many positional arguments");
      a.by_name[params[pos++]] = evalExpr(arg.value.get(), scope);
    }
  }
  return a;
}

// Resolve $fn: a per-call named arg ($fn=6) overrides the ambient scope value,
// matching OpenSCAD's special-variable (dynamic) scoping.
int resolveFn(const Args& a, const Scope& scope) {
  if (a.has("$fn") && a.get("$fn").isNumber()) return (int)a.get("$fn").asNumber();
  auto it = scope.find("$fn");
  if (it != scope.end() && it->second.isNumber()) return (int)it->second.asNumber();
  return 0;
}

// ---- statement execution -> CSG nodes ----
void execStmt(const Stmt* s, Scope& scope, std::vector<NodePtr>& out);

std::vector<NodePtr> execChildren(const CallStmt& call, Scope& parent) {
  Scope scope = parent;  // children see (a copy of) the enclosing scope
  std::vector<NodePtr> kids;
  for (const auto& c : call.children) execStmt(c.get(), scope, kids);
  return kids;
}

NodePtr buildPrimitive(const CallStmt& call, const Scope& scope) {
  const std::string& m = call.name;

  if (m == "cube") {
    Args a = bindArgs(call, {"size", "center"}, scope);
    auto n = std::make_shared<CubeNode>();
    Value size = a.getOr("size", Value::makeNumber(1));
    auto xyz = size.asVecN(3);
    n->x = xyz[0]; n->y = xyz[1]; n->z = xyz[2];
    n->center = a.getOr("center", Value::makeBool(false)).asBool();
    return n;
  }

  if (m == "sphere") {
    Args a = bindArgs(call, {"r"}, scope);  // d handled via name below
    auto n = std::make_shared<SphereNode>();
    if (a.has("d"))      n->r = a.get("d").asNumber() / 2.0;
    else if (a.has("r")) n->r = a.get("r").asNumber();
    else                 n->r = 1.0;
    n->fn = resolveFn(a, scope);
    return n;
  }

  if (m == "cylinder") {
    Args a = bindArgs(call, {"h", "r1", "r2"}, scope);
    auto n = std::make_shared<CylinderNode>();
    n->h = a.num("h", 1.0);
    // OpenSCAD precedence: diameter overrides radius; d1/d2 override r1/r2,
    // which in turn fall back to the base r/d.
    double r = a.has("d") ? a.get("d").asNumber() / 2.0 : a.num("r", 1.0);
    n->r1 = a.has("d1") ? a.get("d1").asNumber() / 2.0
                        : (a.has("r1") ? a.get("r1").asNumber() : r);
    n->r2 = a.has("d2") ? a.get("d2").asNumber() / 2.0
                        : (a.has("r2") ? a.get("r2").asNumber() : r);
    n->center = a.getOr("center", Value::makeBool(false)).asBool();
    n->fn = resolveFn(a, scope);
    return n;
  }

  if (m == "square") {
    Args a = bindArgs(call, {"size", "center"}, scope);
    auto n = std::make_shared<SquareNode>();
    auto xy = a.getOr("size", Value::makeNumber(1)).asVecN(2);
    n->x = xy[0]; n->y = xy[1];
    n->center = a.getOr("center", Value::makeBool(false)).asBool();
    return n;
  }

  if (m == "circle") {
    Args a = bindArgs(call, {"r"}, scope);
    auto n = std::make_shared<CircleNode>();
    if (a.has("d"))      n->r = a.get("d").asNumber() / 2.0;
    else if (a.has("r")) n->r = a.get("r").asNumber();
    else                 n->r = 1.0;
    n->fn = resolveFn(a, scope);
    return n;
  }

  if (m == "polygon") {
    Args a = bindArgs(call, {"points"}, scope);
    if (!a.has("points")) throw std::runtime_error("polygon(): missing points");
    Value pts = a.get("points");
    if (!pts.isVector()) throw std::runtime_error("polygon(): points must be a vector of [x,y]");
    auto n = std::make_shared<PolygonNode>();
    for (const auto& p : pts.vec) {
      auto xy = p.asVecN(2);
      n->points.emplace_back(xy[0], xy[1]);
    }
    return n;
  }
  return nullptr;
}

NodePtr buildTransform(const CallStmt& call, Scope& scope) {
  const std::string& m = call.name;

  if (m == "linear_extrude") {
    Args a = bindArgs(call, {"height"}, scope);
    // Tapered/twisted extrude not implemented yet -- fail loud rather than emit a
    // straight prism that silently disagrees with the model.
    if (a.has("twist") && a.get("twist").asNumber() != 0.0)
      throw std::runtime_error("linear_extrude: twist= not yet supported");
    if (a.has("scale")) {
      auto sc = a.get("scale").asVecN(2);
      if (sc[0] != 1.0 || sc[1] != 1.0)
        throw std::runtime_error("linear_extrude: scale= not yet supported");
    }
    auto n = std::make_shared<LinearExtrudeNode>();
    n->height = a.has("height") ? a.get("height").asNumber()
              : (a.has("h") ? a.get("h").asNumber() : 1.0);
    n->center = a.getOr("center", Value::makeBool(false)).asBool();
    n->children = execChildren(call, scope);
    return n;
  }

  if (m == "rotate_extrude") {
    Args a = bindArgs(call, {"angle"}, scope);
    if (a.has("start"))
      throw std::runtime_error("rotate_extrude: start= not yet supported");
    auto n = std::make_shared<RotateExtrudeNode>();
    n->angle = a.num("angle", 360.0);
    n->children = execChildren(call, scope);
    return n;
  }

  if (m == "translate") {
    Args a = bindArgs(call, {"v"}, scope);
    auto n = std::make_shared<TranslateNode>();
    auto v = a.getOr("v", Value::makeVector({})).asVecN(3);
    n->x = v[0]; n->y = v[1]; n->z = v[2];
    n->children = execChildren(call, scope);
    return n;
  }

  if (m == "scale") {
    Args a = bindArgs(call, {"v"}, scope);
    auto n = std::make_shared<ScaleNode>();
    Value v = a.getOr("v", Value::makeNumber(1));
    auto xyz = v.asVecN(3);
    // A scalar scale broadcasts; vector with missing comps already defaulted to 0,
    // so treat 0 as "1" when the source vector was shorter is not needed because
    // asVecN fills 0 -- guard against a 0 scale turning axes flat unintentionally.
    n->x = xyz[0] != 0 ? xyz[0] : 1;
    n->y = xyz[1] != 0 ? xyz[1] : 1;
    n->z = xyz[2] != 0 ? xyz[2] : 1;
    n->children = execChildren(call, scope);
    return n;
  }

  if (m == "mirror") {
    Args a = bindArgs(call, {"v"}, scope);
    auto n = std::make_shared<MirrorNode>();
    auto v = a.getOr("v", Value::makeVector({})).asVecN(3);
    n->x = v[0]; n->y = v[1]; n->z = v[2];
    n->children = execChildren(call, scope);
    return n;
  }

  if (m == "rotate") {
    Args a = bindArgs(call, {"a", "v"}, scope);
    auto n = std::make_shared<RotateNode>();
    Value av = a.getOr("a", Value::makeNumber(0));
    if (a.has("v") || av.isNumber()) {
      if (a.has("v")) {
        // axis-angle: rotate(a, [x,y,z])
        n->axisAngle = true;
        n->angle = av.asNumber();
        auto axis = a.get("v").asVecN(3);
        n->ax = axis[0]; n->ay = axis[1]; n->az = axis[2];
      } else {
        // scalar a => rotate about Z
        n->axisAngle = false;
        n->x = 0; n->y = 0; n->z = av.asNumber();
      }
    }
    if (av.isVector()) {  // rotate([x,y,z]) Euler
      n->axisAngle = false;
      auto xyz = av.asVecN(3);
      n->x = xyz[0]; n->y = xyz[1]; n->z = xyz[2];
    }
    n->children = execChildren(call, scope);
    return n;
  }

  // fillet(r, edges="all"|"convex"|"concave") / round(...): round selected edges
  // of the children by radius r. The `edges` selector is a declarative query over
  // topology (evaluated against the built solid), not a stateful pick.
  // (No OpenSCAD equivalent -- this is the B-Rep feature layer.)
  if (m == "fillet" || m == "round") {
    Args a = bindArgs(call, {"r", "edges"}, scope);
    auto n = std::make_shared<FilletNode>();
    n->r = a.num("r", 1.0);
    if (a.has("edges")) {
      std::string sel = a.get("edges").asString();
      if (sel == "all") n->sel = FilletNode::Sel::All;
      else if (sel == "convex") n->sel = FilletNode::Sel::Convex;
      else if (sel == "concave") n->sel = FilletNode::Sel::Concave;
      else throw std::runtime_error("fillet(): unknown edges selector \"" + sel +
                                    "\" (expected \"all\", \"convex\" or \"concave\")");
    }
    n->children = execChildren(call, scope);
    return n;
  }

  // attach(on="top" [, from=...]) { parent; children... }: seat the children onto the
  // parent's (first child's) queried face by coinciding datum frames. Faces are named
  // by outward-normal direction word and re-resolved against the freshly-built solids
  // -- a pure topology query, the constructive dual of fillet's edge selection.
  // (No OpenSCAD equivalent -- this is the frames/datums layer.)
  if (m == "attach") {
    Args a = bindArgs(call, {"on", "from"}, scope);
    auto n = std::make_shared<AttachNode>();
    if (a.has("on"))   n->on = a.get("on").asString();
    if (a.has("from")) n->from = a.get("from").asString();
    n->children = execChildren(call, scope);
    return n;
  }
  return nullptr;
}

NodePtr buildBoolean(const CallStmt& call, Scope& scope) {
  const std::string& m = call.name;
  NodeKind k;
  if (m == "union") k = NodeKind::Union;
  else if (m == "difference") k = NodeKind::Difference;
  else if (m == "intersection") k = NodeKind::Intersection;
  else if (m == "group") k = NodeKind::Group;
  else return nullptr;
  auto n = std::make_shared<BooleanNode>(k);
  n->children = execChildren(call, scope);
  return n;
}

void execStmt(const Stmt* s, Scope& scope, std::vector<NodePtr>& out) {
  if (s->kind == Stmt::Kind::Assign) {
    auto* as = static_cast<const AssignStmt*>(s);
    scope[as->name] = evalExpr(as->value.get(), scope);
    return;
  }
  const auto& call = *static_cast<const CallStmt*>(s);
  NodePtr n = buildPrimitive(call, scope);
  if (!n) n = buildTransform(call, scope);
  if (!n) n = buildBoolean(call, scope);
  if (!n)
    throw std::runtime_error("unknown module '" + call.name + "' (line " +
                             std::to_string(call.line) + ")");
  out.push_back(std::move(n));
}

}  // namespace

NodePtr evaluate(const std::vector<StmtPtr>& program) {
  Scope scope;
  auto root = std::make_shared<BooleanNode>(NodeKind::Group);
  for (const auto& s : program) execStmt(s.get(), scope, root->children);
  return root;
}
