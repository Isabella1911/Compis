#ifndef COMPISCRIPT_SEMANTIC_CLOSURE_ANALYZER_H
#define COMPISCRIPT_SEMANTIC_CLOSURE_ANALYZER_H

// No valida nada: recorre el cuerpo de cada funcion (incluidas las
// anidadas) y anota en FunctionSymbol::captured cada variable resuelta
// fuera de su propio Scope (function_scope) pero usada dentro -- es
// exactamente lo que hace falta saber para implementar closures reales en
// tiempo de ejecucion mas adelante (fuera de este proyecto). Corre
// despues de NameResolver: necesita que cada IdentifierExpression ya
// tenga su `symbol` y su `scope` resueltos.
//
// Cuenta como "capturada" cualquier variable que no se declare dentro del
// propio cuerpo de la funcion (incluida una variable global) -- no solo
// las de una funcion contenedora inmediata. Es una simplificacion
// deliberada; se puede refinar si el equipo lo necesita.

#include "ast/nodes.h"
#include "scope.h"
#include "symbol.h"

namespace compiscript {
namespace semantic {

class ClosureAnalyzer {
public:
    void run(ast::Program& program);

private:
    // Busca FunctionDeclaration en cualquier parte del arbol (misma forma
    // que InheritanceResolver::collectClasses) y arranca un analisis
    // independiente por cada una.
    void findFunctions(ast::Statement* stmt);

    void analyzeFunctionBody(ast::FunctionDeclaration* fnDecl);

    // Recorre el cuerpo de UNA funcion (funcScope/fn fijos). Al toparse
    // con una funcion anidada, no sigue recorriendo bajo el contexto
    // actual: dispara un analisis aparte para esa funcion anidada.
    void walkStatement(ast::Statement* stmt, Scope* funcScope, FunctionSymbol* fn);
    void walkExpression(ast::Expression* expr, Scope* funcScope, FunctionSymbol* fn);

    // ¿La resolucion de `name` en `useSiteScope` vino de fuera de `funcScope`?
    bool isCaptured(Scope* useSiteScope, Scope* funcScope, const std::string& name) const;
};

}  // namespace semantic
}  // namespace compiscript

#endif  // COMPISCRIPT_SEMANTIC_CLOSURE_ANALYZER_H
