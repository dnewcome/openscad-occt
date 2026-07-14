#pragma once
// AST produced by the parser. Pure data; the Evaluator walks it.
#include <memory>
#include <string>
#include <vector>

// ---- Expressions ----
struct Expr {
  enum class Kind { Number, Bool, String, Vector, Range, Ident, Unary, Binary, Ternary, Index, Call };
  Kind kind;
  explicit Expr(Kind k) : kind(k) {}
  virtual ~Expr() = default;
};
using ExprPtr = std::unique_ptr<Expr>;

// A call argument (module or function): named (name set) or positional (name empty).
struct Arg {
  std::string name;
  ExprPtr value;
};

enum class BinOp { Add, Sub, Mul, Div, Mod, Lt, Gt, Le, Ge, Eq, Ne, And, Or };

struct NumberExpr : Expr {
  double value;
  explicit NumberExpr(double v) : Expr(Kind::Number), value(v) {}
};
struct BoolExpr : Expr {
  bool value;
  explicit BoolExpr(bool v) : Expr(Kind::Bool), value(v) {}
};
struct StringExpr : Expr {
  std::string value;
  explicit StringExpr(std::string v) : Expr(Kind::String), value(std::move(v)) {}
};
struct VectorExpr : Expr {
  std::vector<ExprPtr> elems;
  VectorExpr() : Expr(Kind::Vector) {}
};
struct RangeExpr : Expr {  // [start:end] or [start:step:end]
  ExprPtr start, step, end;  // step may be null
  RangeExpr() : Expr(Kind::Range) {}
};
struct IdentExpr : Expr {
  std::string name;
  explicit IdentExpr(std::string n) : Expr(Kind::Ident), name(std::move(n)) {}
};
struct UnaryExpr : Expr {
  char op;  // '-' or '!'
  ExprPtr operand;
  UnaryExpr(char o, ExprPtr e) : Expr(Kind::Unary), op(o), operand(std::move(e)) {}
};
struct BinaryExpr : Expr {
  BinOp op;
  ExprPtr lhs, rhs;
  BinaryExpr(BinOp o, ExprPtr l, ExprPtr r)
      : Expr(Kind::Binary), op(o), lhs(std::move(l)), rhs(std::move(r)) {}
};
struct TernaryExpr : Expr {  // cond ? a : b
  ExprPtr cond, a, b;
  TernaryExpr() : Expr(Kind::Ternary) {}
};
struct IndexExpr : Expr {  // base[index]
  ExprPtr base, index;
  IndexExpr() : Expr(Kind::Index) {}
};
struct CallExpr : Expr {  // function call in expression position: name(args)
  std::string name;
  std::vector<Arg> args;
  CallExpr() : Expr(Kind::Call) {}
};

// ---- Statements ----
// A module/function parameter: name with an optional default expression.
struct Param {
  std::string name;
  ExprPtr def;  // may be null
};

struct Stmt {
  enum class Kind { Assign, Call, ModuleDef, FunctionDef, For, If };
  Kind kind;
  explicit Stmt(Kind k) : kind(k) {}
  virtual ~Stmt() = default;
};
using StmtPtr = std::unique_ptr<Stmt>;

struct AssignStmt : Stmt {
  std::string name;
  ExprPtr value;
  AssignStmt() : Stmt(Kind::Assign) {}
};

struct CallStmt : Stmt {
  std::string name;            // module name: cube, translate, union, user modules, ...
  std::vector<Arg> args;
  std::vector<StmtPtr> children;
  int line = 0;
  CallStmt() : Stmt(Kind::Call) {}
};

struct ModuleDefStmt : Stmt {
  std::string name;
  std::vector<Param> params;
  std::vector<StmtPtr> body;
  ModuleDefStmt() : Stmt(Kind::ModuleDef) {}
};

struct FunctionDefStmt : Stmt {
  std::string name;
  std::vector<Param> params;
  ExprPtr body;  // the expression after '='
  FunctionDefStmt() : Stmt(Kind::FunctionDef) {}
};

struct ForStmt : Stmt {
  // for (v1 = e1, v2 = e2, ...) body  -> nested loops over each binding
  std::vector<std::pair<std::string, ExprPtr>> vars;
  std::vector<StmtPtr> body;
  ForStmt() : Stmt(Kind::For) {}
};

struct IfStmt : Stmt {
  ExprPtr cond;
  std::vector<StmtPtr> thenBody, elseBody;
  IfStmt() : Stmt(Kind::If) {}
};
