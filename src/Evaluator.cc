#include "Evaluator.h"

#include <cmath>
#include <map>
#include <stdexcept>
#include <string>

#include "Value.h"

namespace {

using Scope = std::map<std::string, Value>;

// Global registry of user definitions. Collected once up front (a full-tree walk),
// so all user modules/functions are visible everywhere (a documented simplification:
// OpenSCAD block-scopes definitions; real code defines them at top level).
struct Defs {
  std::map<std::string, const ModuleDefStmt*> modules;
  std::map<std::string, const FunctionDefStmt*> functions;
};

// Evaluation environment threaded through statements and expressions.
struct Env {
  Scope vars;
  const Defs* defs = nullptr;
  const Scope* globals = nullptr;                    // top-level scope (module bodies see this)
  const std::vector<StmtPtr>* childNodes = nullptr;  // children passed to current module call
  const Env* childScope = nullptr;                   // env in which to instantiate them
};

Value evalExpr(const Expr* e, const Env& env);
void execStmt(const Stmt* s, Env& env, std::vector<NodePtr>& out);
void execBlock(const std::vector<StmtPtr>& stmts, Env& env, std::vector<NodePtr>& out);

// RAII guard against runaway module/function recursion. Converts a would-be native
// stack overflow (SIGSEGV) into a clean, catchable error -- the fail-loud principle.
struct DepthGuard {
  static int depth;
  DepthGuard() { if (++depth > 400) { --depth; throw std::runtime_error("recursion too deep (limit 400)"); } }
  ~DepthGuard() { --depth; }
};
int DepthGuard::depth = 0;

// ---- definition collection (hoisting) ----
void collectDefs(const std::vector<StmtPtr>& stmts, Defs& defs) {
  for (const auto& sp : stmts) {
    const Stmt* s = sp.get();
    switch (s->kind) {
      case Stmt::Kind::ModuleDef: {
        auto* m = static_cast<const ModuleDefStmt*>(s);
        defs.modules[m->name] = m;
        collectDefs(m->body, defs);
        break;
      }
      case Stmt::Kind::FunctionDef: {
        auto* f = static_cast<const FunctionDefStmt*>(s);
        defs.functions[f->name] = f;
        break;
      }
      case Stmt::Kind::Call:
        collectDefs(static_cast<const CallStmt*>(s)->children, defs);
        break;
      case Stmt::Kind::For:
        collectDefs(static_cast<const ForStmt*>(s)->body, defs);
        break;
      case Stmt::Kind::If: {
        auto* i = static_cast<const IfStmt*>(s);
        collectDefs(i->thenBody, defs);
        collectDefs(i->elseBody, defs);
        break;
      }
      default: break;
    }
  }
}

// ---- built-in functions ----
constexpr double kDeg = M_PI / 180.0;

Value callBuiltin(const std::string& name, const std::vector<Value>& a) {
  auto num = [](double d) { return Value::makeNumber(d); };
  // Bounds-checked access: a missing argument is a clean error, never a segfault.
  auto n = [&](size_t i) { return a.at(i).asNumber(); };
  auto arg = [&](size_t i) -> const Value& { return a.at(i); };

  if (name == "sin")  return num(std::sin(n(0) * kDeg));
  if (name == "cos")  return num(std::cos(n(0) * kDeg));
  if (name == "tan")  return num(std::tan(n(0) * kDeg));
  if (name == "asin") return num(std::asin(n(0)) / kDeg);
  if (name == "acos") return num(std::acos(n(0)) / kDeg);
  if (name == "atan") return num(std::atan(n(0)) / kDeg);
  if (name == "atan2") return num(std::atan2(n(0), n(1)) / kDeg);
  if (name == "sqrt") return num(std::sqrt(n(0)));
  if (name == "abs")  return num(std::fabs(n(0)));
  if (name == "floor") return num(std::floor(n(0)));
  if (name == "ceil")  return num(std::ceil(n(0)));
  if (name == "round") return num(std::round(n(0)));
  if (name == "exp")  return num(std::exp(n(0)));
  if (name == "ln")   return num(std::log(n(0)));
  if (name == "log")  return num(std::log10(n(0)));
  if (name == "pow")  return num(std::pow(n(0), n(1)));
  if (name == "sign") return num((n(0) > 0) - (n(0) < 0));
  if (name == "len") {
    if (arg(0).isString()) return num((double)arg(0).str.size());
    if (arg(0).isVector()) return num((double)arg(0).vec.size());
    throw std::runtime_error("len(): expected a vector or string");
  }
  if (name == "norm") {
    double s = 0;
    for (const auto& x : arg(0).vec) s += x.asNumber() * x.asNumber();
    return num(std::sqrt(s));
  }
  if (name == "cross") {
    auto u = arg(0).asVecN(3), v = arg(1).asVecN(3);
    return Value::makeVector({num(u[1] * v[2] - u[2] * v[1]),
                              num(u[2] * v[0] - u[0] * v[2]),
                              num(u[0] * v[1] - u[1] * v[0])});
  }
  if (name == "min" || name == "max") {
    if (a.empty()) throw std::runtime_error(name + "(): needs an argument");
    bool mx = (name == "max");
    auto fold = [&](double acc, double x) { return mx ? std::max(acc, x) : std::min(acc, x); };
    if (a.size() == 1 && arg(0).isVector()) {
      const auto& v = arg(0).vec;
      if (v.empty()) throw std::runtime_error(name + "(): empty vector");
      double acc = v.at(0).asNumber();
      for (size_t i = 1; i < v.size(); ++i) acc = fold(acc, v[i].asNumber());
      return num(acc);
    }
    double acc = a.at(0).asNumber();
    for (size_t i = 1; i < a.size(); ++i) acc = fold(acc, a[i].asNumber());
    return num(acc);
  }
  throw std::runtime_error("unknown function: " + name);
}

// ---- expression evaluation ----
Value evalIdent(const IdentExpr* id, const Env& env) {
  if (id->name == "undef") return Value{};
  if (id->name == "PI") return Value::makeNumber(M_PI);
  auto it = env.vars.find(id->name);
  if (it != env.vars.end()) return it->second;
  throw std::runtime_error("undefined variable: " + id->name);
}

Value evalUnary(const UnaryExpr* u, const Env& env) {
  Value v = evalExpr(u->operand.get(), env);
  if (u->op == '!') return Value::makeBool(!v.asBool());
  if (v.isVector()) {
    std::vector<Value> r;
    for (auto& x : v.vec) r.push_back(Value::makeNumber(-x.asNumber()));
    return Value::makeVector(std::move(r));
  }
  return Value::makeNumber(-v.asNumber());
}

Value evalBinary(const BinaryExpr* b, const Env& env) {
  // Short-circuiting logical operators.
  if (b->op == BinOp::And) {
    if (!evalExpr(b->lhs.get(), env).asBool()) return Value::makeBool(false);
    return Value::makeBool(evalExpr(b->rhs.get(), env).asBool());
  }
  if (b->op == BinOp::Or) {
    if (evalExpr(b->lhs.get(), env).asBool()) return Value::makeBool(true);
    return Value::makeBool(evalExpr(b->rhs.get(), env).asBool());
  }

  Value l = evalExpr(b->lhs.get(), env);
  Value r = evalExpr(b->rhs.get(), env);

  if (b->op == BinOp::Eq) return Value::makeBool(l.equals(r));
  if (b->op == BinOp::Ne) return Value::makeBool(!l.equals(r));

  // vector (+|-) vector, componentwise
  if (l.isVector() && r.isVector() && (b->op == BinOp::Add || b->op == BinOp::Sub)) {
    if (l.vec.size() != r.vec.size())
      throw std::runtime_error("vector length mismatch in +/-");
    std::vector<Value> out;
    for (size_t i = 0; i < l.vec.size(); ++i) {
      double a = l.vec[i].asNumber(), c = r.vec[i].asNumber();
      out.push_back(Value::makeNumber(b->op == BinOp::Add ? a + c : a - c));
    }
    return Value::makeVector(std::move(out));
  }
  // vector */ scalar (and scalar * vector)
  auto scaleVec = [](const Value& vv, double s, BinOp op) {
    std::vector<Value> out;
    for (auto& x : vv.vec)
      out.push_back(Value::makeNumber(op == BinOp::Mul ? x.asNumber() * s : x.asNumber() / s));
    return Value::makeVector(std::move(out));
  };
  if (l.isVector() && r.isNumber() && (b->op == BinOp::Mul || b->op == BinOp::Div))
    return scaleVec(l, r.num, b->op);
  if (l.isNumber() && r.isVector() && b->op == BinOp::Mul)
    return scaleVec(r, l.num, BinOp::Mul);

  double a = l.asNumber(), c = r.asNumber();
  switch (b->op) {
    case BinOp::Add: return Value::makeNumber(a + c);
    case BinOp::Sub: return Value::makeNumber(a - c);
    case BinOp::Mul: return Value::makeNumber(a * c);
    case BinOp::Div: return Value::makeNumber(a / c);
    case BinOp::Mod: return Value::makeNumber(std::fmod(a, c));
    case BinOp::Lt:  return Value::makeBool(a < c);
    case BinOp::Gt:  return Value::makeBool(a > c);
    case BinOp::Le:  return Value::makeBool(a <= c);
    case BinOp::Ge:  return Value::makeBool(a >= c);
    default: break;
  }
  throw std::runtime_error("bad operator");
}

Value evalCall(const CallExpr* c, const Env& env) {
  // User functions shadow built-ins.
  if (env.defs) {
    auto it = env.defs->functions.find(c->name);
    if (it != env.defs->functions.end()) {
    DepthGuard guard;
    const FunctionDefStmt* def = it->second;
    Env fe;
    fe.defs = env.defs;
    fe.globals = env.globals;
    if (env.globals) fe.vars = *env.globals;
    for (const auto& kv : env.vars)
      if (!kv.first.empty() && kv.first[0] == '$') fe.vars[kv.first] = kv.second;
    // bind params: positional then named, defaults last
    std::map<std::string, Value> named;
    size_t p = 0;
    for (const auto& a : c->args) {
      Value v = evalExpr(a.value.get(), env);
      if (a.name.empty()) { if (p < def->params.size()) fe.vars[def->params[p].name] = v; ++p; }
      else named[a.name] = v;
    }
    for (const auto& kv : named) fe.vars[kv.first] = kv.second;
    for (const auto& par : def->params)
      if (fe.vars.find(par.name) == fe.vars.end())
        fe.vars[par.name] = par.def ? evalExpr(par.def.get(), fe) : Value{};
    return evalExpr(def->body.get(), fe);
    }
  }
  // Built-in: positional args only.
  std::vector<Value> vals;
  for (const auto& a : c->args) vals.push_back(evalExpr(a.value.get(), env));
  return callBuiltin(c->name, vals);
}

Value evalExpr(const Expr* e, const Env& env) {
  switch (e->kind) {
    case Expr::Kind::Number: return Value::makeNumber(static_cast<const NumberExpr*>(e)->value);
    case Expr::Kind::Bool:   return Value::makeBool(static_cast<const BoolExpr*>(e)->value);
    case Expr::Kind::String: return Value::makeString(static_cast<const StringExpr*>(e)->value);
    case Expr::Kind::Ident:  return evalIdent(static_cast<const IdentExpr*>(e), env);
    case Expr::Kind::Unary:  return evalUnary(static_cast<const UnaryExpr*>(e), env);
    case Expr::Kind::Binary: return evalBinary(static_cast<const BinaryExpr*>(e), env);
    case Expr::Kind::Call:   return evalCall(static_cast<const CallExpr*>(e), env);
    case Expr::Kind::Vector: {
      auto* v = static_cast<const VectorExpr*>(e);
      std::vector<Value> out;
      for (auto& el : v->elems) out.push_back(evalExpr(el.get(), env));
      return Value::makeVector(std::move(out));
    }
    case Expr::Kind::Range: {
      auto* r = static_cast<const RangeExpr*>(e);
      double s = evalExpr(r->start.get(), env).asNumber();
      double en = evalExpr(r->end.get(), env).asNumber();
      double st = r->step ? evalExpr(r->step.get(), env).asNumber() : 1.0;
      return Value::makeRange(s, st, en);
    }
    case Expr::Kind::Ternary: {
      auto* tr = static_cast<const TernaryExpr*>(e);
      return evalExpr(tr->cond.get(), env).asBool() ? evalExpr(tr->a.get(), env)
                                                     : evalExpr(tr->b.get(), env);
    }
    case Expr::Kind::Index: {
      auto* ix = static_cast<const IndexExpr*>(e);
      Value base = evalExpr(ix->base.get(), env);
      Value idx = evalExpr(ix->index.get(), env);
      if (base.isVector()) {
        if (!idx.isNumber()) return Value{};
        long k = (long)idx.num;
        if (k < 0 || k >= (long)base.vec.size()) return Value{};
        return base.vec[(size_t)k];
      }
      if (base.isString()) {
        long k = (long)idx.asNumber();
        if (k < 0 || k >= (long)base.str.size()) return Value{};
        return Value::makeString(std::string(1, base.str[(size_t)k]));
      }
      return Value{};
    }
  }
  throw std::runtime_error("bad expression");
}

// ---- resolved arguments (module builtins) ----
struct Args {
  std::map<std::string, Value> by_name;
  bool has(const std::string& k) const { return by_name.count(k) != 0; }
  Value get(const std::string& k) const { return by_name.at(k); }
  Value getOr(const std::string& k, Value def) const {
    auto it = by_name.find(k);
    return it == by_name.end() ? def : it->second;
  }
  double num(const std::string& k, double def) const { return has(k) ? get(k).asNumber() : def; }
};

Args bindArgs(const CallStmt& call, const std::vector<std::string>& params, const Env& env) {
  Args a;
  size_t pos = 0;
  for (const auto& arg : call.args) {
    if (!arg.name.empty()) {
      a.by_name[arg.name] = evalExpr(arg.value.get(), env);
    } else {
      if (pos >= params.size())
        throw std::runtime_error(call.name + "(): too many positional arguments");
      a.by_name[params[pos++]] = evalExpr(arg.value.get(), env);
    }
  }
  return a;
}

int resolveFn(const Args& a, const Env& env) {
  if (a.has("$fn") && a.get("$fn").isNumber()) return (int)a.get("$fn").asNumber();
  auto it = env.vars.find("$fn");
  if (it != env.vars.end() && it->second.isNumber()) return (int)it->second.asNumber();
  return 0;
}

// Children of a builtin transform/boolean: a fresh copy-down scope (they see the
// enclosing locals), with any $-named args on this call injected (dynamic scoping),
// and the children()/module context inherited so `translate() children()` works.
std::vector<NodePtr> execChildren(const CallStmt& call, const Env& env) {
  Env child = env;
  for (const auto& a : call.args)
    if (!a.name.empty() && a.name[0] == '$')
      child.vars[a.name] = evalExpr(a.value.get(), env);
  std::vector<NodePtr> kids;
  for (const auto& c : call.children) execStmt(c.get(), child, kids);
  return kids;
}

NodePtr buildPrimitive(const CallStmt& call, const Env& env) {
  const std::string& m = call.name;

  if (m == "cube") {
    Args a = bindArgs(call, {"size", "center"}, env);
    auto n = std::make_shared<CubeNode>();
    auto xyz = a.getOr("size", Value::makeNumber(1)).asVecN(3);
    n->x = xyz[0]; n->y = xyz[1]; n->z = xyz[2];
    n->center = a.getOr("center", Value::makeBool(false)).asBool();
    return n;
  }
  if (m == "sphere") {
    Args a = bindArgs(call, {"r"}, env);
    auto n = std::make_shared<SphereNode>();
    if (a.has("d"))      n->r = a.get("d").asNumber() / 2.0;
    else if (a.has("r")) n->r = a.get("r").asNumber();
    else                 n->r = 1.0;
    n->fn = resolveFn(a, env);
    return n;
  }
  if (m == "cylinder") {
    Args a = bindArgs(call, {"h", "r1", "r2"}, env);
    auto n = std::make_shared<CylinderNode>();
    n->h = a.num("h", 1.0);
    double r = a.has("d") ? a.get("d").asNumber() / 2.0 : a.num("r", 1.0);
    n->r1 = a.has("d1") ? a.get("d1").asNumber() / 2.0
                        : (a.has("r1") ? a.get("r1").asNumber() : r);
    n->r2 = a.has("d2") ? a.get("d2").asNumber() / 2.0
                        : (a.has("r2") ? a.get("r2").asNumber() : r);
    n->center = a.getOr("center", Value::makeBool(false)).asBool();
    n->fn = resolveFn(a, env);
    return n;
  }
  if (m == "square") {
    Args a = bindArgs(call, {"size", "center"}, env);
    auto n = std::make_shared<SquareNode>();
    auto xy = a.getOr("size", Value::makeNumber(1)).asVecN(2);
    n->x = xy[0]; n->y = xy[1];
    n->center = a.getOr("center", Value::makeBool(false)).asBool();
    return n;
  }
  if (m == "circle") {
    Args a = bindArgs(call, {"r"}, env);
    auto n = std::make_shared<CircleNode>();
    if (a.has("d"))      n->r = a.get("d").asNumber() / 2.0;
    else if (a.has("r")) n->r = a.get("r").asNumber();
    else                 n->r = 1.0;
    n->fn = resolveFn(a, env);
    return n;
  }
  if (m == "polygon") {
    Args a = bindArgs(call, {"points"}, env);
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

NodePtr buildTransform(const CallStmt& call, const Env& env) {
  const std::string& m = call.name;

  if (m == "linear_extrude") {
    Args a = bindArgs(call, {"height"}, env);
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
    n->children = execChildren(call, env);
    return n;
  }
  if (m == "rotate_extrude") {
    Args a = bindArgs(call, {"angle"}, env);
    if (a.has("start")) throw std::runtime_error("rotate_extrude: start= not yet supported");
    auto n = std::make_shared<RotateExtrudeNode>();
    n->angle = a.num("angle", 360.0);
    n->children = execChildren(call, env);
    return n;
  }
  if (m == "translate") {
    Args a = bindArgs(call, {"v"}, env);
    auto n = std::make_shared<TranslateNode>();
    auto v = a.getOr("v", Value::makeVector({})).asVecN(3);
    n->x = v[0]; n->y = v[1]; n->z = v[2];
    n->children = execChildren(call, env);
    return n;
  }
  if (m == "scale") {
    Args a = bindArgs(call, {"v"}, env);
    auto n = std::make_shared<ScaleNode>();
    auto xyz = a.getOr("v", Value::makeNumber(1)).asVecN(3);
    n->x = xyz[0] != 0 ? xyz[0] : 1;
    n->y = xyz[1] != 0 ? xyz[1] : 1;
    n->z = xyz[2] != 0 ? xyz[2] : 1;
    n->children = execChildren(call, env);
    return n;
  }
  if (m == "mirror") {
    Args a = bindArgs(call, {"v"}, env);
    auto n = std::make_shared<MirrorNode>();
    auto v = a.getOr("v", Value::makeVector({})).asVecN(3);
    n->x = v[0]; n->y = v[1]; n->z = v[2];
    n->children = execChildren(call, env);
    return n;
  }
  if (m == "rotate") {
    Args a = bindArgs(call, {"a", "v"}, env);
    auto n = std::make_shared<RotateNode>();
    Value av = a.getOr("a", Value::makeNumber(0));
    if (a.has("v") || av.isNumber()) {
      if (a.has("v")) {
        n->axisAngle = true;
        n->angle = av.asNumber();
        auto axis = a.get("v").asVecN(3);
        n->ax = axis[0]; n->ay = axis[1]; n->az = axis[2];
      } else {
        n->axisAngle = false;
        n->x = 0; n->y = 0; n->z = av.asNumber();
      }
    }
    if (av.isVector()) {
      n->axisAngle = false;
      auto xyz = av.asVecN(3);
      n->x = xyz[0]; n->y = xyz[1]; n->z = xyz[2];
    }
    n->children = execChildren(call, env);
    return n;
  }
  if (m == "fillet" || m == "round") {
    Args a = bindArgs(call, {"r", "edges"}, env);
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
    n->children = execChildren(call, env);
    return n;
  }
  if (m == "attach") {
    Args a = bindArgs(call, {"on", "from"}, env);
    auto n = std::make_shared<AttachNode>();
    if (a.has("on"))   n->on = a.get("on").asString();
    if (a.has("from")) n->from = a.get("from").asString();
    n->children = execChildren(call, env);
    return n;
  }
  return nullptr;
}

NodePtr buildBoolean(const CallStmt& call, const Env& env) {
  const std::string& m = call.name;
  NodeKind k;
  if (m == "union") k = NodeKind::Union;
  else if (m == "difference") k = NodeKind::Difference;
  else if (m == "intersection") k = NodeKind::Intersection;
  else if (m == "group") k = NodeKind::Group;
  else return nullptr;
  auto n = std::make_shared<BooleanNode>(k);
  n->children = execChildren(call, env);
  return n;
}

// ---- user module instantiation ----
void callUserModule(const ModuleDefStmt* def, const CallStmt& call, const Env& env,
                    std::vector<NodePtr>& out) {
  DepthGuard guard;
  Env body;
  body.defs = env.defs;
  body.globals = env.globals;
  if (env.globals) body.vars = *env.globals;
  for (const auto& kv : env.vars)  // dynamic special vars from caller
    if (!kv.first.empty() && kv.first[0] == '$') body.vars[kv.first] = kv.second;

  // bind params: positional then named, then defaults
  std::map<std::string, Value> named;
  size_t p = 0;
  for (const auto& arg : call.args) {
    Value v = evalExpr(arg.value.get(), env);
    if (arg.name.empty()) { if (p < def->params.size()) body.vars[def->params[p].name] = v; ++p; }
    else named[arg.name] = v;  // includes $-specials
  }
  for (const auto& kv : named) body.vars[kv.first] = kv.second;
  for (const auto& par : def->params)
    if (body.vars.find(par.name) == body.vars.end())
      body.vars[par.name] = par.def ? evalExpr(par.def.get(), body) : Value{};

  body.childNodes = &call.children;
  body.childScope = &env;
  body.vars["$children"] = Value::makeNumber((double)call.children.size());

  execBlock(def->body, body, out);
}

// children() / children(i) inside a module body.
void execChildrenCall(const CallStmt& call, const Env& env, std::vector<NodePtr>& out) {
  if (!env.childNodes || !env.childScope) return;  // no children in scope -> nothing
  const auto& nodes = *env.childNodes;
  Env cs = *env.childScope;
  if (call.args.empty()) {
    for (const auto& c : nodes) execStmt(c.get(), cs, out);
    return;
  }
  Value idx = evalExpr(call.args[0].value.get(), env);
  if (idx.isNumber()) {
    long k = (long)idx.num;
    if (k >= 0 && k < (long)nodes.size()) execStmt(nodes[(size_t)k].get(), cs, out);
  } else if (idx.isVector()) {
    for (const auto& e : idx.vec) {
      long k = (long)e.asNumber();
      if (k >= 0 && k < (long)nodes.size()) execStmt(nodes[(size_t)k].get(), cs, out);
    }
  }
}

// ---- control flow ----
void execForRec(const ForStmt* f, size_t idx, const Env& env, std::vector<NodePtr>& out) {
  if (idx == f->vars.size()) {
    Env e = env;
    execBlock(f->body, e, out);
    return;
  }
  Value range = evalExpr(f->vars[idx].second.get(), env);
  const std::string& var = f->vars[idx].first;
  auto step = [&](const Value& item) {
    Env e = env;
    e.vars[var] = item;
    execForRec(f, idx + 1, e, out);
  };
  if (range.isRange()) {
    if (range.rstep == 0) return;
    long cnt = (long)std::floor((range.rend - range.rstart) / range.rstep + 1e-9);
    for (long k = 0; k <= cnt; ++k) step(Value::makeNumber(range.rstart + k * range.rstep));
  } else if (range.isVector()) {
    for (const auto& item : range.vec) step(item);
  } else {
    step(range);
  }
}

// ---- statement execution ----
void execStmt(const Stmt* s, Env& env, std::vector<NodePtr>& out) {
  switch (s->kind) {
    case Stmt::Kind::Assign: {
      auto* as = static_cast<const AssignStmt*>(s);
      env.vars[as->name] = evalExpr(as->value.get(), env);
      return;
    }
    case Stmt::Kind::ModuleDef:
    case Stmt::Kind::FunctionDef:
      return;  // already hoisted into Defs
    // for / if each yield a SINGLE implicit-union group -- so `difference() for(...)`
    // subtracts nothing (one operand = the union of the loop bodies), matching
    // OpenSCAD, rather than treating the loop's Nth body as a tool on the 1st.
    case Stmt::Kind::For: {
      std::vector<NodePtr> kids;
      execForRec(static_cast<const ForStmt*>(s), 0, env, kids);
      if (!kids.empty()) {
        auto g = std::make_shared<BooleanNode>(NodeKind::Group);
        g->children = std::move(kids);
        out.push_back(std::move(g));
      }
      return;
    }
    case Stmt::Kind::If: {
      auto* i = static_cast<const IfStmt*>(s);
      Env e = env;
      std::vector<NodePtr> kids;
      if (evalExpr(i->cond.get(), env).asBool()) execBlock(i->thenBody, e, kids);
      else execBlock(i->elseBody, e, kids);
      if (!kids.empty()) {
        auto g = std::make_shared<BooleanNode>(NodeKind::Group);
        g->children = std::move(kids);
        out.push_back(std::move(g));
      }
      return;
    }
    case Stmt::Kind::Call: break;
  }

  const auto& call = *static_cast<const CallStmt*>(s);
  if (call.name == "children") { execChildrenCall(call, env, out); return; }

  NodePtr n = buildPrimitive(call, env);
  if (!n) n = buildTransform(call, env);
  if (!n) n = buildBoolean(call, env);
  if (n) { out.push_back(std::move(n)); return; }

  if (env.defs) {
    auto it = env.defs->modules.find(call.name);
    if (it != env.defs->modules.end()) { callUserModule(it->second, call, env, out); return; }
  }
  throw std::runtime_error("unknown module '" + call.name + "' (line " +
                           std::to_string(call.line) + ")");
}

void execBlock(const std::vector<StmtPtr>& stmts, Env& env, std::vector<NodePtr>& out) {
  for (const auto& s : stmts) execStmt(s.get(), env, out);
}

}  // namespace

NodePtr evaluate(const std::vector<StmtPtr>& program) {
  Defs defs;
  collectDefs(program, defs);

  Env top;
  top.defs = &defs;
  top.globals = &top.vars;

  auto root = std::make_shared<BooleanNode>(NodeKind::Group);
  execBlock(program, top, root->children);
  return root;
}
