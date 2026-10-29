#include "printer.h"

#include <sstream>

#include "node_view.h"

namespace sqlint {
namespace ast {

namespace {

void printNode(const ViewNode& node, const std::string& prefix, bool last, bool root,
               std::ostringstream& out) {
    if (root) {
        out << node.label << "\n";
    } else {
        out << prefix << (last ? "└─ " : "├─ ") << node.label;
        if (node.loc.line > 0) out << "  [" << node.loc.line << ":" << node.loc.col << "]";
        out << "\n";
    }
    std::string childPrefix = root ? "" : prefix + (last ? "   " : "│  ");
    for (size_t i = 0; i < node.children.size(); ++i) {
        printNode(node.children[i], childPrefix, i + 1 == node.children.size(), false, out);
    }
}

}  // namespace

std::string printTree(const Script& script) {
    std::ostringstream out;
    printNode(viewOf(script), "", true, true, out);
    return out.str();
}

}  // namespace ast
}  // namespace sqlint
