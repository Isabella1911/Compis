#ifndef COMPISCRIPT_SEMANTIC_SCOPE_H
#define COMPISCRIPT_SEMANTIC_SCOPE_H

// Tabla de simbolos implementada como "pila de tablas": cada Scope tiene
// sus propios simbolos y un puntero a su padre. Resolver un nombre busca
// primero en el scope actual y despues sube por los padres (ambito local
// antes que global). Declarar un nombre que ya existe EN EL MISMO scope es
// un error (redeclaracion); declarar el mismo nombre en un scope hijo es
// shadowing valido.
//
// El arbol de scopes es dueno de sus hijos (unique_ptr); todo lo demas
// navega con punteros crudos no propietarios, igual que resolved_type /
// symbol / scope en ast::AstNode.

#include <string>
#include <unordered_map>
#include <vector>

#include "symbol.h"

namespace compiscript {
namespace semantic {

enum class ScopeKind { Global, Function, Class, Block };

class Scope {
public:
    Scope(ScopeKind kind, Scope* parent);

    ScopeKind kind() const { return kind_; }
    Scope* parent() const { return parent_; }

    // Para Function/Class: el FunctionSymbol/ClassSymbol dueño de este
    // scope (no propietario). Lo fija DeclarationCollector al crear el
    // scope. Sirve para responder "¿en que funcion/clase estoy parado?"
    // al recorrer hacia arriba -- lo necesita, por ejemplo, validar el
    // tipo de retorno o resolver 'this'.
    Symbol* owner = nullptr;

    // Inserta `symbol` en este scope. Si ya hay un simbolo con el mismo
    // nombre EN ESTE MISMO scope, no lo reemplaza y retorna ese simbolo
    // existente; retiene el rechazado sin hacerlo visible (para que el AST siga
    // siendo valido y el llamador arme un diagnostico con la linea de
    // la declaracion original). Retorna nullptr si la insercion fue
    // exitosa.
    SymbolPtr declare(SymbolPtr symbol);

    // Busca solo en este scope, sin subir a los padres. Util para
    // verificar redeclaracion antes de declarar.
    SymbolPtr resolveLocal(const std::string& name) const;

    // Busca en este scope y, si no esta, sube por los padres hasta el
    // global. Es la resolucion de nombres real (ambito local o global).
    SymbolPtr resolve(const std::string& name) const;

    // Crea un scope hijo (funcion, clase o bloque) y retiene su
    // propiedad: el hijo vive mientras viva este Scope.
    Scope* createChild(ScopeKind kind);

    const std::vector<std::unique_ptr<Scope>>& children() const { return children_; }
    const std::unordered_map<std::string, SymbolPtr>& symbols() const { return symbols_; }
    const std::vector<SymbolPtr>& rejectedSymbols() const { return rejected_symbols_; }

private:
    ScopeKind kind_;
    Scope* parent_;
    std::unordered_map<std::string, SymbolPtr> symbols_;
    // No participan en resolve(); mantienen vivos los enlaces del AST y owner.
    std::vector<SymbolPtr> rejected_symbols_;
    std::vector<std::unique_ptr<Scope>> children_;
};

}  // namespace semantic
}  // namespace compiscript

#endif  // COMPISCRIPT_SEMANTIC_SCOPE_H
