#ifndef COMPISCRIPT_SEMANTIC_SYMBOL_TABLE_H
#define COMPISCRIPT_SEMANTIC_SYMBOL_TABLE_H

// Punto de entrada unico a la tabla de simbolos: dueno del scope global,
// del que cuelgan todos los demas (funcion, clase, bloque). Esto es lo que
// eventualmente llena CompilationResult::symbol_table.

#include <memory>

#include "scope.h"

namespace compiscript {
namespace semantic {

class SymbolTable {
public:
    SymbolTable() : global_(std::make_unique<Scope>(ScopeKind::Global, nullptr)) {}

    Scope* global() const { return global_.get(); }

private:
    std::unique_ptr<Scope> global_;
};

}  // namespace semantic
}  // namespace compiscript

#endif  // COMPISCRIPT_SEMANTIC_SYMBOL_TABLE_H
