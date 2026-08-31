#ifndef COMPISCRIPT_SEMANTIC_PRINTER_H
#define COMPISCRIPT_SEMANTIC_PRINTER_H

#include <string>

#include "scope.h"

namespace compiscript {
namespace semantic {

// Texto indentado con cada scope y sus simbolos, recursivo. Es el "Estado
// de la tabla de simbolos por cada entorno" que pide el enunciado como
// salida -- todavia sin tipos resueltos (eso llega con el sistema de tipos).
std::string printScopeTree(const Scope* scope);

}  // namespace semantic
}  // namespace compiscript

#endif  // COMPISCRIPT_SEMANTIC_PRINTER_H
