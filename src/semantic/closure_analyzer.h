#ifndef COMPISCRIPT_SEMANTIC_CLOSURE_ANALYZER_H
#define COMPISCRIPT_SEMANTIC_CLOSURE_ANALYZER_H

// Capturas estaticas de variables/constantes/parametros de entornos exteriores.
// Excluye globales, funciones y clases; propaga capturas por funciones intermedias.
// El receptor lexico se registra por separado como captured_this. No hay runtime.

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

    void capture(Symbol* symbol, Scope* funcScope);
    void captureThis(ClassSymbol* cls, Scope* funcScope);

};

}  // namespace semantic
}  // namespace compiscript

#endif  // COMPISCRIPT_SEMANTIC_CLOSURE_ANALYZER_H
