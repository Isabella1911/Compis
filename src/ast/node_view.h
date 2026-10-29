#ifndef SQLINT_AST_NODE_VIEW_H
#define SQLINT_AST_NODE_VIEW_H

// Vista generica del AST para dibujarlo: cada nodo se reduce a una etiqueta
// y sus hijos. El impresor de texto (printer.cpp) y el de Graphviz
// (ast_dot.cpp) recorren esta vista, asi que la forma del arbol se decide en
// un solo lugar (node_view.cpp) y las dos salidas nunca se contradicen.

#include <string>
#include <vector>

#include "nodes.h"

namespace sqlint {
namespace ast {

// Categoria del nodo, para colorearlo en el .dot.
enum class ViewCategory { Script, Statement, Clause, Expr, Column, Literal, Definition };

struct ViewNode {
    std::string label;
    ViewCategory category = ViewCategory::Expr;
    SourceLoc loc;
    std::vector<ViewNode> children;
};

ViewNode viewOf(const Script& script);

}  // namespace ast
}  // namespace sqlint

#endif  // SQLINT_AST_NODE_VIEW_H
