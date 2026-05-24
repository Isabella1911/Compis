# Compis IDE — interfaz simple para el pipeline YALex + YAPar + LL(1)

IDE minimalista en `tkinter` que envuelve el ejecutable `compilador`
generado por `Main.cpp`. No reescribe el compilador: solo permite cargar,
editar, guardar, compilar y ejecutar los tres archivos del flujo actual y
muestra la salida en paneles separados.

## Requisitos

- **Python 3.8 o superior** con `tkinter` (incluido en el instalador
  estándar de Windows y en la mayoría de distribuciones Linux).
- **g++** en el PATH si se quiere usar el botón *Compilar* desde la IDE
  (en Windows: MSYS2 UCRT64, p. ej.
  `$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"`).
- El ejecutable `compilador.exe` (Windows) o `compilador` (Linux/macOS)
  en la **raíz del proyecto**. Si no existe, usar el botón *Compilar*.

## Cómo ejecutar

Desde la raíz del proyecto:

```powershell
python ide/ide_app.py
```

La IDE abre con tres editores vacíos. El estado y la ruta esperada del
ejecutable aparecen en la barra inferior.

## Botones de la barra superior

| Botón | Acción |
|-------|--------|
| Cargar YALex      | Abre un `.yal` o `.yalex` y lo muestra en el primer editor. |
| Cargar YAPar      | Abre un `.yapar` y lo muestra en el segundo editor. |
| Cargar entrada    | Abre el archivo de entrada y lo muestra en el tercer editor. |
| Guardar YALex / YAPar / entrada | Guarda el editor correspondiente; si no tiene ruta, pide *Guardar como*. |
Ejemplo| Guardar todo      | Guarda los tres editores. |
| Compilar          | Ejecuta el `g++` con los fuentes actuales del proyecto y produce `compilador.exe` en la raíz. |
| Ejecutar análisis | Llama al ejecutable con las rutas de los tres archivos cargados. |
| Limpiar salida    | Vacía los paneles de resultados. |

Los editores usan **texto plano** con fuente monoespaciada
(Consolas / DejaVu Sans Mono). No hay resaltado de sintaxis ni colores
por palabra.

## Paneles de resultados

Pestañas inferiores:

- **Salida completa**: stdout y stderr íntegros del compilador.
- **Tokens**: secciones de análisis léxico y filtrado de la salida.
- **YAPar / Validaciones**: lectura del `.yapar`, validación de tokens
  entre YALex y YAPar, gramática y tabla de símbolos.
- **FIRST / FOLLOW**: bloques `FASE 4 — FIRST` y `FASE 4 — FOLLOW`.
- **Tabla LL(1)**: bloque `FASE 4 — Tabla LL(1)` (con conflictos si los
  hay).
- **Traza Parser**: pasos de la fase 5 hasta la decisión final.
- **Resultado**: bloque `Resumen` y veredicto (ACEPTADO / RECHAZADO /
  SIN VEREDICTO) más el código de salida del proceso.
- **Errores**: líneas con `ERROR`, `ADVERTENCIA`, `CONFLICTO`,
  `RECHAZADO`, errores léxicos o sintácticos, e incidencias de la propia
  IDE (archivos no encontrados, fallos de compilación, etc.).

Las secciones se detectan a partir de los encabezados que ya imprime
`Main.cpp` con la función `separador()`. Si la salida cambia, las
pestañas pueden quedar vacías; la pestaña **Salida completa** siempre
contiene todo lo emitido.

## Flujo 

1. Iniciar la IDE.
2. Pulsar *Cargar YALex* y elegir, por ejemplo,
   `input/lexer_complejo.yal`.
3. Pulsar *Cargar YAPar* y elegir `input/parser_complejo.yapar`.
4. Pulsar *Cargar entrada* y elegir `input/entrada_complejo.txt` o
   `input/Test_complejo.txt`.
5. Editar los archivos si hace falta.
6. *Guardar todo*.
7. Si todavía no existe `compilador.exe`, pulsar *Compilar*.
8. Pulsar *Ejecutar análisis* y revisar las pestañas inferiores.

