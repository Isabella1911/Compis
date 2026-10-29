#ifndef SQLINT_SEMANTIC_TYPE_RULES_H
#define SQLINT_SEMANTIC_TYPE_RULES_H

// Reglas de compatibilidad de tipos (docs/sql, Parte 1 §4.5). Funciones
// puras sobre SqlType: el analyzer decide que error reportar y donde; aqui
// solo se responde "se puede o no" y "de que tipo sale".
//
//   - INT y FLOAT se mezclan; si alguno es FLOAT, el resultado es FLOAT.
//   - INT / INT es division entera y da INT.
//   - NULL es compatible con todo.
//   - Se comparan numerico con numerico y cada tipo consigo mismo
//     (VARCHAR de cualquier largo entre si). DATE contra una cadena solo se
//     acepta si la cadena es un literal con fecha valida: eso lo resuelve el
//     analyzer, que convierte el literal a DATE antes de preguntar aqui.

#include "ast/nodes.h"
#include "storage/value.h"

namespace sqlint {
namespace semantic {

bool isNumeric(const storage::SqlType& t);
bool isNull(const storage::SqlType& t);

// a op b con op de comparacion (=, <>, <, <=, >, >=) o IN.
bool comparable(const storage::SqlType& a, const storage::SqlType& b);

// a op b con op aritmetico. Devuelve false si no es valido; si lo es, deja
// el tipo resultante en `result`.
bool arithmeticResult(const storage::SqlType& a, const storage::SqlType& b,
                      storage::SqlType& result);

// AND, OR, NOT y condiciones (WHERE, ON, HAVING): BOOLEAN o NULL.
bool isBooleanLike(const storage::SqlType& t);

// Se puede guardar un valor de tipo `value` en una columna de tipo `column`
// (INSERT y UPDATE). El largo de VARCHAR se revisa aparte, solo para literales.
bool assignable(const storage::SqlType& column, const storage::SqlType& value);

// Tipo de columna a partir de lo escrito en CREATE TABLE.
storage::SqlType fromSpec(const ast::DataTypeSpec& spec);

}  // namespace semantic
}  // namespace sqlint

#endif  // SQLINT_SEMANTIC_TYPE_RULES_H
