#ifndef SQLINT_STORAGE_VALUE_H
#define SQLINT_STORAGE_VALUE_H

// Valores y tipos del interprete (contrato A.6 de docs/sql; dueno: Parte 2).
//
// Un Value es lo que vive en una celda: en memoria (filas, resultados,
// indices) y en disco (un campo del CSV o la llave de un indice). La forma
// en disco la definen toCsvField/fromCsvField, que son simetricas:
//
//   fromCsvField(toCsvField(v), tipo) == v   para todo v de ese tipo.
//
// Formato de un campo (docs/sql/parte2, seccion 4.2):
//   NULL            campo vacio sin comillas
//   ''              ""   (la cadena vacia siempre lleva comillas)
//   INT / FLOAT     punto decimal, sin separador de miles
//   BOOLEAN         TRUE / FALSE
//   DATE            YYYY-MM-DD
//   VARCHAR         tal cual; con comillas si tiene coma, comilla, tab,
//                   salto de linea o espacios en los extremos ("" adentro)

#include <cstdint>
#include <string>
#include <variant>

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

struct Value {
    TypeKind kind = TypeKind::Null;
    std::variant<std::monostate, int64_t, double, std::string, bool, Date> v;

    bool isNull() const { return kind == TypeKind::Null; }

    static Value null() { return Value(); }
    static Value ofInt(int64_t x) { return Value{TypeKind::Int, x}; }
    static Value ofFloat(double x) { return Value{TypeKind::Float, x}; }
    static Value ofString(std::string s) { return Value{TypeKind::Varchar, std::move(s)}; }
    static Value ofBool(bool b) { return Value{TypeKind::Boolean, b}; }
    static Value ofDate(Date d) { return Value{TypeKind::Date, d}; }

    // Acceso directo; quien llama ya reviso kind.
    int64_t asInt() const { return std::get<int64_t>(v); }
    double asFloat() const { return std::get<double>(v); }
    const std::string& asString() const { return std::get<std::string>(v); }
    bool asBool() const { return std::get<bool>(v); }
    const Date& asDate() const { return std::get<Date>(v); }
};

// -1, 0 o 1. Reglas:
//   - INT contra FLOAT se compara como double;
//   - VARCHAR byte a byte, distinguiendo mayusculas ('B' < 'a');
//   - DATE como (anio, mes, dia);
//   - FALSE < TRUE.
// No acepta NULL: quien llama decide que significa (en SQL, comparar con
// NULL da NULL). Tipos que no se pueden comparar se ordenan por TypeKind,
// solo para que el orden sea total; el semantico no deja llegar a ese caso.
int compareValues(const Value& a, const Value& b);

// Orden para std::map / std::sort, basado en compareValues.
struct ValueLess {
    bool operator()(const Value& a, const Value& b) const { return compareValues(a, b) < 0; }
};

// Campo del CSV ya listo para escribir (con comillas si hacen falta).
std::string toCsvField(const Value& value);

// Parsea un campo tal como esta en el archivo (con sus comillas, si tiene).
// Lanza SqlError RUN103 si el texto no es un valor valido del tipo.
Value fromCsvField(const std::string& field, const SqlType& type);

// Valida YYYY-MM-DD con una fecha que exista (2024-02-29 si, 2026-02-29 no).
bool parseDate(const std::string& text, Date& out);

std::string formatDate(const Date& d);  // "2024-01-15"

// "INT", "VARCHAR(50)", "NULL", ...
std::string typeName(const SqlType& type);

// Inverso de typeName. Devuelve false si el texto no es un tipo.
bool parseTypeName(const std::string& text, SqlType& out);

// Texto del valor sin comillas, para mensajes de error: 'Ana', 17, NULL.
std::string displayValue(const Value& value);

// Cantidad de caracteres de un texto UTF-8 (el n de VARCHAR(n) cuenta
// caracteres, no bytes, igual que el semantico).
int utf8Length(const std::string& s);

}  // namespace storage
}  // namespace sqlint

#endif  // SQLINT_STORAGE_VALUE_H
