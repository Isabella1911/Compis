#ifndef SQLINT_SEMANTIC_SCOPE_H
#define SQLINT_SEMANTIC_SCOPE_H

// Alcances de nombres de columnas. Cada consulta (el SELECT principal o una
// subconsulta, y tambien el UPDATE/DELETE sobre su tabla) abre un alcance con
// sus fuentes: las tablas del FROM y de los JOIN, con su alias. Las
// subconsultas apilan su alcance encima del de la consulta externa, asi una
// columna que no esta adentro se busca afuera (subconsulta correlacionada).
//
// Regla de visibilidad: si una tabla tiene alias, solo se puede calificar
// con el alias. En "FROM estudiantes e", "e.nombre" es valido y
// "estudiantes.nombre" no (SEM004), igual que en SQL estandar.

#include <string>
#include <vector>

#include "ast/nodes.h"
#include "storage/catalog.h"

namespace sqlint {
namespace semantic {

struct Source {
    std::string table;  // nombre de la tabla en el catalogo
    std::string alias;  // vacio si no tiene
    const storage::TableDef* def = nullptr;

    // Nombre con el que se califican sus columnas: el alias si lo hay.
    const std::string& visibleName() const { return alias.empty() ? table : alias; }
};

enum class LookupResult { Found, NotFound, Ambiguous, UnknownQualifier };

struct Resolution {
    LookupResult result = LookupResult::NotFound;
    ast::ColumnBinding binding;
    const storage::ColumnDef* column = nullptr;
    // Ambiguous: fuentes (nombre visible) donde aparece la columna.
    // NotFound: fuente(s) donde se busco, para el mensaje de error.
    std::vector<const Source*> sources;
};

class ScopeStack {
public:
    void push(std::vector<Source> sources) { scopes_.push_back(std::move(sources)); }
    void pop() { scopes_.pop_back(); }
    bool empty() const { return scopes_.empty(); }
    const std::vector<Source>& innermost() const { return scopes_.back(); }

    // Resuelve "column" o "qualifier.column" desde el alcance mas interno
    // hacia afuera.
    Resolution resolve(const ast::Ident& qualifier, const ast::Ident& column) const;

    // Fuente del alcance mas interno con ese nombre visible (para "t.*").
    // Devuelve -1 si no hay.
    int findSource(const std::string& visibleName) const;

private:
    std::vector<std::vector<Source>> scopes_;
};

}  // namespace semantic
}  // namespace sqlint

#endif  // SQLINT_SEMANTIC_SCOPE_H
