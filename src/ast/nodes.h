#ifndef SQLINT_AST_NODES_H
#define SQLINT_AST_NODES_H

// AST del interprete SQL (contrato A.6 de docs/sql; dueno: Parte 1).
//
// Es la forma "limpia" del script: sin parentesis, sin ';', sin palabras
// clave, con las variantes de la gramatica ya normalizadas (JOIN == INNER
// JOIN, != == <>, NOT IN == InList con negated). Lo arma el AstBuilder
// (frontend/) y lo consumen el semantico (Parte 1) y la ejecucion
// (Partes 2 y 3). Ningun nodo conoce ANTLR.
//
// Cada nodo guarda su SourceLoc. Los nombres (Ident) guardan ademas la
// posicion del propio nombre, para que un error como "columna inexistente"
// senale exactamente el identificador y no el inicio de la sentencia.
//
// Anotaciones: los campos marcados "[sem]" arrancan vacios y los llena el
// analisis semantico (semantic/analyzer). Si analyze() no devolvio errores,
// todos estan llenos y la ejecucion los usa sin volver a resolver nombres.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "diagnostics/diagnostic.h"
#include "storage/value.h"

namespace sqlint {
namespace ast {

using diagnostics::SourceLoc;

// Nombre tal como se escribio en el script (sin cambiar mayusculas).
struct Ident {
    std::string name;
    SourceLoc loc;

    bool empty() const { return name.empty(); }
};

struct Node {
    SourceLoc loc;
    virtual ~Node() = default;
};

// ---------------------------------------------------------------------------
// Expresiones
// ---------------------------------------------------------------------------

enum class ExprKind {
    Literal,
    ColumnRef,
    Unary,
    Binary,
    IsNull,
    InList,
    InSubquery,
    Exists,
    ScalarSubquery,
    Aggregate,
};

struct Expr : Node {
    const ExprKind kind;
    storage::SqlType type;  // [sem] tipo del resultado; Null para el literal NULL

    explicit Expr(ExprKind k) : kind(k) {}
};
using ExprPtr = std::unique_ptr<Expr>;

// ---------------------------------------------------------------------------
// SELECT (tambien se usa como subconsulta)
//
// Va antes de las expresiones concretas porque las subconsultas (InSubquery,
// Exists, ScalarSubquery) contienen un Select completo.
// ---------------------------------------------------------------------------

// tabla [AS] alias
struct TableRef {
    Ident name;
    Ident alias;  // vacio si no hay alias
    SourceLoc loc;
};

enum class SelectItemKind { AllColumns, TableColumns, Expr };

struct SelectItem {
    int source = -1;  // [sem] fuente de t.*; evita resolver nombres al ejecutar
    SelectItemKind kind = SelectItemKind::Expr;
    Ident table;   // TableColumns: la t de "t.*"
    ExprPtr expr;  // Expr
    Ident alias;   // Expr: [AS] alias, vacio si no hay
    std::string text;  // texto original del item, para nombrar la columna del resultado
    SourceLoc loc;
};

// [INNER] JOIN tabla [AS] alias ON condicion
struct Join {
    TableRef table;
    ExprPtr on;
    SourceLoc loc;
};

struct OrderItem {
    ExprPtr expr;
    bool descending = false;
    SourceLoc loc;
};

// Una columna del resultado de un SELECT, con "*" y "t.*" ya expandidos.
struct OutputColumn {
    std::string name;  // alias, nombre de la columna o texto de la expresion
    storage::SqlType type;
};

struct Select : Node {
    std::vector<SelectItem> items;
    TableRef from;
    std::vector<Join> joins;
    ExprPtr where;                 // null si no hay WHERE
    std::vector<ExprPtr> groupBy;  // vacio si no hay GROUP BY
    ExprPtr having;                // null si no hay HAVING
    std::vector<OrderItem> orderBy;

    std::vector<OutputColumn> outputColumns;  // [sem] en el orden del resultado
    bool isCorrelated = false;  // [sem] alguna columna interna es de una consulta externa
};

enum class LiteralKind { Int, Float, String, Date, Bool, Null };

struct Literal : Expr {
    LiteralKind literalKind = LiteralKind::Null;
    std::string text;         // lexema original: "12", "'O''Neil'", "DATE '2024-01-15'"
    int64_t intValue = 0;     // Int
    double floatValue = 0.0;  // Float
    bool boolValue = false;   // Bool
    std::string stringValue;  // String y Date: contenido sin comillas, '' ya convertido a '
    bool intOverflow = false; // Int: el numero no cabe en 64 bits (lo reporta el semantico)

    Literal() : Expr(ExprKind::Literal) {}
};

// [sem] A que columna se refiere una ColumnRef.
//   depth  = 0 si es de la consulta actual, 1 si es de la consulta externa...
//   source = indice de la tabla en el FROM/JOIN de esa consulta (0 = FROM)
//   column = indice de la columna en esa tabla
// En ORDER BY una columna puede nombrar un alias de la lista del SELECT: en
// ese caso outputColumn es el indice en Select::outputColumns y los otros
// tres campos quedan en -1.
struct ColumnBinding {
    int depth = -1;
    int source = -1;
    int column = -1;
    int outputColumn = -1;

    bool resolved() const { return column >= 0 || outputColumn >= 0; }
};

// col  o  tabla.col  (tabla puede ser un alias).
struct ColumnRef : Expr {
    Ident table;   // vacio si la columna no esta calificada
    Ident column;
    ColumnBinding binding;  // [sem]

    ColumnRef() : Expr(ExprKind::ColumnRef) {}
};

enum class UnaryOp { Neg, Plus, Not };

struct Unary : Expr {
    UnaryOp op = UnaryOp::Neg;
    ExprPtr operand;

    Unary() : Expr(ExprKind::Unary) {}
};

enum class BinaryOp { Add, Sub, Mul, Div, Mod, Eq, Neq, Lt, Le, Gt, Ge, And, Or };

struct Binary : Expr {
    BinaryOp op = BinaryOp::Eq;
    ExprPtr left;
    ExprPtr right;
    SourceLoc opLoc;  // posicion del operador (para "tipos incompatibles")

    Binary() : Expr(ExprKind::Binary) {}
};

// expr IS [NOT] NULL
struct IsNull : Expr {
    ExprPtr operand;
    bool negated = false;

    IsNull() : Expr(ExprKind::IsNull) {}
};

// expr [NOT] IN (a, b, c)
struct InList : Expr {
    ExprPtr operand;
    std::vector<ExprPtr> items;
    bool negated = false;

    InList() : Expr(ExprKind::InList) {}
};

// expr [NOT] IN (SELECT ...)
struct InSubquery : Expr {
    ExprPtr operand;
    std::unique_ptr<Select> subquery;
    bool negated = false;

    InSubquery() : Expr(ExprKind::InSubquery) {}
};

// [NOT] EXISTS (SELECT ...)
struct Exists : Expr {
    std::unique_ptr<Select> subquery;
    bool negated = false;

    Exists() : Expr(ExprKind::Exists) {}
};

// (SELECT ...) usado como valor
struct ScalarSubquery : Expr {
    std::unique_ptr<Select> subquery;

    ScalarSubquery() : Expr(ExprKind::ScalarSubquery) {}
};

enum class AggFunc { Count, Sum, Avg, Min, Max };

// COUNT(*) tiene star = true y arg == nullptr.
struct Aggregate : Expr {
    AggFunc func = AggFunc::Count;
    bool star = false;
    ExprPtr arg;

    Aggregate() : Expr(ExprKind::Aggregate) {}
};

// ---------------------------------------------------------------------------
// Sentencias
// ---------------------------------------------------------------------------

enum class StatementKind {
    CreateTable,
    DropTable,
    CreateIndex,
    DropIndex,
    Insert,
    Update,
    Delete,
    Select,
};

struct Statement : Node {
    const StatementKind kind;
    explicit Statement(StatementKind k) : kind(k) {}
};
using StatementPtr = std::unique_ptr<Statement>;

// Tipo de columna tal como se escribio en CREATE TABLE. El semantico lo
// convierte al SqlType del catalogo.
enum class TypeName { Int, Float, Varchar, Boolean, Date };

struct DataTypeSpec {
    TypeName name = TypeName::Int;
    int64_t length = 0;  // solo VARCHAR(n); VARCHAR(0) lo rechaza el semantico (SEM014)
    SourceLoc loc;
};

struct ColumnDef {
    Ident name;
    DataTypeSpec type;
    bool notNull = false;
    bool primaryKey = false;  // PRIMARY KEY escrito junto a la columna
    SourceLoc loc;
};

struct CreateTable : Statement {
    Ident table;
    std::vector<ColumnDef> columns;
    // PRIMARY KEY (col) escrito como elemento aparte. Se guarda tal cual (sin
    // marcar la columna) para que el semantico pueda reportar una columna
    // inexistente o una llave declarada dos veces con su posicion.
    std::vector<Ident> primaryKeyClauses;

    CreateTable() : Statement(StatementKind::CreateTable) {}
};

struct DropTable : Statement {
    Ident table;

    DropTable() : Statement(StatementKind::DropTable) {}
};

struct CreateIndex : Statement {
    Ident index;
    Ident table;
    Ident column;

    CreateIndex() : Statement(StatementKind::CreateIndex) {}
};

struct DropIndex : Statement {
    Ident index;

    DropIndex() : Statement(StatementKind::DropIndex) {}
};

struct Insert : Statement {
    Ident table;
    // Columnas de la lista "(a, b, c)". Si el script no la trae,
    // explicitColumns = false y el semantico [sem] la llena con todas las
    // columnas en el orden del esquema: la Parte 2 siempre recibe una lista
    // con la columna de cada valor. Las columnas de la tabla que no esten en
    // la lista se guardan como NULL.
    std::vector<Ident> columns;
    bool explicitColumns = false;
    std::vector<std::vector<ExprPtr>> rows;  // una entrada por "(...)" de VALUES
    std::vector<SourceLoc> rowLocs;          // posicion de cada "(" de VALUES

    Insert() : Statement(StatementKind::Insert) {}
};

struct Assignment {
    Ident column;
    ExprPtr value;
};

struct Update : Statement {
    Ident table;
    std::vector<Assignment> assignments;
    ExprPtr where;  // null si no hay WHERE

    Update() : Statement(StatementKind::Update) {}
};

struct Delete : Statement {
    Ident table;
    ExprPtr where;  // null si no hay WHERE

    Delete() : Statement(StatementKind::Delete) {}
};

struct SelectStatement : Statement {
    std::unique_ptr<Select> query;

    SelectStatement() : Statement(StatementKind::Select) {}
};

// Script completo: las sentencias en el orden en que aparecen.
struct Script {
    std::vector<StatementPtr> statements;
};

// Nombres legibles, usados por los impresores y por los mensajes de error.
const char* binaryOpSymbol(BinaryOp op);   // "+", "<=", "AND", ...
const char* unaryOpSymbol(UnaryOp op);     // "-", "+", "NOT"
const char* aggFuncName(AggFunc f);        // "COUNT", "SUM", ...
const char* statementKindName(StatementKind k);  // "SELECT", "CREATE TABLE", ...
std::string typeSpecText(const DataTypeSpec& t); // "VARCHAR(50)"

}  // namespace ast
}  // namespace sqlint

#endif  // SQLINT_AST_NODES_H
