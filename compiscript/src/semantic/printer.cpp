#include "printer.h"

#include <sstream>

namespace compiscript {
namespace semantic {

namespace {

std::string scopeKindName(ScopeKind kind) {
    switch (kind) {
        case ScopeKind::Global: return "Global";
        case ScopeKind::Function: return "Function";
        case ScopeKind::Class: return "Class";
        case ScopeKind::Block: return "Block";
    }
    return "?";
}

std::string symbolKindName(SymbolKind kind) {
    switch (kind) {
        case SymbolKind::Variable: return "var";
        case SymbolKind::Constant: return "const";
        case SymbolKind::Parameter: return "param";
        case SymbolKind::Function: return "function";
        case SymbolKind::Class: return "class";
    }
    return "?";
}

// Descripcion breve del tipo tal como se escribio (sin resolver todavia).
std::string typeText(const ast::TypeAnnotationPtr& type) {
    if (!type) return "sin anotar";
    if (auto* named = dynamic_cast<ast::NamedTypeAnnotation*>(type.get())) {
        return named->name;
    }
    if (auto* arr = dynamic_cast<ast::ArrayTypeAnnotation*>(type.get())) {
        return typeText(arr->element) + "[]";
    }
    return "?";
}

// declared_type solo aplica a variables/constantes/parametros; las
// funciones describen su tipo de retorno y las clases su clase base.
std::string signatureText(const Symbol& symbol) {
    if (auto* fn = dynamic_cast<const FunctionSymbol*>(&symbol)) {
        std::string params;
        for (size_t i = 0; i < fn->params.size(); i++) {
            if (i > 0) params += ", ";
            params += fn->params[i].name + ": " + typeText(fn->params[i].declared_type);
        }
        return "(" + params + ") -> " +
               (fn->return_type ? typeText(fn->return_type) : "void");
    }
    if (auto* cls = dynamic_cast<const ClassSymbol*>(&symbol)) {
        return cls->base_class_name ? ("extiende " + *cls->base_class_name) : "sin base";
    }
    return typeText(symbol.declared_type);
}

void printRec(const Scope* scope, int depth, std::ostringstream& out) {
    std::string indent(static_cast<size_t>(depth) * 2, ' ');
    out << indent << "Scope(" << scopeKindName(scope->kind()) << ")\n";
    for (const auto& [name, symbol] : scope->symbols()) {
        out << indent << "  " << symbolKindName(symbol->kind) << " " << name << " : "
            << signatureText(*symbol) << "  (linea " << symbol->declared_line << ")\n";
    }
    for (const auto& child : scope->children()) {
        printRec(child.get(), depth + 1, out);
    }
}

}  // namespace

std::string printScopeTree(const Scope* scope) {
    std::ostringstream out;
    printRec(scope, 0, out);
    return out.str();
}

}  // namespace semantic
}  // namespace compiscript
