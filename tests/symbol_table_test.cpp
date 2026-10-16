// Prueba unitaria standalone de Scope/Symbol/SymbolTable, sin pasar por el
// AST ni por el compilador: verifica la estructura de datos sola antes de
// conectarla a nada (progresion incremental de la Etapa 2).

#include <cassert>
#include <iostream>

#include "semantic/scope.h"
#include "semantic/symbol_table.h"

using namespace compiscript::semantic;
using compiscript::ast::LiteralKind;

static int checks = 0;
static int failures = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        checks++;                                                       \
        if (!(cond)) {                                                  \
            failures++;                                                 \
            std::cerr << "FALLO linea " << __LINE__ << ": " << #cond << "\n"; \
        }                                                                \
    } while (0)

static SymbolPtr makeVar(const std::string& name, int line = 0) {
    auto s = std::make_shared<Symbol>();
    s->name = name;
    s->kind = SymbolKind::Variable;
    s->declared_line = line;
    return s;
}

int main() {
    // 1. Declarar y resolver en el mismo scope (global).
    {
        SymbolTable table;
        Scope* global = table.global();
        CHECK(global->kind() == ScopeKind::Global);
        CHECK(global->parent() == nullptr);

        auto conflict = global->declare(makeVar("x", 1));
        CHECK(conflict == nullptr);  // primera declaracion: sin conflicto

        auto found = global->resolve("x");
        CHECK(found != nullptr);
        CHECK(found->name == "x");
    }

    // 2. Redeclaracion en el MISMO scope: debe reportar el simbolo existente.
    {
        SymbolTable table;
        Scope* global = table.global();
        global->declare(makeVar("x", 1));
        auto conflict = global->declare(makeVar("x", 5));
        CHECK(conflict != nullptr);
        CHECK(conflict->declared_line == 1);  // el ORIGINAL, no el nuevo
        // La redeclaracion no debe haber reemplazado el simbolo:
        CHECK(global->resolve("x")->declared_line == 1);
    }

    // 3. Shadowing: mismo nombre en un scope hijo es VALIDO (no conflicto).
    {
        SymbolTable table;
        Scope* global = table.global();
        global->declare(makeVar("x", 1));

        Scope* funcScope = global->createChild(ScopeKind::Function);
        auto conflict = funcScope->declare(makeVar("x", 10));
        CHECK(conflict == nullptr);  // shadowing, no es redeclaracion

        // Desde dentro del scope hijo, resolve() debe encontrar la version
        // MAS CERCANA (la del propio scope), no la del global.
        CHECK(funcScope->resolve("x")->declared_line == 10);
        // El global sigue intacto.
        CHECK(global->resolve("x")->declared_line == 1);
    }

    // 4. Resolucion de nombres subiendo por varios niveles de ambito.
    {
        SymbolTable table;
        Scope* global = table.global();
        global->declare(makeVar("g", 1));

        Scope* funcScope = global->createChild(ScopeKind::Function);
        funcScope->declare(makeVar("param", 2));

        Scope* blockScope = funcScope->createChild(ScopeKind::Block);
        blockScope->declare(makeVar("local", 3));

        // Desde el bloque mas interno se debe poder resolver los tres,
        // aunque "g" y "param" no esten en su propio scope.
        CHECK(blockScope->resolve("local") != nullptr);
        CHECK(blockScope->resolve("param") != nullptr);
        CHECK(blockScope->resolve("g") != nullptr);

        // resolveLocal NO debe subir: "g" no esta directamente en blockScope.
        CHECK(blockScope->resolveLocal("local") != nullptr);
        CHECK(blockScope->resolveLocal("g") == nullptr);

        // Variable inexistente: ni local ni resolviendo hacia arriba.
        CHECK(blockScope->resolve("no_existe") == nullptr);
    }

    // 5. FunctionSymbol y ClassSymbol cargan sus campos extra.
    {
        SymbolTable table;
        Scope* global = table.global();

        auto fn = std::make_shared<FunctionSymbol>();
        fn->name = "sumar";
        fn->kind = SymbolKind::Function;
        fn->params.push_back({"a", nullptr, 1, 1});
        fn->params.push_back({"b", nullptr, 1, 1});
        global->declare(fn);

        auto resolved = global->resolve("sumar");
        CHECK(resolved != nullptr);
        auto fnResolved = std::dynamic_pointer_cast<FunctionSymbol>(resolved);
        CHECK(fnResolved != nullptr);
        CHECK(fnResolved->params.size() == 2);

        auto cls = std::make_shared<ClassSymbol>();
        cls->name = "Perro";
        cls->kind = SymbolKind::Class;
        cls->base_class_name = "Animal";
        global->declare(cls);

        auto clsResolved = std::dynamic_pointer_cast<ClassSymbol>(global->resolve("Perro"));
        CHECK(clsResolved != nullptr);
        CHECK(clsResolved->base_class_name.has_value());
        CHECK(*clsResolved->base_class_name == "Animal");
    }

    // 6. is_mutable distingue variable (let/var) de constante (const).
    {
        SymbolTable table;
        Scope* global = table.global();
        auto constant = makeVar("PI", 1);
        constant->kind = SymbolKind::Constant;
        constant->is_mutable = false;
        global->declare(constant);
        CHECK(global->resolve("PI")->is_mutable == false);
    }

    std::cout << checks << " checks, " << failures << " fallos\n";
    return failures == 0 ? 0 : 1;
}
