#ifndef COMPISCRIPT_CODEGEN_FRAME_LAYOUT_H
#define COMPISCRIPT_CODEGEN_FRAME_LAYOUT_H

// Pass previo a la generacion de TAC: completa la tabla de simbolos con lo
// que la generacion de codigo necesita saber de cada simbolo (donde vive,
// en que offset, con que nombre aparece en el TAC) y de cada funcion
// (etiqueta, tamaño de frame, profundidad de anidamiento).
//
// Registro de activacion (offsets relativos a fp, palabras de 4 bytes):
//
//     fp + 8 + 4*i   parametro i  ('this' es el parametro 0 en metodos)
//     fp + 4         enlace de acceso (static link)
//     fp + 0         enlace de control (fp del llamador)
//     fp - 4         direccion de retorno
//     fp - 8 - 4*k   variable local k
//
// Toda variable declarada fuera de una funcion es global estatica
// (g_<nombre>), incluso si esta dentro de un bloque del nivel superior
// (un if/while fuera de toda funcion): `main` nunca se reentra, asi que no
// necesita registro de activacion propio y su frame es 0. Las globales
// sombreadas reciben sufijo (g_x, g_x_1, ...).
//
// Dentro de una funcion, cada local tiene su propia casilla (no se reusan
// casillas entre bloques hermanos) y los nombres repetidos por sombreado
// reciben sufijo en el TAC (x, x@1, ...).
//
// Los atributos de clase (StorageKind::Field) y ClassSymbol::instance_size
// se calculan en layoutClassFields (parte 2): base primero, luego propios.

#include <set>
#include <string>
#include <vector>

#include "ast/nodes.h"
#include "semantic/symbol.h"
#include "semantic/symbol_table.h"

namespace compiscript {
namespace codegen {

class FrameLayout {
public:
    void run(ast::Program& program, semantic::SymbolTable& table);

    // Bytes de locales del frame de `main`. Siempre 0 (ver arriba); se
    // deja como dato para que la fase de MIPS no tenga que suponerlo.
    int mainFrameSize() const { return 0; }

    // Variables globales en orden de declaracion en el codigo fuente.
    const std::vector<semantic::Symbol*>& globals() const { return globals_; }

private:
    // Lo que se va acumulando mientras se recorre el cuerpo de UNA funcion
    // (o el nivel superior, con fn == nullptr).
    struct FrameState {
        semantic::FunctionSymbol* fn = nullptr;
        int locals = 0;
        std::set<std::string> used_names;
    };

    void walkStatements(const std::vector<ast::StatementPtr>& statements, FrameState& state);
    void walkStatement(ast::Statement* stmt, FrameState& state);
    void placeVariable(semantic::Symbol* symbol, FrameState& state);
    void layoutFunction(ast::FunctionDeclaration* decl, semantic::FunctionSymbol* enclosing,
                        semantic::ClassSymbol* ownerClass);
    // Atributos de instancia: offsets desde el inicio del objeto, con los
    // heredados en las mismas posiciones que en la base.
    void layoutClassFields(semantic::ClassSymbol* cls);

    std::vector<semantic::Symbol*> globals_;
    FrameState top_level_;
    std::set<std::string> used_labels_;
    std::set<semantic::ClassSymbol*> laid_out_classes_;
};

}  // namespace codegen
}  // namespace compiscript

#endif  // COMPISCRIPT_CODEGEN_FRAME_LAYOUT_H
