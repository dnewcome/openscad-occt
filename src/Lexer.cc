#include "Lexer.h"

#include <cctype>
#include <stdexcept>

namespace {
bool isIdentStart(char c) { return std::isalpha((unsigned char)c) || c == '_' || c == '$'; }
bool isIdentCont(char c)  { return std::isalnum((unsigned char)c) || c == '_'; }
}  // namespace

std::vector<Token> tokenize(const std::string& src) {
  std::vector<Token> out;
  size_t i = 0, n = src.size();
  int line = 1;

  auto push = [&](Tok k) { out.push_back(Token{k, "", 0.0, line}); };

  while (i < n) {
    char c = src[i];

    if (c == '\n') { ++line; ++i; continue; }
    if (std::isspace((unsigned char)c)) { ++i; continue; }

    // Comments.
    if (c == '/' && i + 1 < n && src[i + 1] == '/') {
      i += 2;
      while (i < n && src[i] != '\n') ++i;
      continue;
    }
    if (c == '/' && i + 1 < n && src[i + 1] == '*') {
      i += 2;
      while (i + 1 < n && !(src[i] == '*' && src[i + 1] == '/')) {
        if (src[i] == '\n') ++line;
        ++i;
      }
      i += 2;  // skip closing */
      continue;
    }

    // Numbers: digits with optional fraction/exponent, or a leading '.'.
    if (std::isdigit((unsigned char)c) || (c == '.' && i + 1 < n && std::isdigit((unsigned char)src[i + 1]))) {
      size_t start = i;
      while (i < n && std::isdigit((unsigned char)src[i])) ++i;
      if (i < n && src[i] == '.') { ++i; while (i < n && std::isdigit((unsigned char)src[i])) ++i; }
      if (i < n && (src[i] == 'e' || src[i] == 'E')) {
        ++i;
        if (i < n && (src[i] == '+' || src[i] == '-')) ++i;
        while (i < n && std::isdigit((unsigned char)src[i])) ++i;
      }
      std::string t = src.substr(start, i - start);
      out.push_back(Token{Tok::Number, t, std::stod(t), line});
      continue;
    }

    // Identifiers (and $special vars).
    if (isIdentStart(c)) {
      size_t start = i;
      ++i;
      while (i < n && isIdentCont(src[i])) ++i;
      out.push_back(Token{Tok::Ident, src.substr(start, i - start), 0.0, line});
      continue;
    }

    // Punctuation / operators.
    ++i;
    switch (c) {
      case '(': push(Tok::LParen); break;
      case ')': push(Tok::RParen); break;
      case '{': push(Tok::LBrace); break;
      case '}': push(Tok::RBrace); break;
      case '[': push(Tok::LBracket); break;
      case ']': push(Tok::RBracket); break;
      case ',': push(Tok::Comma); break;
      case ';': push(Tok::Semicolon); break;
      case '=': push(Tok::Equal); break;
      case '+': push(Tok::Plus); break;
      case '-': push(Tok::Minus); break;
      case '*': push(Tok::Star); break;
      case '/': push(Tok::Slash); break;
      case '!': push(Tok::Bang); break;
      default:
        throw std::runtime_error("lex error: unexpected character '" + std::string(1, c) +
                                 "' on line " + std::to_string(line));
    }
  }

  out.push_back(Token{Tok::End, "", 0.0, line});
  return out;
}
