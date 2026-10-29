#ifndef SQLINT_STORAGE_VALUE_H
#define SQLINT_STORAGE_VALUE_H

// ============================================================================
// STUB DE LA PARTE 1. Este archivo es de la Parte 2 (docs/sql, contrato A.6).
// Solo tiene lo que el analisis semantico necesita para compilar y probarse:
// los tipos y la validacion de fechas. La Parte 2 lo reemplaza con la version
// completa (Value, compareValues, toCsvField, fromCsvField...) sin cambiar
// estas firmas.
// ============================================================================

#include <string>

namespace sqlint {
namespace storage {

enum class TypeKind { Null, Int, Float, Varchar, Boolean, Date };

struct SqlType {
    TypeKind kind = TypeKind::Null;
    int length = 0;  // solo VARCHAR(n)
};

struct Date {
    int y = 0;
    int m = 0;
    int d = 0;
};

// Valida YYYY-MM-DD con una fecha que exista (2024-02-29 si, 2026-02-29 no).
bool parseDate(const std::string& text, Date& out);

// "INT", "VARCHAR(50)", "NULL", ...
std::string typeName(const SqlType& type);

}  // namespace storage
}  // namespace sqlint

#endif  // SQLINT_STORAGE_VALUE_H
