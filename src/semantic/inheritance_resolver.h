#ifndef COMPISCRIPT_SEMANTIC_INHERITANCE_RESOLVER_H
#define COMPISCRIPT_SEMANTIC_INHERITANCE_RESOLVER_H

// Resuelve ClassSymbol::base_class_name (un string, ':' Identifier tal
// como se escribio) al ClassSymbol real de la clase base, y detecta
// herencia circular. Corre despues de DeclarationCollector (necesita que
// TODAS las clases ya esten declaradas, porque una clase puede heredar de
// otra declarada mas abajo en el archivo) y antes de cualquier pass que
// necesite recorrer una cadena de herencia (busqueda de miembros
// heredados, 'this', etc. -- ver docs/03_passes_semanticos.md, seccion 5).

#include "ast/nodes.h"
#include "diagnostics/reporter.h"

namespace compiscript {
namespace semantic {

class InheritanceResolver {
public:
    explicit InheritanceResolver(diagnostics::DiagnosticReporter& reporter);

    void run(ast::Program& program);

private:
    // Encuentra todas las ClassDeclaration del arbol, sin importar que
    // tan anidadas esten (una clase declarada dentro del cuerpo de una
    // funcion es sintacticamente legal, aunque poco comun).
    void collectClasses(ast::Statement* stmt, std::vector<ast::ClassDeclaration*>& out);

    void detectCycle(ast::ClassDeclaration* classDecl);

    diagnostics::DiagnosticReporter& reporter_;
};

}  // namespace semantic
}  // namespace compiscript

#endif  // COMPISCRIPT_SEMANTIC_INHERITANCE_RESOLVER_H
