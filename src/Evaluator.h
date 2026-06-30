#pragma once
#include <vector>

#include "Ast.h"
#include "Node.h"

// Evaluate a parsed program into a single CSG root node (an implicit-union Group
// of all top-level objects). Top-level `name = expr;` assignments populate the
// global scope used while resolving module arguments.
// Throws std::runtime_error on unknown modules or bad arguments.
NodePtr evaluate(const std::vector<StmtPtr>& program);
