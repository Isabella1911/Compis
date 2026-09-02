#ifndef COMPISCRIPT_AST_PRINTER_H
#define COMPISCRIPT_AST_PRINTER_H

#include <string>

#include "nodes.h"

namespace compiscript {
namespace ast {

// Texto indentado, para depurar en consola.
std::string printTree(const AstNode* root);

// Graphviz DOT, para que la IDE lo renderice (igual que Compis ya hace
// con el automata LR(0): dot -Tpng arbol.dot -o arbol.png).
std::string toDot(const AstNode* root);

}  // namespace ast
}  // namespace compiscript

#endif  // COMPISCRIPT_AST_PRINTER_H
