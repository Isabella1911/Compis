#ifndef COMPISCRIPT_SEMANTIC_DECLARATION_COLLECTOR_H
#define COMPISCRIPT_SEMANTIC_DECLARATION_COLLECTOR_H

// Pass 1 del analisis semantico: recorre el AST y puebla la tabla de
// simbolos (crea un Scope por cada funcion, clase y bloque; declara cada
// variable/constante/parametro/funcion/clase donde corresponde). NO
// resuelve referencias (eso es el proximo pass, NameResolver) ni valida
// tipos: solo construye la estructura y reporta redeclaraciones.

#include "ast/nodes.h"
#include "diagnostics/reporter.h"
#include "symbol_table.h"

namespace compiscript {
namespace semantic {

class DeclarationCollector {
public:
    DeclarationCollector(SymbolTable& table, diagnostics::DiagnosticReporter& reporter);

    void run(ast::Program& program);

private:
    void collectStatement(ast::Statement* stmt, Scope* scope);
    // Camina una lista de statements SIN crear un scope nuevo (para cuerpos
    // de funcion/clase, que ya reciben su scope propio del llamador).
    void collectBlockBody(const std::vector<ast::StatementPtr>& statements, Scope* scope);
    // Crea un scope hijo tipo Block y camina el bloque ahi adentro.
    void collectBlock(ast::Block* block, Scope* parentScope);

    // Declara `symbol` en `scope`; si ya existia un simbolo con ese nombre
    // en ese mismo scope, reporta SEM002 con la linea de la declaracion
    // original.
    void declareOrReport(Scope* scope, SymbolPtr symbol);

    SymbolTable& table_;
    diagnostics::DiagnosticReporter& reporter_;
};

}  // namespace semantic
}  // namespace compiscript

#endif  // COMPISCRIPT_SEMANTIC_DECLARATION_COLLECTOR_H
