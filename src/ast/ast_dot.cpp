// toDot (declarada en printer.h): el AST en formato Graphviz.

#include <sstream>

#include "node_view.h"
#include "printer.h"

namespace sqlint {
namespace ast {

namespace {

std::string dotEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

// Mismo esquema de colores que la IDE: sentencias y clausulas en tonos
// neutros, columnas y literales resaltados (son las hojas que importan).
const char* fillColor(ViewCategory category) {
    switch (category) {
        case ViewCategory::Script: return "#e0e0e0";
        case ViewCategory::Statement: return "#cfe2ff";
        case ViewCategory::Clause: return "#f1f3f5";
        case ViewCategory::Expr: return "#fff3cd";
        case ViewCategory::Column: return "#d1e7dd";
        case ViewCategory::Literal: return "#f8d7da";
        case ViewCategory::Definition: return "#e2d9f3";
    }
    return "#ffffff";
}

int emit(const ViewNode& node, int& nextId, std::ostringstream& out) {
    int id = nextId++;
    out << "  n" << id << " [label=\"" << dotEscape(node.label) << "\", fillcolor=\""
        << fillColor(node.category) << "\"];\n";
    for (const auto& child : node.children) {
        int childId = emit(child, nextId, out);
        out << "  n" << id << " -> n" << childId << ";\n";
    }
    return id;
}

}  // namespace

std::string toDot(const Script& script) {
    std::ostringstream body;
    int nextId = 0;
    emit(viewOf(script), nextId, body);

    std::ostringstream out;
    out << "digraph AST {\n"
        << "  node [fontname=\"Helvetica\", fontsize=10, shape=box, style=\"rounded,filled\"];\n"
        << "  edge [arrowsize=0.6];\n"
        << body.str() << "}\n";
    return out.str();
}

}  // namespace ast
}  // namespace sqlint
