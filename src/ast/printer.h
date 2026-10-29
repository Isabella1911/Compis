#ifndef SQLINT_AST_PRINTER_H
#define SQLINT_AST_PRINTER_H

#include <string>

#include "nodes.h"

namespace sqlint {
namespace ast {

// AST como arbol de texto indentado (--emit ast), con la linea:columna de
// cada nodo:
//
//   Script
//   └─ Select  [2:1]
//      ├─ Columnas  [2:1]
//      │  └─ ColumnRef(nombre)  [2:8]
//      ...
std::string printTree(const Script& script);

// AST en formato Graphviz (--emit ast-dot).
std::string toDot(const Script& script);

}  // namespace ast
}  // namespace sqlint

#endif  // SQLINT_AST_PRINTER_H
