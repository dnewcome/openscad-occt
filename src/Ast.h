#pragma once
// AST produced by the parser. Pure data; the Evaluator walks it.
#include <memory>
#include <string>
#include <vector>

// ---- Expressions ----
struct Expr {
  enum class Kind { Number, Bool, String, Vector, Ident, Unary, Binary };
  Kind kind;
  explicit Expr(Kind k) : kind(k) {}
  virtual ~Expr() = default;
};
using ExprPtr = std::unique_ptr<Expr>;

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
  char op;  // '+','-','*','/'
  ExprPtr lhs, rhs;
  BinaryExpr(char o, ExprPtr l, ExprPtr r)
      : Expr(Kind::Binary), op(o), lhs(std::move(l)), rhs(std::move(r)) {}
};

// ---- Statements ----
struct Arg {
  std::string name;  // empty => positional
  ExprPtr value;
};

struct Stmt {
  enum class Kind { Assign, Call };
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
  std::string name;            // module name: cube, translate, union, ...
  std::vector<Arg> args;
  std::vector<StmtPtr> children;
  int line = 0;
  CallStmt() : Stmt(Kind::Call) {}
};
