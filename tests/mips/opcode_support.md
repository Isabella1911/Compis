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
| `Load` / `Store` | — | — | pendiente | CG004 (objetos/listas) |
| `Alloc` | — | — | pendiente | CG004 |
| `TryBegin` / `TryEnd` | — | — | pendiente | `invalid_backend/try_catch.cps` |
| `Comment` | text | `#` | ok | — |

Runtime emitido: `__concat`, `__str_eq`, `__div_zero`, `__runtime_error`.
