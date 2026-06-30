#pragma once
#include <vector>

#include "Ast.h"
#include "Lexer.h"

// Recursive-descent parser for the Milestone-1 SCAD subset:
//   program    := statement*
//   statement  := IDENT '=' expr ';'                 (assignment)
//               | IDENT '(' args ')' child           (module call)
//   child      := ';' | '{' statement* '}' | statement
//   args       := (arg (',' arg)*)?
//   arg        := IDENT '=' expr | expr
//   expr       := add ; precedence: unary > * / > + -
// Throws std::runtime_error with a line number on a syntax error.
std::vector<StmtPtr> parse(const std::vector<Token>& tokens);
