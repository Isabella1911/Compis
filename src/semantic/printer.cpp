#include "printer.h"

#include <algorithm>
#include <sstream>
#include <vector>

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
        std::string signature = "(" + params + ") -> " +
                                  (fn->return_type ? typeText(fn->return_type) : "void");
        if (!fn->captured.empty()) {
            signature += "  [captura: ";
            for (size_t i = 0; i < fn->captured.size(); i++) {
                if (i > 0) signature += ", ";
                signature += fn->captured[i]->name;
            }
            signature += "]";
        }
        return signature;
    }
    if (auto* cls = dynamic_cast<const ClassSymbol*>(&symbol)) {
        return cls->base_class_name ? ("extiende " + *cls->base_class_name) : "sin base";
    }
    return typeText(symbol.declared_type);
}

std::string frameOffset(int offset) {
    return offset >= 0 ? "fp+" + std::to_string(offset) : "fp" + std::to_string(offset);
}

// Lo que agrego codegen/FrameLayout (y el layout de clases de la parte 2):
// donde vive cada simbolo en tiempo de ejecucion. Vacio si no se genero
// codigo (p. ej. porque hubo errores).
std::string codegenText(const Symbol& symbol) {
    if (auto* fn = dynamic_cast<const FunctionSymbol*>(&symbol)) {
        if (fn->label.empty()) return "";
        return "  [" + fn->label + ", frame " + std::to_string(fn->frame_size) +
               " bytes, anidamiento " + std::to_string(fn->nesting_depth) +
               (fn->is_method ? ", metodo" : "") +
               (fn->vtable_slot >= 0 ? ", slot " + std::to_string(fn->vtable_slot) : "") +
               (fn->overridden ? ", redefinido" : "") + "]";
    }
    if (auto* cls = dynamic_cast<const ClassSymbol*>(&symbol)) {
        if (cls->instance_size == 0) return "";
        return "  [instancia " + std::to_string(cls->instance_size) + " bytes, " +
               cls->vtable_label + "]";
    }

    std::string size = std::to_string(symbol.size) + " bytes";
    // Solo se muestra el nombre del TAC cuando difiere del fuente (sombreado).
    std::string alias = (!symbol.ir_name.empty() && symbol.ir_name != symbol.name)
                            ? ", en TAC " + symbol.ir_name
                            : "";
    switch (symbol.storage) {
        case StorageKind::None: return "";
        case StorageKind::Global: return "  [global " + symbol.ir_name + ", " + size + "]";
        case StorageKind::Param:
            return "  [param " + frameOffset(symbol.offset) + ", " + size + alias + "]";
        case StorageKind::Local:
            return "  [local " + frameOffset(symbol.offset) + ", " + size + alias + "]";
        case StorageKind::Field:
            return "  [atributo +" + std::to_string(symbol.offset) + ", " + size + "]";
    }
    return "";
}

// Registro de activacion completo de una funcion, casilla por casilla (el
// mismo esquema que docs/08 seccion 6), para que la tabla de simbolos muestre
// no solo donde vive cada parametro/local sino tambien los enlaces, la
// direccion de retorno y el area de temporales. Vacio si no se genero codigo.
std::string activationRecordText(const FunctionSymbol& fn, const std::string& indent) {
    if (fn.label.empty()) return "";
    constexpr int kWord = 4;
    const int params = static_cast<int>(fn.params.size()) + (fn.is_method ? 1 : 0);
    const int locals = fn.frame_size / kWord;
    const int total = kWord * params + 3 * kWord + fn.frame_size + kWord * fn.max_temps;

    auto range = [](int from, int to) {
        return from == to ? frameOffset(from) : frameOffset(from) + ".." + frameOffset(to);
    };
    auto row = [&indent](const std::string& where, const std::string& what) {
        std::string padded = where;
        if (padded.size() < 14) padded.append(14 - padded.size(), ' ');
        return indent + "      " + padded + what + "\n";
    };

    std::string out = indent + "    registro de activacion (" + std::to_string(total) + " bytes):\n";
    if (params > 0) {
        out += row(range(8, 8 + kWord * (params - 1)),
                   "parametros (" + std::to_string(params) + ")" +
                       (fn.is_method ? ", this en fp+8" : ""));
    }
    out += row("fp+4", fn.nesting_depth > 0 ? "enlace de acceso (frame de la funcion contenedora)"
                                             : "enlace de acceso (sin uso: no esta anidada)");
    out += row("fp+0", "enlace de control (fp del llamador)");
    out += row("fp-4", "direccion de retorno");
    if (locals > 0) {
        out += row(range(-8, -8 - kWord * (locals - 1)),
                   "locales (" + std::to_string(locals) + ")");
    }
    if (fn.max_temps > 0) {
        int first = -8 - fn.frame_size;
        out += row(range(first, first - kWord * (fn.max_temps - 1)),
                   "temporales (" + std::to_string(fn.max_temps) + ")");
    }
    out += indent + "      valor de retorno: " +
           (fn.return_type ? std::string("en registro ($v0)") : std::string("ninguno (void)")) +
           "\n";
    return out;
}

void printRec(const Scope* scope, int depth, std::ostringstream& out) {
    std::string indent(static_cast<size_t>(depth) * 2, ' ');
    out << indent << "Scope(" << scopeKindName(scope->kind()) << ")\n";

    // symbols() es un unordered_map: se ordena por posicion de declaracion
    // para que la salida sea estable entre corridas (IDE, pruebas).
    std::vector<const Symbol*> ordered;
    for (const auto& entry : scope->symbols()) ordered.push_back(entry.second.get());
    std::sort(ordered.begin(), ordered.end(), [](const Symbol* a, const Symbol* b) {
        if (a->declared_line != b->declared_line) return a->declared_line < b->declared_line;
        return a->declared_column < b->declared_column;
    });

    for (const Symbol* symbol : ordered) {
        out << indent << "  " << symbolKindName(symbol->kind) << " " << symbol->name << " : "
            << signatureText(*symbol) << "  (linea " << symbol->declared_line << ")"
            << codegenText(*symbol) << "\n";
        if (auto* fn = dynamic_cast<const FunctionSymbol*>(symbol)) {
            out << activationRecordText(*fn, indent);
        }
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
