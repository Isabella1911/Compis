#ifndef COMPISCRIPT_SEMANTIC_SYMBOL_H
#define COMPISCRIPT_SEMANTIC_SYMBOL_H

// Simbolo de la tabla de simbolos. Un Symbol por cada variable, constante,
// parametro, funcion o clase declarada.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ast/nodes.h"
#include "type.h"

namespace compiscript {
namespace semantic {

class Scope;  // adelante, definido en scope.h

enum class SymbolKind { Variable, Constant, Parameter, Function, Class };

class Symbol {
public:
    std::string name;
    SymbolKind kind;
    ast::TypeAnnotationPtr declared_type;  // tal como se escribio; puede ser null si no se anoto
    TypePtr resolved_type;                 // llenado por TypeChecker; null hasta entonces
    int declared_line = 0;
    int declared_column = 0;
    bool is_mutable = true;  // false para 'const'

    virtual ~Symbol() = default;
};
using SymbolPtr = std::shared_ptr<Symbol>;

// Una funcion necesita ademas su lista de parametros, tipo de retorno, y
// (cuando exista su cuerpo) el Scope propio donde viven esos parametros.
class FunctionSymbol : public Symbol {
public:
    std::vector<ast::Parameter> params;
    ast::TypeAnnotationPtr return_type;  // null -> se asume void
    Scope* function_scope = nullptr;     // no propietario; lo crea quien arma la tabla
};
using FunctionSymbolPtr = std::shared_ptr<FunctionSymbol>;

// Una clase necesita el nombre de su base (herencia con ':') y el Scope
// donde viven sus atributos/metodos.
class ClassSymbol : public Symbol {
public:
    std::optional<std::string> base_class_name;
    Scope* class_scope = nullptr;  // no propietario
};
using ClassSymbolPtr = std::shared_ptr<ClassSymbol>;

}  // namespace semantic
}  // namespace compiscript

#endif  // COMPISCRIPT_SEMANTIC_SYMBOL_H
