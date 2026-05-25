# Compis IDE — interfaz para el pipeline YALex + YAPar + LL(1) + LR(0) + SLR(1) + LALR(1)

IDE minimalista en `tkinter` que envuelve el ejecutable `compilador`
generado a partir de `Main.cpp`. **No reescribe el compilador**: carga,
edita, guarda los tres archivos del flujo, invoca el binario y reparte
su salida (`stdout` / `stderr` y artefactos en `output/`) en pestañas
separadas por fase del pipeline.

## Requisitos

- **Python 3.8 o superior** con `tkinter` (incluido en el instalador
  estándar de Windows y en la mayoría de distribuciones Linux).
- **g++** en el `PATH` si se quiere usar el botón *Compilar* desde la IDE
  (en Windows: MSYS2 UCRT64, p. ej.
  `$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"`).
- **Graphviz (`dot`)** opcional, sólo si querés renderizar el autómata
  LR(0) embebido como PNG dentro de la IDE.
- **`tkinterdnd2`** opcional, sólo si querés arrastrar archivos sobre la
  ventana para importarlos. Instalalo con
  `python -m pip install tkinterdnd2` (en Windows + MSYS2:
  `C:/msys64/ucrt64/bin/python.exe -m pip install tkinterdnd2`).
- El ejecutable `compilador.exe` (Windows) o `compilador` (Linux/macOS)
  en la **raíz del proyecto**. Si no existe, usar el botón *Compilar*.

## Cómo ejecutar

Desde la raíz del proyecto:

```powershell
python ide/ide_app.py
```

La IDE abre con tres editores vacíos. El estado y la ruta esperada del
ejecutable aparecen en la barra inferior.

## Barra superior

| Botón | Acción |
|-------|--------|
| Cargar YALex      | Abre un `.yal` o `.yalex` y lo muestra en el primer editor. |
| Cargar YAPar      | Abre un `.yapar` y lo muestra en el segundo editor. |
| Cargar entrada    | Abre el archivo de entrada y lo muestra en el tercer editor. |
| Guardar YALex / YAPar / entrada | Guarda el editor correspondiente; si no tiene ruta, pide *Guardar como*. |
| Guardar todo      | Guarda los tres editores. |
| Compilar          | Ejecuta `g++` con los fuentes actuales del proyecto y produce `compilador.exe` en la raíz. |
| Ejecutar análisis | Llama al ejecutable con las rutas de los tres archivos cargados. |
| Renderizar LR(0)  | Invoca `dot` sobre `output/lr0.dot` y muestra el PNG embebido en la pestaña *Autómata LR(0)*. |
| Exportar tablas   | Convierte `output/tablas.json` a un archivo JSON único o a varios CSV (FIRST/FOLLOW/LL1/SLR1/LALR1/conflictos). |
| Limpiar salida    | Vacía todos los paneles de resultados. |

Los editores usan fuente monoespaciada (JetBrains Mono / Cascadia /
Consolas si están disponibles, con fallback automático). Cada editor
tiene **resaltado de sintaxis básico** según su tipo:

| Editor | Lenguaje | Reglas resaltadas |
|--------|----------|-------------------|
| `lexer.yal`     | YALex   | `let` / `rule` / `eof` (azul **bold**), strings y caracteres (naranja), comentarios `(* ... *)` (verde itálica), operadores `\| * + ? ( ) [ ] =` (amarillo). |
| `parser.yapar`  | YAPar   | Directivas `%token` / `%ignore` / `%start` / `%left` / `%right` / `%nonassoc` / `%prec` y `IGNORE` (magenta **bold**), separador `%%` (amarillo **bold**), comentarios `/* */` y `//` (verde itálica), **TERMINALES** en MAYÚSCULA (turquesa), no-terminales lhs de producción (celeste **bold**), operadores `: ; \|` (amarillo). |
| `input.txt`     | Texto plano | Sin resaltado (es la entrada del lenguaje, no el lenguaje de descripción). |

El resaltado se aplica con un debounce de 150 ms al escribir, y de
forma inmediata al cargar un archivo.

### Detalles interactivos del editor

- **Línea actual resaltada**: la línea donde está el cursor se pinta
  con un fondo levemente más claro.
- **Match de brackets / paréntesis**: cuando el cursor está sobre un
  `(`, `[`, `{` (o su pareja), ambos se resaltan en azul.
- **Indicador de archivo modificado**: si modificás un editor sin
  guardar, su pestaña pasa de `λ  lexer.yal` a `λ  lexer.yal  •`.
  Al guardar (`Ctrl+S`) el bullet desaparece.
- **Iconos sutiles**: `λ` para YALex, `Σ` para YAPar, `≡` para la
  entrada; aparecen en las pestañas y en el panel *Proyecto*.

### Spinner de progreso

Durante *Compilar* y *Ejecutar análisis* la toolbar muestra una barra
de progreso indeterminada (`ttk.Progressbar`) a la derecha del estado.
Se oculta automáticamente cuando el proceso termina.

### Drop indicator

Mientras estás arrastrando archivos sobre la IDE, la toolbar cambia su
color a azul intenso (`Drop.TFrame`) y la leyenda lo anuncia:
`⤓  Soltá los archivos para importarlos…`. Al soltar o salir del área
con el cursor, la toolbar vuelve al estado normal.

### Empty states

Cada pestaña inferior tiene un mensaje guía cuando todavía no hay
datos (en lugar de un `(vacio)` plano). Ejemplos:

- *Consola*: `▷  Ejecutá Compilar (F5) o Ejecutar análisis (F6)…`
- *Tokens*: `▷  Tokens emitidos por el lexer…`
- *Errores*: `✓  Sin errores.`

### Persistencia entre sesiones

Al cerrar la IDE se guarda un archivo `.ide_state.json` en la raíz del
proyecto con:

- Geometría de la ventana.
- Rutas de los tres archivos abiertos.
- Tab activa en el grupo de editores.
- Grupo de resultados activo (Resultados / Parsing / Visualización).

La próxima vez que abras la IDE, las rutas se recargan automáticamente
si los archivos siguen existiendo, y el layout se restaura. El archivo
está en `.gitignore` para que no se commitee.

## Pestañas de resultados

| Pestaña | Contenido |
|---------|-----------|
| Salida completa     | `stdout` + `stderr` íntegros del compilador. |
| Tokens              | Tokens emitidos por el lexer y filtrado por IGNORE. |
| YAPar / Validaciones| Lectura del `.yapar`, validación cruzada con YALex, gramática y tabla de símbolos. |
| FIRST / FOLLOW      | Bloques `FASE 4 — FIRST` y `FASE 4 — FOLLOW`. |
| Tabla LL(1)         | Tabla LL(1) y conflictos (resaltados si los hay). |
| LR(0)               | Autómata LR(0) impreso (gramática aumentada, conjuntos canónicos y GOTOs). |
| SLR(1)              | Tabla ACTION/GOTO SLR(1), evaluación paso a paso y conflictos resaltados. |
| LALR(1)             | Tabla ACTION/GOTO LALR(1), evaluación paso a paso y conflictos resaltados. |
| Traza LL(1)         | Pasos del parser LL(1) (Fase 5). |
| Autómata LR(0)      | Vista Graphviz del autómata: render PNG embebido + sub-pestaña con el fuente `.dot`. Botones para renderizar y abrir externo. |
| Parser paso a paso  | Combobox para elegir parser (LL(1) / SLR(1) / LALR(1)) y ver su traza completa. |
| Resultado           | Tabla compacta con un veredicto por parser (LL/SLR/LALR), seguido del bloque `Resumen` que imprime el back. |
| Errores             | Líneas con `ERROR`, `ADVERTENCIA`, `CONFLICTO`, `shift/reduce`, `reduce/reduce`, `RECHAZADO`, errores léxicos o sintácticos, e incidencias de la propia IDE (archivos faltantes, fallos de compilación, etc.). |

La detección de secciones se basa en los encabezados que ya imprime
`Main.cpp` con la función `separador()`. Si la salida cambia, las
pestañas pueden quedar vacías; la pestaña **Salida completa** siempre
contiene todo lo emitido.

### Resaltado de conflictos

En las pestañas *Tabla LL(1)*, *SLR(1)*, *LALR(1)* y *Errores*, las
líneas que contienen `shift/reduce` se resaltan en amarillo y las
`reduce/reduce` en rojo. La pestaña *Resultado* resalta los veredictos:
`ACEPTADO` en verde, `RECHAZADO` en rojo.

### Autómata LR(0) (Graphviz)

Cada vez que el back corre, exporta `output/lr0.dot`. La IDE lo carga
automáticamente en la pestaña *Autómata LR(0)*. Si Graphviz (`dot`)
está disponible en el `PATH`:

1. Pulsar *Renderizar PNG* (o el botón equivalente de la pestaña).
2. La IDE genera `output/lr0.png` y lo muestra embebido en un canvas
   con scrollbars.

Si `dot` no está instalado, la sub-pestaña *Fuente .dot* sigue
mostrando el grafo en texto y *Abrir .dot externo* / *Abrir PNG
externo* delegan al visor del sistema.

### Exportación de tablas

`output/tablas.json` se genera en cada corrida con la siguiente
estructura:

```json
{
  "first":  { "<NT>": ["<terminal>", ...] },
  "follow": { "<NT>": ["<terminal>", ...] },
  "ll1":    {
    "terminales": [...],
    "filas":      { "<NT>": { "<terminal>": <indice_produccion> } }
  },
  "slr1":   { "terminales": [...], "no_terminales": [...],
              "action": [{...}, ...], "goto": [{...}, ...],
              "conflictos": [...] },
  "lalr1":  { ... misma estructura que slr1 ... }
}
```

Desde *Exportar tablas...* podés:

- Guardar el JSON tal cual (un sólo archivo).
- Elegir extensión `.csv`: la IDE crea varios CSV en la carpeta de
  destino (FIRST, FOLLOW, LL1, SLR1 ACTION/GOTO, LALR1 ACTION/GOTO y
  los conflictos cuando existen).

## Drag & drop de archivos

Si `tkinterdnd2` está instalado, podés arrastrar uno o varios archivos
desde el Explorador / Finder / Nautilus y soltarlos sobre cualquier
parte de la ventana (la ventana, el panel *Proyecto*, las pestañas de
editor o el área de texto). La IDE los clasifica automáticamente por
extensión:

| Extensión                       | Editor destino |
|---------------------------------|----------------|
| `.yal`, `.yalex`                | YALex          |
| `.yapar`, `.yalp`, `.grammar`   | YAPar          |
| Cualquier otra (`.txt`, ...)    | Entrada        |

Si se sueltan los tres archivos a la vez, cada uno va a su editor
correspondiente y el foco queda en el último importado. Si la
dependencia opcional no está instalada, la barra de estado lo indica
con un mensaje sugerente; la IDE sigue funcionando con los diálogos
*Archivo → Cargar...*.

### Soporte para WSL

Si corrés la IDE dentro de WSL (con WSLg en Windows 11 o un X server
configurado), las rutas que llegan desde un drop pueden tener dos
formatos:

| Formato recibido | Origen | Conversión que aplica la IDE |
|------------------|--------|------------------------------|
| `C:\Users\foo\x.yal` | Explorador de Windows → WSL | `/mnt/c/Users/foo/x.yal` |
| `D:/proyecto/foo.txt` | (forward slash) | `/mnt/d/proyecto/foo.txt` |
| `\\wsl.localhost\Ubuntu\home\u\f.yal` | Vista UNC desde Windows | `/home/u/f.yal` |
| `/home/usuario/file.yal` | Filesystem de WSL | sin cambios |
| `/mnt/c/...` | WSL leyendo Windows | sin cambios |

La conversión usa el utilitario nativo `wslpath -u` cuando está
disponible (lo está por default en cualquier distro WSL moderna); si
no, cae a una conversión manual probada para `C:\...` y rutas UNC de
`\\wsl.localhost\...` / `\\wsl$\...`.

La detección de WSL se hace al inicio leyendo `WSL_DISTRO_NAME` /
`WSL_INTEROP` o, como fallback, buscando "microsoft" en
`/proc/version`.

## Flujo recomendado

1. Iniciar la IDE.
2. Cargar los tres archivos:
   - opción A: *Archivo → Cargar YALex / YAPar / Entrada*;
   - opción B: arrastrar los tres archivos sobre la ventana.
3. Como ejemplo: `input/lexer_complejo.yal`,
   `input/parser_complejo.yapar` y `input/Test_complejo.txt`.
4. Editar los archivos si hace falta.
5. *Archivo → Guardar todo* (o `Ctrl+Shift+S`).
6. Si todavía no existe `compilador.exe`, *Compilar* (`F5`).
7. *Ejecutar análisis* (`F6`) y revisar las pestañas inferiores;
   los veredictos por parser quedan en *Resultado*.
8. (Opcional) *Visualizar → Renderizar autómata LR(0)* y/o
   *Herramientas → Exportar tablas*.

## Detalles internos

- La compilación y la ejecución se hacen en hilos aparte para que la
  ventana no se congele; la barra de estado indica cuándo termina cada
  proceso.
- Las rutas son relativas a la raíz del proyecto; tanto la compilación
  como la ejecución usan esa raíz como directorio de trabajo, de modo
  que `Main.cpp` puede crear su carpeta `output/` y resolver rutas
  `input/...` igual que desde la línea de comandos.
- La IDE conserva el `stdout` de la última ejecución para que la
  pestaña *Parser paso a paso* y los botones de exportación funcionen
  sin tener que reejecutar el binario.
