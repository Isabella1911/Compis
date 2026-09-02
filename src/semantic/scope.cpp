#include "scope.h"

namespace compiscript {
namespace semantic {

Scope::Scope(ScopeKind kind, Scope* parent) : kind_(kind), parent_(parent) {}

SymbolPtr Scope::declare(SymbolPtr symbol) {
    auto existing = symbols_.find(symbol->name);
    if (existing != symbols_.end()) {
        return existing->second;
    }
    symbols_[symbol->name] = symbol;
    return nullptr;
}

SymbolPtr Scope::resolveLocal(const std::string& name) const {
    auto it = symbols_.find(name);
    if (it != symbols_.end()) return it->second;
    return nullptr;
}

SymbolPtr Scope::resolve(const std::string& name) const {
    for (const Scope* scope = this; scope != nullptr; scope = scope->parent_) {
        auto it = scope->symbols_.find(name);
        if (it != scope->symbols_.end()) return it->second;
    }
    return nullptr;
}

Scope* Scope::createChild(ScopeKind kind) {
    children_.push_back(std::make_unique<Scope>(kind, this));
    return children_.back().get();
}

}  // namespace semantic
}  // namespace compiscript
