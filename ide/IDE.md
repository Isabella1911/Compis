# IDE de Compiscript

Interfaz gráfica (Python + Tkinter) sobre el binario `compiscript`. No
reimplementa el compilador: lo invoca como subproceso y reparte su salida
en pestañas.

## Requisitos

| Componente | Uso |
|------------|-----|
| Python 3.8+ con Tkinter | Ventana principal |
| Binario `compiscript` | Análisis (tras `make` o el botón Compilar) |
| Graphviz (`dot`) | Opcional: PNG del AST |
| `tkinterdnd2` | Opcional: arrastrar `.cps` a la ventana |

```bash
bash tools/setup.sh   # primera vez
make                  # genera/compila el back
python3 ide/ide_app.py
```

## Uso

1. Abrí o escribí un `.cps` en el editor (también se puede cargar un
   fixture desde el panel Proyecto).
2. **Compilar** (`F5`) ejecuta `make` y construye el binario.
3. **Ejecutar análisis** (`F6`) guarda el archivo activo y corre
   `./compiscript <archivo.cps>`.
4. Revisá el resultado en las pestañas inferiores:
   - **Diagnósticos** — código, severidad, línea, columna, mensaje.
     Doble clic salta a la posición en el editor; las líneas con error
     quedan resaltadas (fondo y número en el gutter).
   - **AST / Tabla de símbolos** — texto indentado del CLI.
   - **AST (gráfico)** — renderiza `output/ast.dot` con Graphviz.

## Relación con el compilador

La IDE solo consume el formato de stdout de `src/main.cpp`:

```text
Diagnosticos:
  [SEM001] error 3:5: ...
AST:
  ...
Tabla de simbolos:
  ...
[AST-DOT] Exportado a: output/ast.dot
```

Cualquier cambio interno de pases semánticos no rompe la IDE mientras
esa fachada se mantenga.
