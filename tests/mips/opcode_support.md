# Inventario de opcodes TAC → soporte MIPS (Fase 3 / doc 01)

Generado y mantenido junto al backend. Estados:

- **ok**: traducido y cubierto por `tests/mips/`
- **parcial**: traducido con limitaciones (ver notas)
- **pendiente**: produce `CG001`/`CG004` (no hay `.asm` exitoso)

| Opcode TAC | Operandos | Lowering MIPS | Estado | Prueba |
|---|---|---|---|---|
| `Copy` | dst, a | load/store / `li` / `la` | ok | `valid/aritmetica.cps` |
| `Binary` `+ - * / %` | dst, a, b | `addu/subu/mul/div` | ok | `valid/aritmetica.cps` |
| `Binary` `== != < <= > >=` | dst, a, b | `seq/sne/slt/...` | ok | `valid/if_else.cps` |
| `Unary` `-` `!` | dst, a | `negu` / `seq $t0,$zero` | ok | `valid/aritmetica.cps` |
| `Label` | label | `label:` | ok | `valid/if_else.cps` |
| `Goto` | label | `j` | ok | `valid/if_else.cps` |
| `IfGoto` / `IfFalseGoto` | a[,b], label | `bnez/beqz` o branches | ok | `valid/while_and.cps` |
| `Param` | a | push word | ok | `valid/strings.cps` |
| `Link` | a | push static link | parcial | (closures: doc 02) |
| `Call` directa | label, argc | `jal` + cleanup | parcial | builtins + funciones simples |
| `Call` indirecta | a, argc | — | pendiente | CG004 |
| `Return` | [a] | `$v0` + epílogo / exit | ok | (funciones) |
| `Print` int/str/bool | a | syscall 1/4 + NL | ok | varios `valid/` |
| `Load` `dst = a[b]` | dst, base, offset | `lw` (offset const o en registro); base 0 → `__null_ref` | ok | `valid/listas.cps` |
| `Store` `dst[a] = b` | base, offset, valor | `sw`; base 0 → `__null_ref` | ok | `valid/listas.cps` |
| `Alloc` | dst, bytes | `syscall 9` (sbrk), tamaño redondeado a múltiplo de 4 | ok | `valid/lista_vacia_y_objetos_heap.cps` |
| `TryBegin` / `TryEnd` | — | — | pendiente | `invalid_backend/try_catch.cps` |
| `Comment` | text | `#` | ok | — |

Runtime emitido: `__concat`, `__str_eq`, `__raise`, `__div_zero`, `__null_ref`,
`__runtime_error` y `__fail`. Todos los errores de runtime terminan en `__fail`:
imprime `Error en tiempo de ejecucion: <mensaje>` y sale con código 1
(syscall 17). Ver `valid/indice_fuera_de_rango.cps`.

Desplazamientos que no caben en 16 bits (`lw`/`sw`) se arman en `$t9`.
