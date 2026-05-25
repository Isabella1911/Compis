# Compis

Compilador didáctico (UVG, Diseño de Lenguajes) que implementa un pipeline
completo desde el archivo de especificación léxica hasta el parseo
sintáctico, comparando varias técnicas clásicas en una sola corrida:

```
YALex  -> AFD -> lexer
YAPar  -> gramática -> FIRST / FOLLOW
                                LL(1)   -> tabla + traza
                                LR(0)   -> autómata canónico (.dot)
                                SLR(1)  -> ACTION / GOTO + evaluación
                                LALR(1) -> lookaheads refinados + evaluación
```

El back está en C++17 y se invoca como un solo ejecutable
(`compilador.exe`). La carpeta `ide/` contiene una IDE en `tkinter` que
envuelve ese binario y muestra cada subproducto del compilador en una
pestaña separada.

---

## 1. Requisitos

| Componente | Para qué |
|------------|----------|
| **g++** con C++17 | Compilar el back. En Windows funciona MSYS2 UCRT64. |
| **Python 3.8+** con `tkinter` | Ejecutar la IDE. |
| **Graphviz (`dot`)** *(opcional)* | Renderizar el autómata LR(0) embebido en la IDE. |
| **`tkinterdnd2`** *(opcional)* | Arrastrar archivos sobre la IDE. Instalar con `python -m pip install tkinterdnd2`. |

En Windows / PowerShell, agregar MSYS2 al PATH de la sesión:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
```

---

## 2. Compilar el back

Desde la raíz del repo:

```bash
g++ -std=c++17 -O2 -DCOMPILAR_CON_ORQUESTADOR \
    Main.cpp \
    Lexer/YalexParser.cpp \
    Parser/YaparParser.cpp Parser/Grammar.cpp \
    Parser/FirstFollow.cpp Parser/LL1Table.cpp \
    Parser/LR0.cpp Parser/SLR1.cpp Parser/LALR1.cpp \
    -o compilador.exe
```

En Linux / macOS, omitir el `.exe` final.

---

## 3. Ejecutar el back (línea de comandos)

Modo interactivo (muestra un menú con los conjuntos de archivos
preconfigurados en `input/`):

```bash
./compilador.exe
```

Modo directo (rutas explícitas):

```bash
./compilador.exe input/ejemplo.yal input/parser_ejemplo.yapar input/entrada_ejemplo.txt
./compilador.exe input/lexer_complejo.yal input/parser_complejo.yapar input/entrada_complejo.txt
./compilador.exe input/lexer_complejo.yal input/parser_complejo.yapar input/Test_complejo.txt
```

El proceso retorna `0` cuando al menos uno de los parsers acepta y `3`
cuando todos rechazan; lee el resumen final para ver el veredicto por
parser.

---

## 4. Salidas

Cada corrida deja artefactos en `output/`:

- `output/lr0.dot` — autómata LR(0) en formato Graphviz (puede
  renderizarse con `dot -Tpng output/lr0.dot -o output/lr0.png`).
- `output/tablas.json` — FIRST, FOLLOW, tabla LL(1), tabla SLR(1),
  tabla LALR(1) y la lista de conflictos por parser, listos para
  consumir desde la IDE (o exportar a CSV).

La IDE consume ambos archivos automáticamente.

---

## 5. IDE

Desde la raíz del repo:

```bash
python ide/ide_app.py
```

La interfaz se documenta con detalle en
[`ide/IDE.md`](ide/IDE.md). En resumen:

- Tres editores para el `.yal`, el `.yapar` y la entrada.
- Botones para compilar y ejecutar el back, renderizar el autómata
  LR(0) con Graphviz y exportar las tablas a JSON o CSV.
- Pestañas dedicadas por fase del compilador (Tokens, FIRST/FOLLOW,
  LL(1), LR(0), SLR(1), LALR(1), trazas, resultado, errores).
- Resaltado de conflictos *shift/reduce* y *reduce/reduce* y un
  veredicto separado por cada parser en la pestaña *Resultado*.

---

## 6. Estructura del repositorio

```
Compis/
├── Main.cpp                     # Orquestador: 8 fases del pipeline
├── Lexer/
│   └── YalexParser.cpp/.h       # YALex -> AFN -> AFD -> AFD minimizado -> lexer
├── Parser/
│   ├── YaparParser.cpp/.h       # Lectura del .yapar
│   ├── Grammar.cpp/.h           # Estructuras de gramática + validaciones
│   ├── FirstFollow.cpp/.h       # FIRST y FOLLOW
│   ├── LL1Table.cpp/.h          # Tabla LL(1) y detección de conflictos
│   ├── LR0.cpp/.h               # Autómata LR(0) + exportación a .dot
│   ├── SLR1.cpp/.h              # Tabla SLR(1) y evaluador
│   └── LALR1.cpp/.h             # LALR(1) con `esLALR1()` propio
├── ide/
│   ├── ide_app.py               # IDE tkinter (envuelve el ejecutable)
│   └── IDE.md                   # Documentación detallada de la IDE
├── input/                       # Casos de prueba (.yal, .yapar, .txt)
├── output/                      # Artefactos generados (lr0.dot, tablas.json, ...)
└── README.md
```

---

## 7. Notas

- El binario se ejecuta desde la raíz del repo para que `input/...` y
  `output/...` resuelvan igual que en la IDE.
- Si una corrida falla por encoding (caracteres no ASCII en la
  consola), forzar UTF-8: `chcp 65001` y `PYTHONIOENCODING=utf-8` antes
  de invocar Python desde PowerShell.
- Los binarios (`compilador.exe`, `output/`, `.o`, `__pycache__/`,
  generadores intermedios) están en `.gitignore`.
