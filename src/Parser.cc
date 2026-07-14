#include "Parser.h"

#include <stdexcept>

namespace {

class Parser {
 public:
  explicit Parser(const std::vector<Token>& toks) : t(toks) {}

  std::vector<StmtPtr> parseProgram() {
    std::vector<StmtPtr> prog;
    while (!at(Tok::End)) {
      if (at(Tok::Semicolon)) { advance(); continue; }  // stray ';'
      prog.push_back(parseStatement());
    }
    return prog;
  }

 private:
  const std::vector<Token>& t;
  size_t pos = 0;

  const Token& cur() const { return t[pos]; }
  const Token& peek() const { return t[pos + 1]; }
  bool at(Tok k) const { return t[pos].kind == k; }
  bool atIdent(const char* s) const { return at(Tok::Ident) && cur().text == s; }
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
    if (atIdent("module"))   return parseModuleDef();
    if (atIdent("function")) return parseFunctionDef();
    if (atIdent("for"))      return parseFor();
    if (atIdent("if"))       return parseIf();
    if (!at(Tok::Ident)) err("expected a statement (identifier)");
    if (peek().kind == Tok::Equal) return parseAssignment();
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
    parseArgList(s->args);
    expect(Tok::RParen, "')'");
    parseChild(s->children);
    return s;
  }

  void parseArgList(std::vector<Arg>& out) {
    if (at(Tok::RParen)) return;
    for (;;) {
      out.push_back(parseArg());
      if (at(Tok::Comma)) { advance(); continue; }
      break;
    }
  }

  Arg parseArg() {
    Arg a;
    if (at(Tok::Ident) && peek().kind == Tok::Equal) {  // named: IDENT '=' expr
      a.name = advance().text;
      advance();  // '='
    }
    a.value = parseExpr();
    return a;
  }

  std::vector<Param> parseParams() {
    std::vector<Param> params;
    expect(Tok::LParen, "'('");
    if (!at(Tok::RParen)) {
      for (;;) {
        Param p;
        p.name = expect(Tok::Ident, "parameter name").text;
        if (at(Tok::Equal)) { advance(); p.def = parseExpr(); }
        params.push_back(std::move(p));
        if (at(Tok::Comma)) { advance(); continue; }
        break;
      }
    }
    expect(Tok::RParen, "')'");
    return params;
  }

  StmtPtr parseModuleDef() {
    advance();  // 'module'
    auto s = std::make_unique<ModuleDefStmt>();
    s->name = expect(Tok::Ident, "module name").text;
    s->params = parseParams();
    parseChild(s->body);
    return s;
  }

  StmtPtr parseFunctionDef() {
    advance();  // 'function'
    auto s = std::make_unique<FunctionDefStmt>();
    s->name = expect(Tok::Ident, "function name").text;
    s->params = parseParams();
    expect(Tok::Equal, "'='");
    s->body = parseExpr();
    expect(Tok::Semicolon, "';'");
    return s;
  }

  StmtPtr parseFor() {
    advance();  // 'for'
    auto s = std::make_unique<ForStmt>();
    expect(Tok::LParen, "'('");
    for (;;) {
      std::string v = expect(Tok::Ident, "loop variable").text;
      expect(Tok::Equal, "'='");
      s->vars.emplace_back(std::move(v), parseExpr());
      if (at(Tok::Comma)) { advance(); continue; }
      break;
    }
    expect(Tok::RParen, "')'");
    parseChild(s->body);
    return s;
  }

  StmtPtr parseIf() {
    advance();  // 'if'
    auto s = std::make_unique<IfStmt>();
    expect(Tok::LParen, "'('");
    s->cond = parseExpr();
    expect(Tok::RParen, "')'");
    parseChild(s->thenBody);
    if (atIdent("else")) {
      advance();
      parseChild(s->elseBody);
    }
    return s;
  }

  // A body/child: ';' (none), a '{...}' block, or a single nested statement.
  void parseChild(std::vector<StmtPtr>& out) {
    if (at(Tok::Semicolon)) { advance(); return; }
    if (at(Tok::LBrace)) {
      advance();
      while (!at(Tok::RBrace)) {
        if (at(Tok::End)) err("unterminated '{' block");
        if (at(Tok::Semicolon)) { advance(); continue; }
        out.push_back(parseStatement());
      }
      advance();  // '}'
      return;
    }
    out.push_back(parseStatement());
  }

  // ---- expressions (precedence: ternary < || < && < eq < rel < add < mul < unary) ----
  ExprPtr parseExpr() { return parseTernary(); }

  ExprPtr parseTernary() {
    ExprPtr c = parseOr();
    if (at(Tok::Question)) {
      advance();
      auto e = std::make_unique<TernaryExpr>();
      e->cond = std::move(c);
      e->a = parseExpr();
      expect(Tok::Colon, "':'");
      e->b = parseTernary();
      return e;
    }
    return c;
  }

  ExprPtr parseOr() {
    ExprPtr l = parseAnd();
    while (at(Tok::Or)) { advance(); l = std::make_unique<BinaryExpr>(BinOp::Or, std::move(l), parseAnd()); }
    return l;
  }
  ExprPtr parseAnd() {
    ExprPtr l = parseEquality();
    while (at(Tok::And)) { advance(); l = std::make_unique<BinaryExpr>(BinOp::And, std::move(l), parseEquality()); }
    return l;
  }
  ExprPtr parseEquality() {
    ExprPtr l = parseRel();
    while (at(Tok::EqEq) || at(Tok::NotEq)) {
      BinOp op = at(Tok::EqEq) ? BinOp::Eq : BinOp::Ne;
      advance();
      l = std::make_unique<BinaryExpr>(op, std::move(l), parseRel());
    }
    return l;
  }
  ExprPtr parseRel() {
    ExprPtr l = parseAddSub();
    while (at(Tok::Less) || at(Tok::Greater) || at(Tok::LessEq) || at(Tok::GreaterEq)) {
      BinOp op = at(Tok::Less) ? BinOp::Lt : at(Tok::Greater) ? BinOp::Gt
               : at(Tok::LessEq) ? BinOp::Le : BinOp::Ge;
      advance();
      l = std::make_unique<BinaryExpr>(op, std::move(l), parseAddSub());
    }
    return l;
  }
  ExprPtr parseAddSub() {
    ExprPtr l = parseMulDiv();
    while (at(Tok::Plus) || at(Tok::Minus)) {
      BinOp op = at(Tok::Plus) ? BinOp::Add : BinOp::Sub;
      advance();
      l = std::make_unique<BinaryExpr>(op, std::move(l), parseMulDiv());
    }
    return l;
  }
  ExprPtr parseMulDiv() {
    ExprPtr l = parseUnary();
    while (at(Tok::Star) || at(Tok::Slash) || at(Tok::Percent)) {
      BinOp op = at(Tok::Star) ? BinOp::Mul : at(Tok::Slash) ? BinOp::Div : BinOp::Mod;
      advance();
      l = std::make_unique<BinaryExpr>(op, std::move(l), parseUnary());
    }
    return l;
  }
  ExprPtr parseUnary() {
    if (at(Tok::Minus)) { advance(); return std::make_unique<UnaryExpr>('-', parseUnary()); }
    if (at(Tok::Bang))  { advance(); return std::make_unique<UnaryExpr>('!', parseUnary()); }
    return parsePostfix();
  }
  ExprPtr parsePostfix() {
    ExprPtr e = parsePrimary();
    while (at(Tok::LBracket)) {
      advance();
      auto ix = std::make_unique<IndexExpr>();
      ix->base = std::move(e);
      ix->index = parseExpr();
      expect(Tok::RBracket, "']'");
      e = std::move(ix);
    }
    return e;
  }

  ExprPtr parsePrimary() {
    if (at(Tok::Number)) return std::make_unique<NumberExpr>(advance().num);
    if (at(Tok::String)) return std::make_unique<StringExpr>(advance().text);
    if (at(Tok::Ident)) {
      const std::string& name = cur().text;
      if (name == "true")  { advance(); return std::make_unique<BoolExpr>(true); }
      if (name == "false") { advance(); return std::make_unique<BoolExpr>(false); }
      if (name == "undef") { advance(); return std::make_unique<IdentExpr>("undef"); }
      if (peek().kind == Tok::LParen) {  // function call
        auto c = std::make_unique<CallExpr>();
        c->name = advance().text;
        advance();  // '('
        parseArgList(c->args);
        expect(Tok::RParen, "')'");
        return c;
      }
      return std::make_unique<IdentExpr>(advance().text);
    }
    if (at(Tok::LParen)) {
      advance();
      ExprPtr e = parseExpr();
      expect(Tok::RParen, "')'");
      return e;
    }
    if (at(Tok::LBracket)) return parseBracket();
    err("expected an expression");
  }

  // '[' -> either a vector literal or a range [start:end] / [start:step:end].
  ExprPtr parseBracket() {
    advance();  // '['
    if (at(Tok::RBracket)) { advance(); return std::make_unique<VectorExpr>(); }
    ExprPtr first = parseExpr();
    if (at(Tok::Colon)) {  // range
      advance();
      auto r = std::make_unique<RangeExpr>();
      r->start = std::move(first);
      ExprPtr second = parseExpr();
      if (at(Tok::Colon)) { advance(); r->step = std::move(second); r->end = parseExpr(); }
      else                { r->end = std::move(second); }
      expect(Tok::RBracket, "']'");
      return r;
    }
    auto v = std::make_unique<VectorExpr>();  // vector literal
    v->elems.push_back(std::move(first));
    while (at(Tok::Comma)) {
      advance();
      if (at(Tok::RBracket)) break;  // trailing comma
      v->elems.push_back(parseExpr());
    }
    expect(Tok::RBracket, "']'");
    return v;
  }
};

}  // namespace

std::vector<StmtPtr> parse(const std::vector<Token>& tokens) {
  Parser p(tokens);
  return p.parseProgram();
}
