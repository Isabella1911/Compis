#ifndef COMPISCRIPT_SEMANTIC_NAME_RESOLVER_H
#define COMPISCRIPT_SEMANTIC_NAME_RESOLVER_H

// Pass 2 del analisis semantico: recorre el AST (esta vez SI entrando a las
// expresiones) y resuelve cada identificador usado contra la tabla de
// simbolos ya poblada por DeclarationCollector. Reporta SEM001 cuando algo
// no existe.
//
// Deliberadamente NO resuelve todavia:
//   - nombres de miembro (`obj.campo`, `this.campo`): resolverlos requiere
//     saber el tipo estatico de `obj`, y el sistema de tipos es la
//     siguiente rebanada.
//   - la palabra `this`: su validez depende del contexto de clase, que es
//     una regla semantica propia ("Clases y Objetos" en el PDF), no un
//     caso de "variable no declarada".
//   - numero/tipo de argumentos en llamadas: eso es la rebanada de
//     "Funciones y Procedimientos".

#include "ast/nodes.h"
#include "diagnostics/reporter.h"
#include "scope.h"

namespace compiscript {
namespace semantic {

class NameResolver {
public:
    explicit NameResolver(diagnostics::DiagnosticReporter& reporter);

    void run(ast::Program& program);

private:
    void resolveStatement(ast::Statement* stmt);
    void resolveExpression(ast::Expression* expr, Scope* scope);

    // Busca `name` en `scope`; si no existe, reporta SEM001 en la posicion
    // (line, column) dada. Retorna el simbolo encontrado o nullptr.
    SymbolPtr resolveOrReport(Scope* scope, const std::string& name, int line, int column);

    diagnostics::DiagnosticReporter& reporter_;
};

}  // namespace semantic
}  // namespace compiscript

#endif  // COMPISCRIPT_SEMANTIC_NAME_RESOLVER_H
