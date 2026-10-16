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
// Las variables del scope global no van en ningun frame: son globales
// estaticas (g_<nombre>). Las de bloques del nivel superior (dentro de un
// if/while fuera de toda funcion) son locales del frame de `main`.
//
// Los atributos de clase (StorageKind::Field) y ClassSymbol::instance_size
// NO se calculan aqui: son de la parte 2 (gen_classes.cpp).

#include "ast/nodes.h"
#include "semantic/symbol_table.h"

namespace compiscript {
namespace codegen {

class FrameLayout {
public:
    void run(ast::Program& program, semantic::SymbolTable& table);

    // Bytes de locales del frame de `main` (codigo de nivel superior).
    int mainFrameSize() const { return main_frame_size_; }

private:
    int main_frame_size_ = 0;
};

}  // namespace codegen
}  // namespace compiscript

#endif  // COMPISCRIPT_CODEGEN_FRAME_LAYOUT_H
