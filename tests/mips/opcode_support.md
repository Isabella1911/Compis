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
| `Param` | a | push inmediato (no al llegar al `call`: el TAC reutiliza el temporal) | ok | `valid/funciones.cps` |
| `Link` | a | push del static link, justo antes del `call` | ok | `tests/tac/p2_closure.cps` |
| `Call` directa | label, argc | `jal`; el llamador limpia `4*argc + 4` | ok | `valid/funciones.cps` |
| `Call` indirecta | a, argc | `jalr` con la dirección sacada de la vtable | ok | `valid/herencia_polimorfismo.cps` |
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

## Convención de llamada y verificación de pila

```
llamador:  push arg_n ... push arg_1   (cada param se apila al aparecer)
           push link (0 si no hay)
           jal f / jalr $t3
           addiu $sp, $sp, 4*argc + 4
llamado:   fp+8+4i arg i | fp+4 link | fp+0 fp viejo | fp-4 $ra
           fp-8-4k local k | debajo, temporales
epílogo:   move $sp,$fp; lw $ra,-4($fp); lw $fp,0($fp); addiu $sp,$sp,4; jr $ra
```

Al terminar, `main` compara `$sp` con el valor guardado tras su prólogo
(`__sp_main`); si no coincide, termina con
`Error en tiempo de ejecucion: pila desbalanceada al terminar main`. Así cada
prueba ejecutada en MARS verifica que todas las llamadas dejaron la pila
balanceada. Esta verificación detectó que el epílogo original descartaba
una palabra de más (`addiu $sp, $sp, 8`).
