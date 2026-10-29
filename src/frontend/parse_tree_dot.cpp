#include "parse_tree_dot.h"

#include <sstream>

#include "ParserRuleContext.h"
#include "Token.h"
#include "tree/ErrorNode.h"
#include "tree/TerminalNode.h"

namespace sqlint {
namespace frontend {

namespace {

std::string dotEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (c == '\n') {
            out += "\\n";
        } else {
            out += c;
        }
    }
    return out;
}

class DotWriter {
public:
    explicit DotWriter(const antlr4::Parser& parser) : ruleNames_(parser.getRuleNames()) {}

    // Devuelve el id del nodo emitido.
    int visit(antlr4::tree::ParseTree* node) {
        int id = nextId_++;
        if (auto* error = dynamic_cast<antlr4::tree::ErrorNode*>(node)) {
            out_ << "  n" << id << " [label=\"" << dotEscape(error->getText())
                 << "\", shape=box, style=filled, fillcolor=\"#f8d7da\", color=\"#c0392b\"];\n";
            return id;
        }
        if (auto* terminal = dynamic_cast<antlr4::tree::TerminalNode*>(node)) {
            antlr4::Token* token = terminal->getSymbol();
            std::string text =
                token->getType() == antlr4::Token::EOF ? "<EOF>" : token->getText();
            out_ << "  n" << id << " [label=\"" << dotEscape(text)
                 << "\", shape=box, style=\"rounded,filled\", fillcolor=\"#e8f1fb\"];\n";
            return id;
        }
        auto* rule = dynamic_cast<antlr4::ParserRuleContext*>(node);
        std::string name = rule != nullptr && rule->getRuleIndex() < ruleNames_.size()
                               ? ruleNames_[rule->getRuleIndex()]
                               : "?";
        out_ << "  n" << id << " [label=\"" << dotEscape(name) << "\"];\n";
        for (antlr4::tree::ParseTree* child : node->children) {
            int childId = visit(child);
            out_ << "  n" << id << " -> n" << childId << ";\n";
        }
        return id;
    }

    std::string str() const { return out_.str(); }

private:
    const std::vector<std::string>& ruleNames_;
    std::ostringstream out_;
    int nextId_ = 0;
};

}  // namespace

std::string parseTreeToDot(antlr4::tree::ParseTree* root, const antlr4::Parser& parser) {
    DotWriter writer(parser);
    if (root != nullptr) writer.visit(root);

    std::ostringstream out;
    out << "digraph ParseTree {\n"
        << "  node [fontname=\"Helvetica\", fontsize=10, shape=ellipse];\n"
        << "  edge [arrowsize=0.6];\n"
        << writer.str() << "}\n";
    return out.str();
}

}  // namespace frontend
}  // namespace sqlint
