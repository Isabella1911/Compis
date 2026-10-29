#include "scope.h"

namespace sqlint {
namespace semantic {

using storage::sameName;

Resolution ScopeStack::resolve(const ast::Ident& qualifier, const ast::Ident& column) const {
    Resolution res;
    const int levels = static_cast<int>(scopes_.size());

    if (!qualifier.empty()) {
        // tabla.col: la primera fuente con ese nombre visible, de adentro hacia afuera.
        for (int depth = 0; depth < levels; ++depth) {
            const auto& sources = scopes_[levels - 1 - depth];
            for (size_t s = 0; s < sources.size(); ++s) {
                if (!sameName(sources[s].visibleName(), qualifier.name)) continue;
                res.sources.push_back(&sources[s]);
                int col = sources[s].def->findColumn(column.name);
                if (col < 0) {
                    res.result = LookupResult::NotFound;
                    return res;
                }
                res.result = LookupResult::Found;
                res.binding = ast::ColumnBinding{depth, static_cast<int>(s), col, -1};
                res.column = &sources[s].def->columns[col];
                return res;
            }
        }
        res.result = LookupResult::UnknownQualifier;
        return res;
    }

    // col sin calificar: en cada nivel puede aparecer en una sola fuente; si
    // aparece en dos o mas del mismo nivel, es ambigua.
    for (int depth = 0; depth < levels; ++depth) {
        const auto& sources = scopes_[levels - 1 - depth];
        std::vector<std::pair<size_t, int>> matches;
        for (size_t s = 0; s < sources.size(); ++s) {
            int col = sources[s].def->findColumn(column.name);
            if (col >= 0) matches.emplace_back(s, col);
        }
        if (matches.size() > 1) {
            res.result = LookupResult::Ambiguous;
            for (const auto& m : matches) res.sources.push_back(&sources[m.first]);
            return res;
        }
        if (matches.size() == 1) {
            auto [s, col] = matches[0];
            res.result = LookupResult::Found;
            res.binding = ast::ColumnBinding{depth, static_cast<int>(s), col, -1};
            res.column = &sources[s].def->columns[col];
            return res;
        }
    }
    res.result = LookupResult::NotFound;
    if (levels > 0) {
        for (const auto& source : scopes_.back()) res.sources.push_back(&source);
    }
    return res;
}

int ScopeStack::findSource(const std::string& visibleName) const {
    if (scopes_.empty()) return -1;
    const auto& sources = scopes_.back();
    for (size_t s = 0; s < sources.size(); ++s) {
        if (sameName(sources[s].visibleName(), visibleName)) return static_cast<int>(s);
    }
    return -1;
}

}  // namespace semantic
}  // namespace sqlint
