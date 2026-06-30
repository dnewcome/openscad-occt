#include "Parser.h"

#include <stdexcept>

namespace {

class Parser {
 public:
  explicit Parser(const std::vector<Token>& toks) : t(toks) {}

  std::vector<StmtPtr> parseProgram() {
    std::vector<StmtPtr> prog;
    while (!at(Tok::End)) prog.push_back(parseStatement());
    return prog;
  }

 private:
  const std::vector<Token>& t;
  size_t pos = 0;

  const Token& cur() const { return t[pos]; }
  bool at(Tok k) const { return t[pos].kind == k; }
  const Token& advance() { return t[pos++]; }

  [[noreturn]] void err(const std::string& msg) const {
    throw std::runtime_error("parse error (line " + std::to_string(cur().line) + "): " + msg);
  }

  const Token& expect(Tok k, const char* what) {
    if (!at(k)) err(std::string("expected ") + what);
    return advance();
  }

  // ---- statements ----
  StmtPtr parseStatement() {
    if (!at(Tok::Ident)) err("expected a statement (identifier)");
    // Lookahead: IDENT '=' is an assignment, IDENT '(' is a module call.
    if (t[pos + 1].kind == Tok::Equal) return parseAssignment();
    return parseCall();
  }

  StmtPtr parseAssignment() {
    auto s = std::make_unique<AssignStmt>();
    s->name = advance().text;           // IDENT
    expect(Tok::Equal, "'='");
    s->value = parseExpr();
    expect(Tok::Semicolon, "';'");
    return s;
  }

  StmtPtr parseCall() {
    auto s = std::make_unique<CallStmt>();
    s->line = cur().line;
    s->name = advance().text;           // IDENT
    expect(Tok::LParen, "'('");
    if (!at(Tok::RParen)) {
      for (;;) {
        s->args.push_back(parseArg());
        if (at(Tok::Comma)) { advance(); continue; }
        break;
      }
    }
    expect(Tok::RParen, "')'");
    parseChild(s->children);
    return s;
  }

  Arg parseArg() {
    Arg a;
    // named arg: IDENT '=' expr
    if (at(Tok::Ident) && t[pos + 1].kind == Tok::Equal) {
      a.name = advance().text;
      advance();  // '='
    }
    a.value = parseExpr();
    return a;
  }

  // A module call's child(ren): ';' (none), a '{...}' block, or a single nested call.
  void parseChild(std::vector<StmtPtr>& out) {
    if (at(Tok::Semicolon)) { advance(); return; }
    if (at(Tok::LBrace)) {
      advance();
      while (!at(Tok::RBrace)) {
        if (at(Tok::End)) err("unterminated '{' block");
        out.push_back(parseStatement());
      }
      advance();  // '}'
      return;
    }
    // single child statement (e.g. translate(...) cube(...);)
    out.push_back(parseStatement());
  }

  // ---- expressions ----
  ExprPtr parseExpr() { return parseAddSub(); }

  ExprPtr parseAddSub() {
    ExprPtr lhs = parseMulDiv();
    while (at(Tok::Plus) || at(Tok::Minus)) {
      char op = at(Tok::Plus) ? '+' : '-';
      advance();
      lhs = std::make_unique<BinaryExpr>(op, std::move(lhs), parseMulDiv());
    }
    return lhs;
  }

  ExprPtr parseMulDiv() {
    ExprPtr lhs = parseUnary();
    while (at(Tok::Star) || at(Tok::Slash)) {
      char op = at(Tok::Star) ? '*' : '/';
      advance();
      lhs = std::make_unique<BinaryExpr>(op, std::move(lhs), parseUnary());
    }
    return lhs;
  }

  ExprPtr parseUnary() {
    if (at(Tok::Minus)) { advance(); return std::make_unique<UnaryExpr>('-', parseUnary()); }
    if (at(Tok::Bang))  { advance(); return std::make_unique<UnaryExpr>('!', parseUnary()); }
    return parsePrimary();
  }

  ExprPtr parsePrimary() {
    if (at(Tok::Number)) return std::make_unique<NumberExpr>(advance().num);
    if (at(Tok::Ident)) {
      const std::string& name = cur().text;
      if (name == "true")  { advance(); return std::make_unique<BoolExpr>(true); }
      if (name == "false") { advance(); return std::make_unique<BoolExpr>(false); }
      if (name == "undef") { advance(); return std::make_unique<IdentExpr>("undef"); }
      return std::make_unique<IdentExpr>(advance().text);
    }
    if (at(Tok::LParen)) {
      advance();
      ExprPtr e = parseExpr();
      expect(Tok::RParen, "')'");
      return e;
    }
    if (at(Tok::LBracket)) {
      advance();
      auto v = std::make_unique<VectorExpr>();
      if (!at(Tok::RBracket)) {
        for (;;) {
          v->elems.push_back(parseExpr());
          if (at(Tok::Comma)) { advance(); continue; }
          break;
        }
      }
      expect(Tok::RBracket, "']'");
      return v;
    }
    err("expected an expression");
  }
};

}  // namespace

std::vector<StmtPtr> parse(const std::vector<Token>& tokens) {
  Parser p(tokens);
  return p.parseProgram();
}
