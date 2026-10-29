#ifndef SQLINT_FRONTEND_PARSE_TREE_DOT_H
#define SQLINT_FRONTEND_PARSE_TREE_DOT_H

// Arbol sintactico de ANTLR en formato Graphviz (--emit parse-tree). Es el
// arbol "concreto": una hoja por token y un nodo por regla de SQL.g4, a
// diferencia del AST, que solo conserva lo que importa para ejecutar. El
// enunciado pide mostrar los dos.
//
// Solo lo incluye parser_driver.cpp: es el unico header de frontend/ que
// expone tipos de ANTLR hacia afuera de su .cpp.

#include <string>

#include "Parser.h"
#include "tree/ParseTree.h"

namespace sqlint {
namespace frontend {

std::string parseTreeToDot(antlr4::tree::ParseTree* root, const antlr4::Parser& parser);

}  // namespace frontend
}  // namespace sqlint

#endif  // SQLINT_FRONTEND_PARSE_TREE_DOT_H
