#pragma once
#include <string>
#include <vector>

enum class Tok {
  End, Number, Ident, String,
  LParen, RParen, LBrace, RBrace, LBracket, RBracket,
  Comma, Semicolon, Equal, Colon, Question,
  Plus, Minus, Star, Slash, Percent, Bang,
  Less, Greater, LessEq, GreaterEq, EqEq, NotEq, And, Or
};

struct Token {
  Tok kind = Tok::End;
  std::string text;   // identifier name / string contents / raw number text
  double num = 0.0;   // value when kind == Number
  int line = 1;
};

// Tokenize SCAD source. Handles // and /* */ comments, numbers (incl. .5, 1e3),
// identifiers including leading '$' (so $fn/$fa/$fs lex as identifiers).
// Throws std::runtime_error on an unrecognized character.
std::vector<Token> tokenize(const std::string& src);
