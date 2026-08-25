#include "printer.h"

#include <sstream>
#include <unordered_map>
#include <vector>

namespace compiscript {
namespace ast {

namespace {

struct NodeView {
    std::string label;
    std::vector<std::pair<std::string, const AstNode*>> children;
};

std::string literalKindName(LiteralKind kind) {
    switch (kind) {
        case LiteralKind::Integer: return "integer";
        case LiteralKind::String: return "string";
        case LiteralKind::Boolean: return "boolean";
        case LiteralKind::Null: return "null";
    }
    return "?";
}

std::string paramsText(const std::vector<Parameter>& params) {
    std::ostringstream out;
    out << "[";
    for (size_t i = 0; i < params.size(); i++) {
        if (i > 0) out << ", ";
        out << params[i].name;
        if (params[i].declared_type) {
            // Se resuelve el nombre del tipo con una descripcion minima,
            // suficiente para el arbol visual (el detalle completo lo da
            // resolved_type en etapas posteriores).
            if (auto* named = dynamic_cast<NamedTypeAnnotation*>(params[i].declared_type.get())) {
                out << ": " << named->name;
            } else {
                out << ": []";
            }
        }
    }
    out << "]";
    return out.str();
}

// describe() concentra TODO el conocimiento de la jerarquia de nodos en un
// solo lugar: tanto el printer de texto como el de DOT reutilizan esta
// funcion en vez de duplicar el dynamic_cast por cada formato de salida.
NodeView describe(const AstNode* node) {
    if (node == nullptr) return {"<null>", {}};

    if (auto* n = dynamic_cast<const Program*>(node)) {
        NodeView v{"Program", {}};
        for (auto& s : n->statements) v.children.push_back({"", s.get()});
        return v;
    }
    if (auto* n = dynamic_cast<const VariableDeclaration*>(node)) {
        NodeView v{n->keyword + " " + n->name, {}};
        if (n->declared_type) v.children.push_back({"type", n->declared_type.get()});
        if (n->initializer) v.children.push_back({"init", n->initializer.get()});
        return v;
    }
    if (auto* n = dynamic_cast<const ConstantDeclaration*>(node)) {
        NodeView v{"const " + n->name, {}};
        if (n->declared_type) v.children.push_back({"type", n->declared_type.get()});
        v.children.push_back({"init", n->initializer.get()});
        return v;
    }
    if (auto* n = dynamic_cast<const AssignmentStatement*>(node)) {
        return {"AssignmentStatement(" + n->target_name + ")", {{"value", n->value.get()}}};
    }
    if (auto* n = dynamic_cast<const PropertyAssignment*>(node)) {
        return {"PropertyAssignment(." + n->member_name + ")",
                {{"object", n->object.get()}, {"value", n->value.get()}}};
    }
    if (auto* n = dynamic_cast<const ExpressionStatement*>(node)) {
        return {"ExpressionStatement", {{"", n->expression.get()}}};
    }
    if (auto* n = dynamic_cast<const PrintStatement*>(node)) {
        return {"PrintStatement", {{"", n->expression.get()}}};
    }
    if (auto* n = dynamic_cast<const Block*>(node)) {
        NodeView v{"Block", {}};
        for (auto& s : n->statements) v.children.push_back({"", s.get()});
        return v;
    }
    if (auto* n = dynamic_cast<const IfStatement*>(node)) {
        NodeView v{"IfStatement", {{"cond", n->condition.get()}, {"then", n->then_block.get()}}};
        if (n->else_block) v.children.push_back({"else", n->else_block.get()});
        return v;
    }
    if (auto* n = dynamic_cast<const WhileStatement*>(node)) {
        return {"WhileStatement", {{"cond", n->condition.get()}, {"body", n->body.get()}}};
    }
    if (auto* n = dynamic_cast<const DoWhileStatement*>(node)) {
        return {"DoWhileStatement", {{"body", n->body.get()}, {"cond", n->condition.get()}}};
    }
    if (auto* n = dynamic_cast<const ForStatement*>(node)) {
        NodeView v{"ForStatement", {}};
        if (n->init) v.children.push_back({"init", n->init.get()});
        if (n->condition) v.children.push_back({"cond", n->condition.get()});
        if (n->update) v.children.push_back({"update", n->update.get()});
        v.children.push_back({"body", n->body.get()});
        return v;
    }
    if (auto* n = dynamic_cast<const ForeachStatement*>(node)) {
        return {"ForeachStatement(" + n->var_name + ")",
                {{"iterable", n->iterable.get()}, {"body", n->body.get()}}};
    }
    if (auto* n = dynamic_cast<const SwitchCase*>(node)) {
        NodeView v{"SwitchCase", {{"value", n->expression.get()}}};
        for (auto& s : n->statements) v.children.push_back({"", s.get()});
        return v;
    }
    if (auto* n = dynamic_cast<const SwitchStatement*>(node)) {
        NodeView v{"SwitchStatement", {{"subject", n->subject.get()}}};
        for (auto& c : n->cases) v.children.push_back({"case", c.get()});
        if (n->has_default) {
            for (auto& s : n->default_statements) v.children.push_back({"default", s.get()});
        }
        return v;
    }
    if (auto* n = dynamic_cast<const TryCatchStatement*>(node)) {
        return {"TryCatchStatement(catch " + n->error_name + ")",
                {{"try", n->try_block.get()}, {"catch", n->catch_block.get()}}};
    }
    if (auto* n = dynamic_cast<const ReturnStatement*>(node)) {
        NodeView v{"ReturnStatement", {}};
        if (n->value) v.children.push_back({"", n->value.get()});
        return v;
    }
    if (dynamic_cast<const BreakStatement*>(node)) return {"BreakStatement", {}};
    if (dynamic_cast<const ContinueStatement*>(node)) return {"ContinueStatement", {}};
    if (auto* n = dynamic_cast<const FunctionDeclaration*>(node)) {
        NodeView v{"FunctionDeclaration " + n->name + paramsText(n->params), {}};
        if (n->return_type) v.children.push_back({"returns", n->return_type.get()});
        v.children.push_back({"body", n->body.get()});
        return v;
    }
    if (auto* n = dynamic_cast<const ClassDeclaration*>(node)) {
        std::string label = "ClassDeclaration " + n->name;
        if (n->base_name) label += " : " + *n->base_name;
        NodeView v{label, {}};
        for (auto& m : n->members) v.children.push_back({"member", m.get()});
        return v;
    }
    if (auto* n = dynamic_cast<const AssignmentExpression*>(node)) {
        return {"AssignmentExpression", {{"target", n->target.get()}, {"value", n->value.get()}}};
    }
    if (auto* n = dynamic_cast<const PropertyAssignExpr*>(node)) {
        return {"PropertyAssignExpr(." + n->member_name + ")",
                {{"object", n->object.get()}, {"value", n->value.get()}}};
    }
    if (auto* n = dynamic_cast<const TernaryExpression*>(node)) {
        return {"TernaryExpression",
                {{"cond", n->condition.get()},
                 {"then", n->then_expr.get()},
                 {"else", n->else_expr.get()}}};
    }
    if (auto* n = dynamic_cast<const BinaryExpression*>(node)) {
        return {"BinaryExpression(" + n->op + ")",
                {{"left", n->left.get()}, {"right", n->right.get()}}};
    }
    if (auto* n = dynamic_cast<const UnaryExpression*>(node)) {
        return {"UnaryExpression(" + n->op + ")", {{"", n->operand.get()}}};
    }
    if (auto* n = dynamic_cast<const LiteralExpression*>(node)) {
        return {"LiteralExpression(" + literalKindName(n->kind) + ", " + n->value + ")", {}};
    }
    if (auto* n = dynamic_cast<const ArrayLiteral*>(node)) {
        NodeView v{"ArrayLiteral", {}};
        for (auto& e : n->elements) v.children.push_back({"", e.get()});
        return v;
    }
    if (auto* n = dynamic_cast<const IdentifierExpression*>(node)) {
        return {"IdentifierExpression(" + n->name + ")", {}};
    }
    if (dynamic_cast<const ThisExpression*>(node)) return {"ThisExpression", {}};
    if (auto* n = dynamic_cast<const NewExpression*>(node)) {
        NodeView v{"NewExpression(" + n->class_name + ")", {}};
        for (auto& a : n->arguments) v.children.push_back({"arg", a.get()});
        return v;
    }
    if (auto* n = dynamic_cast<const CallExpression*>(node)) {
        NodeView v{"CallExpression", {{"callee", n->callee.get()}}};
        for (auto& a : n->arguments) v.children.push_back({"arg", a.get()});
        return v;
    }
    if (auto* n = dynamic_cast<const ArrayAccessExpression*>(node)) {
        return {"ArrayAccessExpression", {{"array", n->array.get()}, {"index", n->index.get()}}};
    }
    if (auto* n = dynamic_cast<const MemberAccessExpression*>(node)) {
        return {"MemberAccessExpression(." + n->member_name + ")",
                {{"object", n->object.get()}}};
    }
    if (auto* n = dynamic_cast<const NamedTypeAnnotation*>(node)) {
        return {"Type(" + n->name + ")", {}};
    }
    if (auto* n = dynamic_cast<const ArrayTypeAnnotation*>(node)) {
        return {"Type([])", {{"element", n->element.get()}}};
    }

    return {"<nodo desconocido>", {}};
}

void printTextRec(const AstNode* node, int depth, std::ostringstream& out) {
    NodeView v = describe(node);
    out << std::string(static_cast<size_t>(depth) * 2, ' ');
    out << v.label << "  (linea " << node->line << ", col " << node->column << ")\n";
    for (auto& [edgeLabel, child] : v.children) {
        if (child == nullptr) continue;
        if (!edgeLabel.empty()) {
            out << std::string(static_cast<size_t>(depth + 1) * 2, ' ') << "- " << edgeLabel
                << ":\n";
            printTextRec(child, depth + 2, out);
        } else {
            printTextRec(child, depth + 1, out);
        }
    }
}

void toDotRec(const AstNode* node, std::unordered_map<const AstNode*, int>& ids, int& counter,
              std::ostringstream& out) {
    if (ids.count(node)) return;
    int id = counter++;
    ids[node] = id;
    NodeView v = describe(node);
    std::string escaped;
    for (char c : v.label) {
        if (c == '"' || c == '\\') escaped += '\\';
        escaped += c;
    }
    out << "  n" << id << " [label=\"" << escaped << "\"];\n";
    for (auto& [edgeLabel, child] : v.children) {
        if (child == nullptr) continue;
        toDotRec(child, ids, counter, out);
        out << "  n" << id << " -> n" << ids[child];
        if (!edgeLabel.empty()) out << " [label=\"" << edgeLabel << "\"]";
        out << ";\n";
    }
}

}  // namespace

std::string printTree(const AstNode* root) {
    std::ostringstream out;
    printTextRec(root, 0, out);
    return out.str();
}

std::string toDot(const AstNode* root) {
    std::ostringstream out;
    out << "digraph AST {\n  node [shape=box, fontname=\"monospace\"];\n";
    std::unordered_map<const AstNode*, int> ids;
    int counter = 0;
    toDotRec(root, ids, counter, out);
    out << "}\n";
    return out.str();
}

}  // namespace ast
}  // namespace compiscript
