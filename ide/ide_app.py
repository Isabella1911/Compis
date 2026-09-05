"""IDE para el compilador Compis.

Pipeline cubierto:
    YALex -> AFD -> lexer
    YAPar -> gramática -> FIRST / FOLLOW
    LL(1)   -> tabla y traza
    LR(0)   -> autómata canónico (exportable a Graphviz .dot)
    SLR(1)  -> tabla ACTION/GOTO y evaluación
    LALR(1) -> lookaheads refinados y evaluación

La IDE NO reimplementa nada: ejecuta `compilador.exe` (compilado a partir
de `Main.cpp`) y reparte su salida en pestañas agrupadas para inspeccionar
cada subproducto del compilador (gramática, FIRST/FOLLOW, tablas, autómata
LR(0), trazas y conflictos).

El layout sigue el rediseño solicitado:
    Menú superior + toolbar compacta + panel Proyecto + editor con tabs
    + área inferior con tres grupos de pestañas (Resultados / Parsing /
    Visualización) + barra de estado enriquecida.
"""

from __future__ import annotations

import csv
import json
import os
import re
import shutil
import subprocess
import threading
import tkinter as tk
import tkinter.font as tkfont
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

# Drag & drop: dependencia opcional. Si no está, la IDE sigue funcionando
# pero sin la capacidad de soltar archivos sobre la ventana.
try:
    from tkinterdnd2 import DND_FILES, TkinterDnD  # type: ignore
    DND_DISPONIBLE = True
except ImportError:  # pragma: no cover - depende del entorno
    TkinterDnD = None  # type: ignore[assignment]
    DND_FILES = None   # type: ignore[assignment]
    DND_DISPONIBLE = False


# ──────────────────────────────────────────────────────────────────────
# Rutas y constantes
# ──────────────────────────────────────────────────────────────────────

PROJECT_ROOT      = Path(__file__).resolve().parent.parent
INPUT_DIR         = PROJECT_ROOT / "input"
OUTPUT_DIR        = PROJECT_ROOT / "output"
EJECUTABLE_NOMBRE = "compilador.exe" if os.name == "nt" else "compilador"
EJECUTABLE_PATH   = PROJECT_ROOT / EJECUTABLE_NOMBRE


def _detectar_wsl():
    """Devuelve True si la IDE corre dentro de WSL/WSL2."""
    if os.name == "nt":
        return False
    if os.environ.get("WSL_DISTRO_NAME") or os.environ.get("WSL_INTEROP"):
        return True
    try:
        with open("/proc/version", "r",
                   encoding="utf-8", errors="replace") as f:
            return "microsoft" in f.read().lower()
    except OSError:
        return False


IS_WSL = _detectar_wsl()
_PATRON_DRIVE_LETTER = re.compile(r"^[A-Za-z]:[\\/]")
_PATRON_UNC_WSL = re.compile(r"^\\\\wsl(?:\.localhost|\$)[\\/]", re.IGNORECASE)


def _candidatos_ejecutable():
    """Posibles rutas del binario del back, en orden de preferencia
    según el SO actual."""
    base = PROJECT_ROOT
    if os.name == "nt":
        return [base / "compilador.exe", base / "compilador"]
    if IS_WSL:
        # En WSL2 se pueden ejecutar .exe vía interop, pero preferimos
        # el binario Linux nativo si está. El .exe es fallback.
        return [base / "compilador", base / "compilador.exe"]
    return [base / "compilador"]


def obtener_ejecutable_real():
    """Devuelve el primer candidato existente, o el preferido si ninguno
    existe (para mostrar mensajes coherentes)."""
    for c in _candidatos_ejecutable():
        if c.exists():
            return c
    return _candidatos_ejecutable()[0]


def ruta_windows_desde_wsl(p):
    """Convierte una ruta POSIX a su equivalente Windows usando wslpath.
    Devuelve None si la conversión falla o no estamos en WSL."""
    if not IS_WSL:
        return None
    wp = shutil.which("wslpath")
    if wp is None:
        return None
    try:
        proc = subprocess.run(
            [wp, "-w", str(p)],
            capture_output=True, text=True, timeout=2)
        salida = proc.stdout.strip()
        if proc.returncode == 0 and salida:
            return salida
    except (OSError, subprocess.TimeoutExpired):
        pass
    return None


def normalizar_ruta_arrastrada(raw):
    """Limpia un path arrastrado y lo convierte al formato del SO actual.

    - Quita llaves `{}` y comillas que tkinterdnd2 puede agregar.
    - En WSL: si llega un path estilo Windows (`C:\\path\\foo`), lo traduce
      a `/mnt/c/path/foo`. Si `wslpath` está instalado se usa para máxima
      exactitud; si no, hay un fallback manual.
    - En Windows: si llega un path UNC de WSL (`\\\\wsl.localhost\\...`)
      lo deja tal cual; Windows 11 los abre nativamente.
    """
    s = str(raw).strip().strip("{}").strip('"').strip("'")
    if not s:
        return s

    # WSL: convertir paths Windows → /mnt/...
    if IS_WSL and _PATRON_DRIVE_LETTER.match(s):
        wp = shutil.which("wslpath")
        if wp is not None:
            try:
                proc = subprocess.run(
                    [wp, "-u", s], capture_output=True, text=True,
                    timeout=2)
                salida = proc.stdout.strip()
                if proc.returncode == 0 and salida:
                    return salida
            except (OSError, subprocess.TimeoutExpired):
                pass
        # Fallback manual: C:\path\foo  →  /mnt/c/path/foo
        unidad = s[0].lower()
        resto = s[2:].replace("\\", "/")
        if not resto.startswith("/"):
            resto = "/" + resto
        return f"/mnt/{unidad}{resto}"

    # WSL: convertir UNC \\wsl.localhost\... a su /... equivalente
    if IS_WSL and _PATRON_UNC_WSL.match(s):
        wp = shutil.which("wslpath")
        if wp is not None:
            try:
                proc = subprocess.run(
                    [wp, "-u", s], capture_output=True, text=True,
                    timeout=2)
                salida = proc.stdout.strip()
                if proc.returncode == 0 and salida:
                    return salida
            except (OSError, subprocess.TimeoutExpired):
                pass
        # Sin wslpath, intento heurístico: \\wsl.localhost\Distro\home\u\f
        partes = s.replace("\\", "/").split("/")
        # Saltar entradas vacías y el nombre de la distro: ['', '', 'wsl.localhost', 'Distro', 'home', ...]
        try:
            idx_distro = next(i for i, p in enumerate(partes)
                              if p.lower().startswith("wsl"))
            return "/" + "/".join(partes[idx_distro + 2:])
        except (StopIteration, IndexError):
            pass

    return s
LR0_DOT_PATH      = OUTPUT_DIR / "lr0.dot"
LR0_PNG_PATH      = OUTPUT_DIR / "lr0.png"
TABLAS_JSON_PATH  = OUTPUT_DIR / "tablas.json"
ESTADO_IDE_PATH   = PROJECT_ROOT / ".ide_state.json"

COMANDO_COMPILACION = [
    "g++", "-std=c++17", "-O2", "-DCOMPILAR_CON_ORQUESTADOR",
    "-o", str(EJECUTABLE_PATH),
    str(PROJECT_ROOT / "Main.cpp"),
    str(PROJECT_ROOT / "Lexer" / "YalexParser.cpp"),
    str(PROJECT_ROOT / "Parser" / "YaparParser.cpp"),
    str(PROJECT_ROOT / "Parser" / "Grammar.cpp"),
    str(PROJECT_ROOT / "Parser" / "FirstFollow.cpp"),
    str(PROJECT_ROOT / "Parser" / "LL1Table.cpp"),
    str(PROJECT_ROOT / "Parser" / "LR0.cpp"),
    str(PROJECT_ROOT / "Parser" / "SLR1.cpp"),
    str(PROJECT_ROOT / "Parser" / "LALR1.cpp"),
]


# ──────────────────────────────────────────────────────────────────────
# Paleta de tema oscuro
# ──────────────────────────────────────────────────────────────────────

class Tema:
    BG          = "#17191d"   # Fondo principal
    BG_PANEL    = "#20242a"   # Paneles
    BG_PANEL_2  = "#282d35"   # Paneles secundarios (gutter, headers)
    BG_BUTTON   = "#2f353d"   # Botones
    BG_BUTTON_H = "#3a424c"   # Botones hover
    BG_ACTIVE   = "#183a52"   # Selección / tab activo
    BORDE       = "#252a31"
    BORDE_SOFT  = "#1f2329"

    FG          = "#edf2f7"
    FG_MUTED    = "#97a3b6"
    FG_INVERT   = "#17191d"

    ACCENT      = "#5fb3f3"   # Azul principal
    ACCENT_DARK = "#145b86"
    ACCENT_SOFT = "#24455e"
    ACCENT_WARM = "#f2a65a"

    OK          = "#7ad27f"   # Verde
    WARN        = "#f2d06b"   # Amarillo
    ERR         = "#ff8f7a"   # Rojo
    INFO        = "#7cc7ff"   # Azul info

    CONFLICTO_SR_BG = "#5a4a1a"   # shift/reduce
    CONFLICTO_SR_FG = "#ffe27a"
    CONFLICTO_RR_BG = "#5a1f1f"   # reduce/reduce
    CONFLICTO_RR_FG = "#ffaaaa"

    # ── Highlights interactivos del editor ──
    CURRENT_LINE  = "#222831"   # Fondo de la línea actual
    MATCH_BRACKET = "#305978"   # Fondo del bracket pareado
    DND_HINT      = "#145b86"   # Estado del toolbar mientras se arrastra

    # ── Resaltado de sintaxis (estilo VSCode Dark+) ──
    SYN_KEYWORD = "#569cd6"   # let, rule, %token, %ignore, ...
    SYN_DIRECT  = "#c586c0"   # %left, %right, %prec, ... (directivas)
    SYN_STRING  = "#ce9178"   # "..." y '.'
    SYN_COMMENT = "#6a9955"   # (* ... *) y /* ... */
    SYN_NUMBER  = "#b5cea8"   # 123 0x... 
    SYN_OP      = "#dcdcaa"   # | * + ? ( ) [ ] : ;
    SYN_TERM    = "#4ec9b0"   # TERMINALES en MAYUSCULA (en YAPar)
    SYN_NT      = "#9cdcfe"   # no-terminales (lhs de produccion)
    SYN_HEAD    = "#dcdcaa"   # encabezado YAPar (%%)


# ──────────────────────────────────────────────────────────────────────
# Patrones de resaltado de sintaxis
# ──────────────────────────────────────────────────────────────────────
#
# El orden importa: se aplica primero la tupla más larga (comentarios y
# strings antes que palabras reservadas, etc.).
#
PATRONES_YALEX = [
    ("com",    re.compile(r"\(\*[\s\S]*?\*\)")),
    ("str",    re.compile(r'"(?:\\.|[^"\\])*"')),
    ("str",    re.compile(r"'(?:\\.|[^'\\])'")),
    ("kw",     re.compile(r"\b(?:let|rule|eof|return|null)\b")),
    ("num",    re.compile(r"\b\d+\b")),
    ("op",     re.compile(r"[|*+?()\[\]=]")),
]

# Para "nt" (no-terminal al lado izquierdo de :) queremos resaltar
# SOLO el identificador, no los ":" — para eso devolvemos el grupo 1.
# La función _resaltar lo detecta porque "nt" es el único caso especial.
EMPTY_STATES = {
    "salida":      "  ▷  Ejecutá Compilar (F5) o Ejecutar análisis (F6)\n"
                    "      para ver la salida del compilador aquí.",
    "tokens":      "  ▷  Tokens emitidos por el lexer (después de filtrar IGNORE).\n"
                    "      Ejecutá un análisis (F6) para verlos.",
    "validaciones":"  ▷  Validaciones del .yapar contra el .yal, gramática y tabla\n"
                    "      de símbolos. Disponibles tras ejecutar un análisis.",
    "ff":          "  ▷  Conjuntos FIRST y FOLLOW de la gramática.\n"
                    "      Se calculan al ejecutar un análisis.",
    "ll1_text":    "  ▷  Tabla LL(1) en texto plano.\n"
                    "      Mirá la tab 'Tabla LL(1)' para la vista interactiva.",
    "slr1_text":   "  ▷  Tabla SLR(1) en texto plano.\n"
                    "      Mirá la tab 'Tabla SLR(1)' para la vista interactiva.",
    "lalr1_text":  "  ▷  Tabla LALR(1) en texto plano.\n"
                    "      Mirá la tab 'Tabla LALR(1)' para la vista interactiva.",
    "traza_ll1":   "  ▷  Traza paso a paso del parser LL(1).\n"
                    "      Disponible tras ejecutar un análisis.",
    "lr0":         "  ▷  Autómata LR(0) (gramática aumentada, items y GOTOs).\n"
                    "      Mirá también 'Autómata LR(0)' para el render gráfico.",
    "resultado":   "  ▷  Veredicto por parser (LL(1) / SLR(1) / LALR(1)) y resumen\n"
                    "      del pipeline. Disponible tras ejecutar.",
    "errores":     "  ✓  Sin errores.",
}


PATRONES_YAPAR = [
    ("com",    re.compile(r"/\*[\s\S]*?\*/")),
    ("com",    re.compile(r"//[^\n]*")),
    ("str",    re.compile(r"'(?:\\.|[^'\\])*'")),
    ("direct", re.compile(r"%(?:token|ignore|start|left|right|nonassoc|"
                          r"prec|type|expect|union|code)\b")),
    ("direct", re.compile(r"\bIGNORE\b")),       # variante de Compis
    ("head",   re.compile(r"%%")),
    ("term",   re.compile(r"\b[A-Z][A-Z0-9_]*\b")),
    ("nt",     re.compile(r"^\s*([a-z_][a-zA-Z0-9_]*)\s*:", re.MULTILINE)),
    ("num",    re.compile(r"\b\d+\b")),
    ("op",     re.compile(r"[|:;]")),
]


def _resolver_fuente_mono(prefer=("JetBrains Mono", "Cascadia Mono",
                                   "Consolas", "DejaVu Sans Mono",
                                   "Menlo", "Courier New")):
    """Devuelve la primera fuente monoespaciada disponible."""
    disponibles = {f.lower() for f in tkfont.families()}
    for nombre in prefer:
        if nombre.lower() in disponibles:
            return nombre
    return "TkFixedFont"


# Se inicializan más abajo, cuando hay Tk root
FUENTE_MONO_NAME = "Consolas"
FUENTE_MONO_SIZE = 11
FUENTE_UI_NAME   = "Segoe UI"
FUENTE_UI_SIZE   = 9


# ──────────────────────────────────────────────────────────────────────
# Particionado de la salida del compilador
# ──────────────────────────────────────────────────────────────────────

SEPARADOR_LINEA = "=" * 50

MAPA_SECCIONES = [
    ("tokens",       ["Analisis lexico"]),
    ("validaciones", ["Lectura del .yapar",
                       "Tabla de simbolos",
                       "Gramatica"]),
    ("ff",           ["FIRST", "FOLLOW"]),
    ("ll1",          ["Tabla LL(1)"]),
    ("traza_ll1",    ["Parsing LL(1)"]),
    ("lr0",          ["Automata LR(0)", "Automata LR"]),
    ("slr1",         ["Tabla SLR(1)", "Evaluacion SLR(1)"]),
    ("lalr1",        ["Tabla LALR(1)", "Evaluacion LALR(1)"]),
    ("resumen",      ["Resumen"]),
]


def extraer_bloques(stdout):
    lineas = stdout.splitlines()
    bloques = []
    titulo_actual = None
    cuerpo_actual = []

    i = 0
    while i < len(lineas):
        es_sep_arriba = lineas[i].strip().startswith("=====")
        if (es_sep_arriba and i + 2 < len(lineas)
                and lineas[i + 2].strip().startswith("=====")):
            if titulo_actual is not None:
                bloques.append((titulo_actual, "\n".join(cuerpo_actual)))
            titulo_actual = lineas[i + 1].strip()
            cuerpo_actual = []
            i += 3
            continue
        if titulo_actual is not None:
            cuerpo_actual.append(lineas[i])
        i += 1

    if titulo_actual is not None:
        bloques.append((titulo_actual, "\n".join(cuerpo_actual)))
    return bloques


def dividir_salida(stdout, stderr):
    secciones = {clave: "" for clave, _ in MAPA_SECCIONES}
    bloques = extraer_bloques(stdout)

    for clave, claves_titulo in MAPA_SECCIONES:
        piezas = []
        for titulo, cuerpo in bloques:
            for ct in claves_titulo:
                if ct in titulo:
                    piezas.append(f"=== {titulo} ===\n{cuerpo}".rstrip())
                    break
        secciones[clave] = "\n\n".join(piezas)

    errores = []
    fuentes = stdout.splitlines() + stderr.splitlines()
    for linea in fuentes:
        limpia = linea.strip()
        if not limpia:
            continue
        u = limpia.upper()
        if (u.startswith("ERROR")
                or "ERROR SINTACTICO" in u
                or "ERROR LEXICO" in u
                or "ADVERTENCIA" in u
                or "FAIL" in u
                or "CONFLICTO" in u
                or "SHIFT/REDUCE" in u
                or "REDUCE/REDUCE" in u
                or "RECHAZADO" in u):
            errores.append(linea)
    secciones["errores"] = "\n".join(errores)
    return secciones


def extraer_veredictos(stdout):
    parsers = {
        "ll1":   {"es": "?", "resultado": "?", "conflictos": None},
        "slr1":  {"es": "?", "resultado": "?", "conflictos": None},
        "lalr1": {"es": "?", "resultado": "?", "conflictos": None},
    }
    activo = None
    for raw in stdout.splitlines():
        linea = raw.strip()
        if linea.startswith("LL(1):"):
            activo = "ll1"; continue
        if linea.startswith("SLR(1):"):
            activo = "slr1"; continue
        if linea.startswith("LALR(1):"):
            activo = "lalr1"; continue
        if linea.startswith("LR(0):") or linea == "":
            continue
        if activo is None:
            continue
        if linea.startswith("Es LL(1):") and activo == "ll1":
            parsers["ll1"]["es"] = linea.split(":", 1)[1].strip()
        elif linea.startswith("Es SLR(1):") and activo == "slr1":
            parsers["slr1"]["es"] = linea.split(":", 1)[1].strip()
        elif linea.startswith("Es LALR(1):") and activo == "lalr1":
            parsers["lalr1"]["es"] = linea.split(":", 1)[1].strip()
        elif linea.startswith("Resultado:"):
            parsers[activo]["resultado"] = linea.split(":", 1)[1].strip()
        elif linea.startswith("Conflictos:"):
            try:
                parsers[activo]["conflictos"] = int(
                    linea.split(":", 1)[1].strip())
            except ValueError:
                pass
    return parsers


def formatear_veredictos(veredictos, codigo):
    enc = f"{'Parser':<10}{'¿Es ese?':<12}{'Conflictos':<14}{'Resultado'}"
    sep = "─" * len(enc)
    filas = [enc, sep]
    for clave, etiqueta in (("ll1", "LL(1)"),
                             ("slr1", "SLR(1)"),
                             ("lalr1", "LALR(1)")):
        v = veredictos[clave]
        es = v["es"]
        conf = v["conflictos"]
        conf_str = "?" if conf is None else str(conf)
        res = v["resultado"]
        filas.append(f"{etiqueta:<10}{es:<12}{conf_str:<14}{res}")
    filas.append("")
    filas.append(f"Codigo de salida del proceso: {codigo}")
    return "\n".join(filas)



def parser_ganador(veredictos):
    """Devuelve el parser de mayor poder que aceptó, o '-' si ninguno."""
    for clave, etiqueta in (("lalr1", "LALR(1)"),
                             ("slr1", "SLR(1)"),
                             ("ll1", "LL(1)")):
        res = veredictos.get(clave, {}).get("resultado", "")
        if res.startswith("ACEPTADO"):   # cubre "ACEPTADO" y "ACEPTADO CON ERRORES"
            return etiqueta
    return "-"

def contar_tokens_filtrados(stdout):
    """Lee del Resumen el conteo 'Tokens tras filtrado'."""
    for linea in stdout.splitlines():
        s = linea.strip()
        if s.startswith("Tokens tras filtrado:"):
            try:
                resto = s.split(":", 1)[1].strip()
                num = resto.split()[0]
                return int(num)
            except (ValueError, IndexError):
                return None
    return None


# ──────────────────────────────────────────────────────────────────────
# Widget: editor con números de línea
# ──────────────────────────────────────────────────────────────────────

class EditorConNumeros(ttk.Frame):
    """Text widget oscuro con gutter de números de línea sincronizado y
    resaltado de sintaxis básico para YALex / YAPar."""

    def __init__(self, master, on_cursor_change=None,
                  on_modificado_change=None, tipo="plain"):
        super().__init__(master, style="Editor.TFrame")

        self._on_cursor_change = on_cursor_change
        self._on_modificado_change = on_modificado_change
        self.ruta = None
        self._titulo_base = "(sin archivo)"
        self._tipo = tipo
        self._resaltado_after = None
        self._modificado = False
        self._cargando = False  # bandera para no marcar como modificado al cargar

        contenedor = ttk.Frame(self, style="Editor.TFrame")
        contenedor.pack(fill="both", expand=True)

        self._gutter = tk.Canvas(
            contenedor, width=46, highlightthickness=0,
            background=Tema.BG_PANEL_2, borderwidth=0,
        )
        self._gutter.grid(row=0, column=0, sticky="ns")

        self.text = tk.Text(
            contenedor, wrap="none", undo=True,
            font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE),
            background=Tema.BG, foreground=Tema.FG,
            insertbackground=Tema.FG,
            selectbackground=Tema.BG_ACTIVE,
            selectforeground=Tema.FG,
            borderwidth=0, relief="flat",
            padx=12, pady=10,
            tabs=("4c",),
            tabstyle="wordprocessor",
        )
        self.text.grid(row=0, column=1, sticky="nsew")

        scroll_y = ttk.Scrollbar(
            contenedor, orient="vertical",
            command=self._yview_sync,
        )
        scroll_y.grid(row=0, column=2, sticky="ns")

        scroll_x = ttk.Scrollbar(
            contenedor, orient="horizontal",
            command=self.text.xview,
        )
        scroll_x.grid(row=1, column=1, sticky="ew")

        self.text.configure(
            yscrollcommand=self._on_yscroll_text,
            xscrollcommand=scroll_x.set,
        )

        contenedor.rowconfigure(0, weight=1)
        contenedor.columnconfigure(1, weight=1)

        # Eventos para mantener números y cursor info actualizados.
        for ev in ("<KeyRelease>", "<MouseWheel>", "<Button-1>",
                   "<ButtonRelease-1>", "<Configure>",
                   "<<Modified>>"):
            self.text.bind(ev, self._on_evento_text, add="+")
        # Auto-indentación básica con Enter.
        self.text.bind("<Return>", self._on_return, add="+")

        # Tags de resaltado de sintaxis (siempre configurados; se aplican
        # según self._tipo en _resaltar()).
        self.text.tag_configure("syn_kw",     foreground=Tema.SYN_KEYWORD,
                                  font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE,
                                          "bold"))
        self.text.tag_configure("syn_direct", foreground=Tema.SYN_DIRECT,
                                  font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE,
                                          "bold"))
        self.text.tag_configure("syn_str",    foreground=Tema.SYN_STRING)
        self.text.tag_configure("syn_com",    foreground=Tema.SYN_COMMENT,
                                  font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE,
                                          "italic"))
        self.text.tag_configure("syn_num",    foreground=Tema.SYN_NUMBER)
        self.text.tag_configure("syn_op",     foreground=Tema.SYN_OP)
        self.text.tag_configure("syn_term",   foreground=Tema.SYN_TERM)
        self.text.tag_configure("syn_nt",     foreground=Tema.SYN_NT,
                                  font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE,
                                          "bold"))
        self.text.tag_configure("syn_head",   foreground=Tema.SYN_HEAD,
                                  font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE,
                                          "bold"))

        # Tag para la línea actual (fondo más claro).
        self.text.tag_configure("current_line",
                                  background=Tema.CURRENT_LINE)
        # Tag para el match de paréntesis/brackets.
        self.text.tag_configure("match_bracket",
                                  background=Tema.MATCH_BRACKET,
                                  foreground=Tema.FG)
        # Las pasadas posteriores no deben tapar `current_line`.
        # Las líneas de sintaxis se aplican _por encima_ (no usan background).
        self.text.tag_lower("current_line")

        self._lineas_dibujadas = 0

    def _yview_sync(self, *args):
        self.text.yview(*args)
        self._redibujar_gutter()

    def _on_yscroll_text(self, first, last):
        # Reenviar al scrollbar real
        first_f = float(first)
        last_f = float(last)
        # Buscar scrollbar
        children = self.text.master.grid_slaves(row=0, column=2)
        if children and isinstance(children[0], ttk.Scrollbar):
            children[0].set(first, last)
        self._redibujar_gutter()
        del first_f, last_f  # silenciar lint

    def _on_evento_text(self, _ev=None):
        self._redibujar_gutter()
        self._resaltar_linea_actual()
        self._resaltar_brackets()
        self._programar_resaltado()
        # Marcar modificado (excepto cuando estamos cargando un archivo).
        try:
            esta_mod = bool(self.text.edit_modified())
        except tk.TclError:
            esta_mod = False
        if esta_mod and not self._cargando and not self._modificado:
            self._modificado = True
            if self._on_modificado_change is not None:
                self._on_modificado_change(self, True)
        if self._on_cursor_change is not None:
            self._on_cursor_change(self)
        # Reiniciar bandera para que <<Modified>> siga disparando.
        try:
            self.text.edit_modified(False)
        except tk.TclError:
            pass

    # ── Línea actual y match de brackets ──────────────────────────────

    def _resaltar_linea_actual(self):
        try:
            self.text.tag_remove("current_line", "1.0", "end")
            inicio = self.text.index("insert linestart")
            fin = self.text.index("insert lineend +1c")
            self.text.tag_add("current_line", inicio, fin)
        except tk.TclError:
            pass

    _PARES_BRACKETS = {
        "(": (")", 1), ")": ("(", -1),
        "[": ("]", 1), "]": ("[", -1),
        "{": ("}", 1), "}": ("{", -1),
    }

    def _resaltar_brackets(self):
        text = self.text
        try:
            text.tag_remove("match_bracket", "1.0", "end")
        except tk.TclError:
            return

        # Tomar el carácter en el cursor y el anterior; preferir el "abre"
        # si hay uno justo a la izquierda.
        try:
            cursor = text.index("insert")
            ch_aqui = text.get(cursor)
            ch_prev = text.get(f"{cursor} -1c") if cursor != "1.0" else ""
        except tk.TclError:
            return

        objetivo_idx = None
        objetivo_ch = None
        if ch_prev in self._PARES_BRACKETS:
            objetivo_idx = f"{cursor} -1c"
            objetivo_ch = ch_prev
        elif ch_aqui in self._PARES_BRACKETS:
            objetivo_idx = cursor
            objetivo_ch = ch_aqui
        if objetivo_idx is None:
            return

        pareja_ch, direccion = self._PARES_BRACKETS[objetivo_ch]
        contenido = text.get("1.0", "end-1c")
        pos_obj = self._index_to_offset(text, objetivo_idx)
        if pos_obj is None:
            return

        # Búsqueda manual contando profundidad, ignorando contenido en
        # strings / comentarios sería ideal, pero a nivel "básico" basta
        # con balance simple. Es suficiente para feedback visual.
        nivel = 0
        i = pos_obj
        while 0 <= i < len(contenido):
            c = contenido[i]
            if c == objetivo_ch:
                nivel += 1
            elif c == pareja_ch:
                nivel -= 1
                if nivel == 0:
                    pareja_idx = f"1.0 + {i} chars"
                    try:
                        text.tag_add("match_bracket",
                                       objetivo_idx, f"{objetivo_idx} +1c")
                        text.tag_add("match_bracket",
                                       pareja_idx, f"{pareja_idx} +1c")
                    except tk.TclError:
                        pass
                    return
            i += direccion

    @staticmethod
    def _index_to_offset(text, indice):
        try:
            previo = text.get("1.0", indice)
            return len(previo)
        except tk.TclError:
            return None

    # ── Resaltado de sintaxis con debounce ────────────────────────────

    def _programar_resaltado(self, retraso_ms=150):
        if self._tipo not in ("yalex", "yapar"):
            return
        if self._resaltado_after is not None:
            try:
                self.text.after_cancel(self._resaltado_after)
            except (tk.TclError, ValueError):
                pass
        self._resaltado_after = self.text.after(retraso_ms, self._resaltar)

    def _resaltar(self):
        self._resaltado_after = None
        if self._tipo not in ("yalex", "yapar"):
            return
        patrones = (PATRONES_YALEX if self._tipo == "yalex"
                     else PATRONES_YAPAR)

        # Limpiar tags previos
        for tag in ("syn_kw", "syn_direct", "syn_str", "syn_com",
                     "syn_num", "syn_op", "syn_term", "syn_nt",
                     "syn_head"):
            self.text.tag_remove(tag, "1.0", "end")

        contenido = self.text.get("1.0", "end-1c")
        if not contenido:
            return

        # Mapa nombre-corto -> tag completo
        tag_map = {
            "kw":     "syn_kw",
            "direct": "syn_direct",
            "str":    "syn_str",
            "com":    "syn_com",
            "num":    "syn_num",
            "op":     "syn_op",
            "term":   "syn_term",
            "nt":     "syn_nt",
            "head":   "syn_head",
        }

        # Para no pisar comentarios y strings con otros tags, los
        # aplicamos primero y guardamos los rangos ocupados.
        ocupados = []  # lista de (inicio, fin) ya con tag prioritario

        def _solapa(a, b, ranges):
            for (s, e) in ranges:
                if not (b <= s or a >= e):
                    return True
            return False

        # Pasada 1: comentarios y strings (prioritarios)
        prioritarios = [(nm, p) for (nm, p) in patrones
                          if nm in ("com", "str")]
        for nombre, patron in prioritarios:
            tag = tag_map[nombre]
            for m in patron.finditer(contenido):
                a, b = m.span()
                if _solapa(a, b, ocupados):
                    continue
                self._aplicar_tag(tag, a, b)
                ocupados.append((a, b))

        # Pasada 2: el resto
        for nombre, patron in patrones:
            if nombre in ("com", "str"):
                continue
            tag = tag_map.get(nombre)
            if tag is None:
                continue
            for m in patron.finditer(contenido):
                # Caso especial: para "nt" queremos solo el identificador
                # (grupo 1), no el ":" que también está en el match.
                if nombre == "nt":
                    a, b = m.span(1)
                else:
                    a, b = m.span()
                if _solapa(a, b, ocupados):
                    continue
                self._aplicar_tag(tag, a, b)

    def _aplicar_tag(self, tag, char_inicio, char_fin):
        self.text.tag_add(
            tag,
            f"1.0 + {char_inicio} chars",
            f"1.0 + {char_fin} chars",
        )

    def _on_return(self, _ev=None):
        try:
            indice = self.text.index("insert linestart")
            linea = self.text.get(indice, "insert")
            indent = []
            for c in linea:
                if c in (" ", "\t"):
                    indent.append(c)
                else:
                    break
            self.text.insert("insert", "\n" + "".join(indent))
            self._redibujar_gutter()
        except tk.TclError:
            return None
        return "break"

    def _redibujar_gutter(self):
        c = self._gutter
        c.delete("all")
        try:
            primera = self.text.index("@0,0")
            ultima_visible = self.text.index(
                f"@0,{self.text.winfo_height()}")
        except tk.TclError:
            return
        try:
            primera_n = int(primera.split(".")[0])
            ultima_n  = int(ultima_visible.split(".")[0])
        except ValueError:
            return
        total = int(self.text.index("end-1c").split(".")[0])
        ultima_n = min(ultima_n, total)

        ancho_dig = len(str(max(total, 1)))
        nuevo_ancho = max(46, 12 + ancho_dig * 9)
        if int(c["width"]) != nuevo_ancho:
            c.configure(width=nuevo_ancho)

        for i in range(primera_n, ultima_n + 1):
            try:
                bbox = self.text.bbox(f"{i}.0")
            except tk.TclError:
                bbox = None
            if not bbox:
                continue
            y = bbox[1] + 1
            c.create_text(
                int(c["width"]) - 6, y,
                anchor="ne",
                text=str(i),
                fill=Tema.FG_MUTED,
                font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE - 1),
            )

    # ── Operaciones de archivo ────────────────────────────────────────

    def cargar(self, ruta):
        ruta = Path(ruta)
        with open(ruta, "r", encoding="utf-8", errors="replace") as f:
            contenido = f.read()
        self._cargando = True
        self.text.delete("1.0", tk.END)
        self.text.insert("1.0", contenido)
        try:
            self.text.edit_modified(False)
        except tk.TclError:
            pass
        self._cargando = False
        self.ruta = ruta
        self._marcar_modificado(False)
        self._redibujar_gutter()
        self._resaltar_linea_actual()
        # Resaltar inmediatamente al cargar (sin debounce).
        self._resaltar()
        if self._on_cursor_change is not None:
            self._on_cursor_change(self)

    def guardar(self, nueva_ruta=None):
        if nueva_ruta is not None:
            self.ruta = Path(nueva_ruta)
        if self.ruta is None:
            raise ValueError("Editor sin ruta asignada")
        contenido = self.text.get("1.0", "end-1c")
        with open(self.ruta, "w", encoding="utf-8") as f:
            f.write(contenido)
        self._marcar_modificado(False)

    def _marcar_modificado(self, valor):
        if valor == self._modificado:
            return
        self._modificado = valor
        if self._on_modificado_change is not None:
            self._on_modificado_change(self, valor)

    def vacio(self):
        return not self.text.get("1.0", "end-1c").strip()

    def linea_columna(self):
        try:
            idx = self.text.index("insert")
            l, c = idx.split(".")
            return int(l), int(c) + 1  # 1-indexed
        except tk.TclError:
            return 1, 1


# ──────────────────────────────────────────────────────────────────────
# Tabla visual con ttk.Treeview (LL(1) / SLR(1) / LALR(1))
# ──────────────────────────────────────────────────────────────────────

class TablaParser(ttk.Frame):
    """Treeview con headers fijos, scroll H/V y resaltado de conflictos."""

    def __init__(self, master):
        super().__init__(master, style="Card.TFrame")
        self._tree = ttk.Treeview(self, show="headings",
                                   style="Tabla.Treeview")
        scroll_y = ttk.Scrollbar(self, orient="vertical",
                                  command=self._tree.yview)
        scroll_x = ttk.Scrollbar(self, orient="horizontal",
                                  command=self._tree.xview)
        self._tree.configure(yscrollcommand=scroll_y.set,
                              xscrollcommand=scroll_x.set)
        self._tree.grid(row=0, column=0, sticky="nsew")
        scroll_y.grid(row=0, column=1, sticky="ns")
        scroll_x.grid(row=1, column=0, sticky="ew")
        self.rowconfigure(0, weight=1)
        self.columnconfigure(0, weight=1)

        self._tree.tag_configure(
            "conflicto_sr",
            background=Tema.CONFLICTO_SR_BG,
            foreground=Tema.CONFLICTO_SR_FG)
        self._tree.tag_configure(
            "conflicto_rr",
            background=Tema.CONFLICTO_RR_BG,
            foreground=Tema.CONFLICTO_RR_FG)

    def limpiar(self):
        for item in self._tree.get_children():
            self._tree.delete(item)
        self._tree["columns"] = ()

    def cargar_tabla_ll1(self, datos, conflictos_por_celda):
        """datos = {terminales, filas: {NT: {t: prod_idx}}}.
        conflictos_por_celda = set((NT, t)).
        """
        self.limpiar()
        if not datos or "terminales" not in datos:
            return
        cols = ["NT"] + list(datos["terminales"])
        self._tree["columns"] = cols
        for c in cols:
            self._tree.heading(c, text=c)
            ancho = 60 if c == "NT" else 56
            self._tree.column(c, width=ancho, anchor="center", stretch=False)

        for nt, fila in datos.get("filas", {}).items():
            valores = [nt]
            tags = []
            for t in datos["terminales"]:
                v = fila.get(t, "")
                valores.append(str(v))
            if any((nt, t) in conflictos_por_celda for t in datos["terminales"]):
                tags.append("conflicto_rr")
            self._tree.insert("", "end", values=valores, tags=tags)

    def cargar_tabla_lr(self, datos, etiqueta):
        """datos = TablaSLR/LALR (dict de tablas.json)."""
        self.limpiar()
        if not datos or "action" not in datos:
            return
        terms  = list(datos.get("terminales", []) or [])
        nterms = list(datos.get("no_terminales", []) or [])
        # Construir columnas con grupo visual
        cols_action = [f"a:{t}" for t in terms]
        cols_goto   = [f"g:{nt}" for nt in nterms]
        cols = ["Est"] + cols_action + cols_goto
        self._tree["columns"] = cols
        self._tree.heading("Est", text="Est")
        self._tree.column("Est", width=46, anchor="center", stretch=False)
        for c, etq in zip(cols_action, terms):
            self._tree.heading(c, text=etq)
            self._tree.column(c, width=58, anchor="center", stretch=False)
        for c, etq in zip(cols_goto, nterms):
            self._tree.heading(c, text=etq)
            self._tree.column(c, width=58, anchor="center", stretch=False)

        # Map conflictos por (estado, terminal) -> tipo
        conf_map = {}
        for c in datos.get("conflictos", []) or []:
            est = c.get("estado")
            t   = c.get("terminal")
            desc = (c.get("descripcion") or "").lower()
            tipo = "conflicto_rr" if "reduce/reduce" in desc else "conflicto_sr"
            conf_map[(est, t)] = tipo

        action = datos["action"]
        goto   = datos.get("goto", [])
        for i, fila_action in enumerate(action):
            fila_goto = goto[i] if i < len(goto) else {}
            valores = [i]
            for t in terms:
                valores.append(fila_action.get(t, ""))
            for nt in nterms:
                v = fila_goto.get(nt, "")
                valores.append(str(v) if v != "" else "")
            tags = []
            for t in terms:
                if (i, t) in conf_map:
                    tags = [conf_map[(i, t)]]
                    break
            self._tree.insert("", "end", values=valores, tags=tags)

        del etiqueta  # parámetro reservado para futuras variantes


# ──────────────────────────────────────────────────────────────────────
# Aplicación principal
# ──────────────────────────────────────────────────────────────────────

class IDE:
    def __init__(self, root):
        global FUENTE_MONO_NAME
        self.root = root
        FUENTE_MONO_NAME = _resolver_fuente_mono()

        self.root.title("Compis IDE — YALex + YAPar + Parsers")
        self.root.geometry("1400x880")
        self.root.minsize(1100, 680)
        self.root.configure(background=Tema.BG)

        self._configurar_estilo()

        # Estado
        self._ultimo_stdout = ""
        self._ultimo_stderr = ""
        self._ultimo_codigo = None
        self._ultimo_veredictos = None
        self._tablas_json = None
        self._lr0_imagen = None
        self.proceso_activo = False

        # Capturas para shortcuts contextuales
        self._editor_activo = None

        # Construcción UI
        self._construir_menu()
        self._construir_toolbar()
        self._construir_cuerpo()
        self._construir_barra_estado()
        self._registrar_shortcuts()
        self._registrar_drag_and_drop()

        # Estado inicial de la barra
        self._actualizar_estado()

        # Persistencia: cargar estado previo (rutas + layout) y registrar
        # el guardado al cerrar la ventana.
        self.root.after(80, self._cargar_estado)
        self.root.protocol("WM_DELETE_WINDOW", self._on_cerrar)

    # ── Estilo ttk ────────────────────────────────────────────────────

    def _configurar_estilo(self):
        s = ttk.Style()
        try:
            s.theme_use("clam")
        except tk.TclError:
            pass

        fuente_ui = (FUENTE_UI_NAME, FUENTE_UI_SIZE)
        fuente_ui_b = (FUENTE_UI_NAME, FUENTE_UI_SIZE, "bold")
        fuente_ui_sb = (FUENTE_UI_NAME, FUENTE_UI_SIZE + 1, "bold")

        s.configure(".",
                    background=Tema.BG,
                    foreground=Tema.FG,
                    fieldbackground=Tema.BG_PANEL,
                    bordercolor=Tema.BORDE_SOFT,
                    troughcolor=Tema.BG_PANEL_2,
                    font=fuente_ui)

        s.configure("TFrame", background=Tema.BG)
        s.configure("Card.TFrame", background=Tema.BG_PANEL)
        s.configure("Editor.TFrame", background=Tema.BG)
        s.configure("Side.TFrame", background=Tema.BG_PANEL_2)

        s.configure("TLabel", background=Tema.BG, foreground=Tema.FG)
        s.configure("Card.TLabel", background=Tema.BG_PANEL,
                    foreground=Tema.FG)
        s.configure("Muted.TLabel", background=Tema.BG,
                    foreground=Tema.FG_MUTED)
        s.configure("Side.TLabel", background=Tema.BG_PANEL_2,
                    foreground=Tema.FG, font=fuente_ui_sb)
        s.configure("Status.TLabel", background=Tema.BG_PANEL_2,
                    foreground=Tema.FG_MUTED, padding=(10, 4))
        s.configure("Accent.TLabel", background=Tema.BG_PANEL,
                    foreground=Tema.ACCENT, font=fuente_ui_b)
        s.configure("Section.TLabel", background=Tema.BG,
                    foreground=Tema.FG_MUTED, font=fuente_ui_b)

        s.configure("Toolbar.TFrame", background=Tema.BG_PANEL)
        s.configure("Drop.TFrame",    background=Tema.DND_HINT)
        s.configure("Status.TFrame", background=Tema.BG_PANEL_2)

        # Botones
        s.configure("TButton",
                    background=Tema.BG_BUTTON,
                    foreground=Tema.FG,
                    bordercolor=Tema.BG_BUTTON,
                    focuscolor=Tema.ACCENT,
                    lightcolor=Tema.BG_BUTTON,
                    darkcolor=Tema.BG_BUTTON,
                    padding=(14, 8),
                    relief="flat",
                    font=fuente_ui)
        s.map("TButton",
              background=[("active", Tema.BG_BUTTON_H),
                           ("pressed", Tema.ACCENT_DARK)],
              foreground=[("disabled", Tema.FG_MUTED)])

        s.configure("Accent.TButton",
                    background=Tema.ACCENT_DARK,
                    foreground=Tema.FG,
                    bordercolor=Tema.ACCENT_DARK,
                    lightcolor=Tema.ACCENT_DARK,
                    darkcolor=Tema.ACCENT_DARK,
                    padding=(16, 8))
        s.map("Accent.TButton",
              background=[("active", Tema.ACCENT),
                           ("pressed", Tema.ACCENT_DARK)])

        # Scrollbars
        s.configure("TScrollbar",
                    background=Tema.BG_BUTTON,
                    troughcolor=Tema.BG_PANEL,
                    bordercolor=Tema.BORDE_SOFT,
                    arrowcolor=Tema.FG_MUTED)
        s.map("TScrollbar",
              background=[("active", Tema.BG_BUTTON_H)])

        # Notebook (tabs)
        s.configure("TNotebook",
                    background=Tema.BG,
                    borderwidth=0,
                    bordercolor=Tema.BG,
                    tabmargins=(10, 10, 10, 0))
        s.configure("TNotebook.Tab",
                    background=Tema.BG_PANEL,
                    foreground=Tema.FG_MUTED,
                    padding=(18, 9),
                    borderwidth=0,
                    bordercolor=Tema.BG_PANEL,
                    lightcolor=Tema.BG_PANEL,
                    darkcolor=Tema.BG_PANEL,
                    font=fuente_ui)
        s.map("TNotebook.Tab",
              background=[("selected", Tema.BG_PANEL_2),
                           ("active", Tema.BG_BUTTON_H)],
              foreground=[("selected", Tema.FG),
                           ("active", Tema.FG)])

        # Treeview (tablas)
        s.configure("Tabla.Treeview",
                    background=Tema.BG_PANEL,
                    foreground=Tema.FG,
                    fieldbackground=Tema.BG_PANEL,
                    rowheight=22,
                    borderwidth=0,
                    font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE - 1))
        s.configure("Tabla.Treeview.Heading",
                    background=Tema.BG_PANEL_2,
                    foreground=Tema.ACCENT,
                    bordercolor=Tema.BG_PANEL_2,
                    font=fuente_ui_b,
                    padding=(8, 8))
        s.map("Tabla.Treeview",
              background=[("selected", Tema.BG_ACTIVE)],
              foreground=[("selected", Tema.FG)])
        s.map("Tabla.Treeview.Heading",
              background=[("active", Tema.BG_BUTTON_H)])

        # Treeview del panel Proyecto (sidebar)
        s.configure("Proyecto.Treeview",
                    background=Tema.BG_PANEL_2,
                    foreground=Tema.FG,
                    fieldbackground=Tema.BG_PANEL_2,
                    rowheight=28,
                    borderwidth=0,
                    font=fuente_ui)
        s.configure("Proyecto.Treeview.Heading",
                    background=Tema.BG_PANEL_2,
                    foreground=Tema.FG_MUTED)
        s.map("Proyecto.Treeview",
              background=[("selected", Tema.BG_ACTIVE)],
              foreground=[("selected", Tema.FG)])

        # Combobox
        s.configure("TCombobox",
                    fieldbackground=Tema.BG_PANEL,
                    background=Tema.BG_BUTTON,
                    foreground=Tema.FG,
                    arrowcolor=Tema.FG,
                    bordercolor=Tema.BG_BUTTON,
                    lightcolor=Tema.BG_PANEL,
                    darkcolor=Tema.BG_PANEL,
                    padding=(8, 6))
        s.map("TCombobox",
              fieldbackground=[("readonly", Tema.BG_PANEL)],
              foreground=[("readonly", Tema.FG)])

        # PanedWindow
        s.configure("TPanedwindow", background=Tema.BG)
        s.configure("TPanedwindow.Sash", background=Tema.BG_PANEL_2,
                    sashthickness=10)

        # Separator
        s.configure("TSeparator", background=Tema.BORDE_SOFT)

    # ── Menú superior ─────────────────────────────────────────────────

    def _construir_menu(self):
        menubar = tk.Menu(self.root, tearoff=0,
                          background=Tema.BG_PANEL, foreground=Tema.FG,
                          activebackground=Tema.BG_ACTIVE,
                          activeforeground=Tema.FG,
                          borderwidth=0)

        # Archivo
        m_archivo = tk.Menu(menubar, tearoff=0,
                             background=Tema.BG_PANEL, foreground=Tema.FG,
                             activebackground=Tema.BG_ACTIVE,
                             activeforeground=Tema.FG)
        m_archivo.add_command(label="Cargar YALex...",
                              command=self.cargar_yalex)
        m_archivo.add_command(label="Cargar YAPar...",
                              command=self.cargar_yapar)
        m_archivo.add_command(label="Cargar entrada...",
                              command=self.cargar_entrada)
        m_archivo.add_separator()
        m_archivo.add_command(label="Guardar",
                              accelerator="Ctrl+S",
                              command=self.guardar_activo)
        m_archivo.add_command(label="Guardar como...",
                              command=self.guardar_como_activo)
        m_archivo.add_command(label="Guardar todo",
                              accelerator="Ctrl+Shift+S",
                              command=self.guardar_todo)
        m_archivo.add_separator()
        m_archivo.add_command(label="Salir", command=self.root.quit)
        menubar.add_cascade(label="Archivo", menu=m_archivo)

        # Editar
        m_editar = tk.Menu(menubar, tearoff=0,
                            background=Tema.BG_PANEL, foreground=Tema.FG,
                            activebackground=Tema.BG_ACTIVE,
                            activeforeground=Tema.FG)
        m_editar.add_command(label="Deshacer", accelerator="Ctrl+Z",
                              command=lambda: self._enviar_evento("<<Undo>>"))
        m_editar.add_command(label="Rehacer", accelerator="Ctrl+Y",
                              command=lambda: self._enviar_evento("<<Redo>>"))
        m_editar.add_separator()
        m_editar.add_command(label="Cortar", accelerator="Ctrl+X",
                              command=lambda: self._enviar_evento("<<Cut>>"))
        m_editar.add_command(label="Copiar", accelerator="Ctrl+C",
                              command=lambda: self._enviar_evento("<<Copy>>"))
        m_editar.add_command(label="Pegar", accelerator="Ctrl+V",
                              command=lambda: self._enviar_evento("<<Paste>>"))
        m_editar.add_separator()
        m_editar.add_command(label="Buscar...", accelerator="Ctrl+F",
                              command=self.abrir_buscar)
        menubar.add_cascade(label="Editar", menu=m_editar)

        # Compilar
        m_comp = tk.Menu(menubar, tearoff=0,
                          background=Tema.BG_PANEL, foreground=Tema.FG,
                          activebackground=Tema.BG_ACTIVE,
                          activeforeground=Tema.FG)
        m_comp.add_command(label="Compilar", accelerator="F5",
                            command=self.compilar)
        m_comp.add_command(label="Ejecutar análisis", accelerator="F6",
                            command=self.ejecutar)
        m_comp.add_separator()
        m_comp.add_command(label="Limpiar consola", accelerator="Ctrl+L",
                            command=self.limpiar_salida)
        menubar.add_cascade(label="Compilar", menu=m_comp)

        # Visualizar
        m_vis = tk.Menu(menubar, tearoff=0,
                         background=Tema.BG_PANEL, foreground=Tema.FG,
                         activebackground=Tema.BG_ACTIVE,
                         activeforeground=Tema.FG)
        m_vis.add_command(label="Renderizar autómata LR(0)",
                           command=self.renderizar_lr0)
        m_vis.add_command(label="Abrir LR(0) externo (.dot)",
                           command=lambda: self._abrir_archivo(LR0_DOT_PATH))
        m_vis.add_command(label="Abrir LR(0) externo (PNG)",
                           command=lambda: self._abrir_archivo(LR0_PNG_PATH))
        m_vis.add_separator()
        m_vis.add_command(label="Mostrar FIRST/FOLLOW",
                           command=lambda: self._seleccionar_seccion("ff"))
        m_vis.add_command(label="Mostrar tabla LL(1)",
                           command=lambda: self._seleccionar_seccion("ll1"))
        m_vis.add_command(label="Mostrar tabla SLR(1)",
                           command=lambda: self._seleccionar_seccion("slr1"))
        m_vis.add_command(label="Mostrar tabla LALR(1)",
                           command=lambda: self._seleccionar_seccion("lalr1"))
        menubar.add_cascade(label="Visualizar", menu=m_vis)

        # Herramientas
        m_tools = tk.Menu(menubar, tearoff=0,
                           background=Tema.BG_PANEL, foreground=Tema.FG,
                           activebackground=Tema.BG_ACTIVE,
                           activeforeground=Tema.FG)
        m_tools.add_command(label="Exportar tablas (JSON / CSV)...",
                              command=self.exportar_tablas)
        m_tools.add_command(label="Abrir carpeta output",
                              command=lambda: self._abrir_archivo(OUTPUT_DIR))
        menubar.add_cascade(label="Herramientas", menu=m_tools)

        # Ayuda
        m_help = tk.Menu(menubar, tearoff=0,
                          background=Tema.BG_PANEL, foreground=Tema.FG,
                          activebackground=Tema.BG_ACTIVE,
                          activeforeground=Tema.FG)
        m_help.add_command(label="Atajos", command=self._mostrar_atajos)
        m_help.add_command(label="Acerca de", command=self._mostrar_acerca)
        menubar.add_cascade(label="Ayuda", menu=m_help)

        self.root.config(menu=menubar)

    # ── Toolbar compacta ──────────────────────────────────────────────

    def _construir_toolbar(self):
        barra = ttk.Frame(self.root, style="Toolbar.TFrame", padding=(14, 12))
        barra.pack(side="top", fill="x")

        # Acciones principales (4)
        ttk.Button(barra, text="▶  Compilar",
                    style="Accent.TButton",
                    command=self.compilar).pack(side="left", padx=(0, 10))
        ttk.Button(barra, text="▷  Ejecutar",
                    style="Accent.TButton",
                    command=self.ejecutar).pack(side="left", padx=(0, 6))

        ttk.Separator(barra, orient="vertical").pack(
            side="left", fill="y", padx=14, pady=2)

        ttk.Button(barra, text="⟳  LR(0)",
                    command=self.renderizar_lr0).pack(side="left", padx=(0, 8))
        ttk.Button(barra, text="✕  Limpiar",
                    command=self.limpiar_salida).pack(side="left")

        # Indicador derecho
        self._toolbar_estado = ttk.Label(
            barra, text="Listo", style="Accent.TLabel")
        self._toolbar_estado.configure(background=Tema.BG_PANEL)
        self._toolbar_estado.pack(side="right", padx=(12, 0))

        # Progressbar indeterminado (oculto por defecto).
        self._toolbar = barra  # referencia para empacar/desempacar el spinner
        self._spinner = ttk.Progressbar(
            barra, mode="indeterminate", length=140)

    def _iniciar_spinner(self):
        # Si ya está visible, no hacer nada.
        if self._spinner.winfo_ismapped():
            return
        # Pack al lado del label de estado (a la derecha).
        self._spinner.pack(side="right", padx=(2, 8), pady=2,
                            before=self._toolbar_estado)
        try:
            self._spinner.start(15)
        except tk.TclError:
            pass

    def _detener_spinner(self):
        try:
            self._spinner.stop()
        except tk.TclError:
            pass
        if self._spinner.winfo_ismapped():
            self._spinner.pack_forget()

    # ── Cuerpo principal ──────────────────────────────────────────────

    def _construir_cuerpo(self):
        contenedor = ttk.Frame(self.root)
        contenedor.pack(fill="both", expand=True)

        paned_h = ttk.PanedWindow(contenedor, orient="horizontal")
        paned_h.pack(fill="both", expand=True, padx=12, pady=(12, 10))

        # Sidebar - Proyecto
        self._construir_sidebar(paned_h)

        # Editor + área inferior
        right = ttk.Frame(paned_h)
        paned_h.add(right, weight=4)

        paned_v = ttk.PanedWindow(right, orient="vertical")
        paned_v.pack(fill="both", expand=True)

        # Editor con tabs (arriba)
        self._construir_editor(paned_v)
        # Grupos de pestañas (abajo)
        self._construir_resultados(paned_v)

    def _construir_sidebar(self, paned):
        side = ttk.Frame(paned, style="Side.TFrame", padding=(10, 10, 10, 12))
        paned.add(side, weight=1)

        ttk.Label(side, text="Proyecto", style="Side.TLabel",
                   padding=(6, 2, 4, 10)).pack(anchor="w", fill="x")

        self._proyecto = ttk.Treeview(
            side, show="tree", selectmode="browse",
            style="Proyecto.Treeview")
        self._proyecto.pack(fill="both", expand=True)
        self._proyecto.column("#0", width=220, stretch=True)

        self._proyecto_root = self._proyecto.insert(
            "", "end", text="📁  Compis", open=True)
        self._proyecto_items = {}
        for clave, etq in (("yalex",   "λ  (sin .yal)"),
                            ("yapar",   "Σ  (sin .yapar)"),
                            ("entrada", "≡  (sin entrada)")):
            iid = self._proyecto.insert(
                self._proyecto_root, "end", text=etq, values=(clave,))
            self._proyecto_items[clave] = iid

        self._proyecto.bind("<<TreeviewSelect>>",
                              self._on_proyecto_seleccion)
        self._proyecto.bind("<Double-1>",
                              self._on_proyecto_double)

    def _construir_editor(self, paned):
        marco = ttk.Frame(paned, style="Editor.TFrame", padding=(10, 2, 10, 8))
        paned.add(marco, weight=3)

        self._editor_tabs = ttk.Notebook(marco)
        self._editor_tabs.pack(fill="both", expand=True)

        # 3 editores fijos (yalex, yapar, entrada) con iconos Unicode sutiles.
        self._editores = {}
        self._editor_tab_index = {}
        self._editor_tab_titulo_base = {}
        for clave, titulo, tipo in (
            ("yalex",   "λ  lexer.yal",     "yalex"),
            ("yapar",   "Σ  parser.yapar",  "yapar"),
            ("entrada", "≡  input.txt",     "plain"),
        ):
            ed = EditorConNumeros(
                self._editor_tabs,
                on_cursor_change=self._on_editor_cursor,
                on_modificado_change=self._on_editor_modificado,
                tipo=tipo)
            self._editor_tabs.add(ed, text=titulo)
            idx = len(self._editores)
            self._editor_tab_index[clave] = idx
            self._editor_tab_titulo_base[clave] = titulo
            self._editores[clave] = ed
        self._editor_tabs.bind(
            "<<NotebookTabChanged>>", self._on_editor_tab_changed)
        self._editor_activo = self._editores["yalex"]

        # Extensiones de diálogo
        self._editores["yalex"].extensiones_dialogo = (
            ("YALex", "*.yal *.yalex"), ("Todos", "*.*"))
        self._editores["yapar"].extensiones_dialogo = (
            ("YAPar", "*.yapar"), ("Todos", "*.*"))
        self._editores["entrada"].extensiones_dialogo = (
            ("Texto", "*.txt"), ("Todos", "*.*"))

    def _construir_resultados(self, paned):
        marco = ttk.Frame(paned, style="Editor.TFrame", padding=(10, 6, 10, 10))
        paned.add(marco, weight=2)

        # Sub-toolbar: tabs de grupo
        self._grupos = ttk.Notebook(marco)
        self._grupos.pack(fill="both", expand=True)

        self.paneles = {}

        # Grupo 1: Resultados
        g1 = ttk.Frame(self._grupos, style="Card.TFrame")
        self._grupos.add(g1, text=" Resultados ")
        nb1 = ttk.Notebook(g1)
        nb1.pack(fill="both", expand=True, padx=8, pady=8)
        for clave, titulo in (
            ("salida",     "Consola"),
            ("tokens",     "Tokens"),
            ("validaciones", "YAPar / Validaciones"),
            ("resultado",  "Resultado"),
            ("errores",    "Errores"),
        ):
            self.paneles[clave] = self._panel_texto(nb1, titulo)

        # Grupo 2: Parsing
        g2 = ttk.Frame(self._grupos, style="Card.TFrame")
        self._grupos.add(g2, text=" Parsing ")
        nb2 = ttk.Notebook(g2)
        nb2.pack(fill="both", expand=True, padx=8, pady=8)
        self.paneles["ff"] = self._panel_texto(nb2, "FIRST / FOLLOW")
        # Tablas con Treeview
        self._tabla_ll1   = self._panel_tabla(nb2, "Tabla LL(1)")
        self._tabla_slr1  = self._panel_tabla(nb2, "Tabla SLR(1)")
        self._tabla_lalr1 = self._panel_tabla(nb2, "Tabla LALR(1)")
        # Versión texto fallback (la salida cruda del back)
        self.paneles["ll1_text"]   = self._panel_texto(nb2, "LL(1) texto")
        self.paneles["slr1_text"]  = self._panel_texto(nb2, "SLR(1) texto")
        self.paneles["lalr1_text"] = self._panel_texto(nb2, "LALR(1) texto")
        self.paneles["traza_ll1"] = self._panel_texto(nb2, "Traza LL(1)")
        self._construir_panel_parser_paso(nb2)

        # Grupo 3: Visualización
        g3 = ttk.Frame(self._grupos, style="Card.TFrame")
        self._grupos.add(g3, text=" Visualización ")
        nb3 = ttk.Notebook(g3)
        nb3.pack(fill="both", expand=True, padx=8, pady=8)
        self._construir_panel_lr0(nb3)
        self.paneles["lr0"] = self._panel_texto(nb3, "LR(0) (texto)")

    def _panel_texto(self, notebook, titulo):
        frame = ttk.Frame(notebook, style="Card.TFrame")
        notebook.add(frame, text=titulo)
        text = tk.Text(
            frame, wrap="none",
            font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE),
            background=Tema.BG_PANEL, foreground=Tema.FG,
            insertbackground=Tema.FG,
            selectbackground=Tema.BG_ACTIVE,
            selectforeground=Tema.FG,
            borderwidth=0, relief="flat",
            padx=12, pady=10,
            state="disabled",
        )
        sy = ttk.Scrollbar(frame, orient="vertical", command=text.yview)
        sx = ttk.Scrollbar(frame, orient="horizontal", command=text.xview)
        text.configure(yscrollcommand=sy.set, xscrollcommand=sx.set)
        text.grid(row=0, column=0, sticky="nsew")
        sy.grid(row=0, column=1, sticky="ns")
        sx.grid(row=1, column=0, sticky="ew")
        frame.rowconfigure(0, weight=1)
        frame.columnconfigure(0, weight=1)
        text._tab_frame = frame
        text._tab_titulo = titulo
        text._notebook = notebook

        # Tags de color para consola y errores
        text.tag_configure("ok",          foreground=Tema.OK)
        text.tag_configure("err",         foreground=Tema.ERR)
        text.tag_configure("warn",        foreground=Tema.WARN)
        text.tag_configure("info",        foreground=Tema.INFO)
        text.tag_configure("muted",       foreground=Tema.FG_MUTED)
        text.tag_configure("bold",
                            font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE,
                                  "bold"))
        text.tag_configure("conflicto_sr",
                            background=Tema.CONFLICTO_SR_BG,
                            foreground=Tema.CONFLICTO_SR_FG)
        text.tag_configure("conflicto_rr",
                            background=Tema.CONFLICTO_RR_BG,
                            foreground=Tema.CONFLICTO_RR_FG)
        text.tag_configure("veredicto_ok",
                            foreground=Tema.OK,
                            font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE,
                                  "bold"))
        text.tag_configure("veredicto_ko",
                            foreground=Tema.ERR,
                            font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE,
                                  "bold"))
        text.tag_configure("encabezado",
                            foreground=Tema.ACCENT,
                            font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE,
                                  "bold"))
        return text

    def _panel_tabla(self, notebook, titulo):
        frame = ttk.Frame(notebook, style="Card.TFrame")
        notebook.add(frame, text=titulo)
        tabla = TablaParser(frame)
        tabla.pack(fill="both", expand=True, padx=6, pady=6)
        tabla._tab_titulo = titulo
        tabla._notebook = notebook
        return tabla

    def _construir_panel_lr0(self, notebook):
        frame = ttk.Frame(notebook, style="Card.TFrame")
        notebook.add(frame, text="Autómata LR(0)")

        cabecera = ttk.Frame(frame, style="Card.TFrame", padding=(10, 10))
        cabecera.pack(side="top", fill="x")
        ttk.Label(
            cabecera,
            text="Autómata LR(0) — render con Graphviz (output/lr0.png).",
            style="Card.TLabel",
        ).pack(side="left")
        ttk.Button(cabecera, text="Renderizar PNG",
                    command=self.renderizar_lr0).pack(side="right", padx=2)
        ttk.Button(cabecera, text="Abrir externo (PNG)",
                    command=lambda: self._abrir_archivo(LR0_PNG_PATH)
                    ).pack(side="right", padx=2)
        ttk.Button(cabecera, text="Abrir externo (.dot)",
                    command=lambda: self._abrir_archivo(LR0_DOT_PATH)
                    ).pack(side="right", padx=2)

        contenedor = ttk.Frame(frame, style="Card.TFrame")
        contenedor.pack(fill="both", expand=True, padx=8, pady=(0, 8))

        self._lr0_canvas = tk.Canvas(
            contenedor, background=Tema.BG_PANEL,
            highlightthickness=0, borderwidth=0)
        sy = ttk.Scrollbar(contenedor, orient="vertical",
                            command=self._lr0_canvas.yview)
        sx = ttk.Scrollbar(contenedor, orient="horizontal",
                            command=self._lr0_canvas.xview)
        self._lr0_canvas.configure(yscrollcommand=sy.set,
                                     xscrollcommand=sx.set)
        self._lr0_canvas.grid(row=0, column=0, sticky="nsew")
        sy.grid(row=0, column=1, sticky="ns")
        sx.grid(row=1, column=0, sticky="ew")
        contenedor.rowconfigure(0, weight=1)
        contenedor.columnconfigure(0, weight=1)

        # Zoom con rueda
        self._lr0_zoom = 1.0
        self._lr0_canvas.bind("<MouseWheel>", self._lr0_on_wheel)
        self._lr0_canvas.bind("<Control-MouseWheel>", self._lr0_on_wheel_zoom)

        self._lr0_canvas.create_text(
            12, 12, anchor="nw",
            text=("(sin imagen) Ejecutá un análisis y presioná 'Renderizar "
                  "PNG'. Requiere Graphviz `dot` en el PATH."),
            font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE),
            fill=Tema.FG_MUTED, tags=("placeholder",),
        )

    def _construir_panel_parser_paso(self, notebook):
        frame = ttk.Frame(notebook, style="Card.TFrame")
        notebook.add(frame, text="Parser paso a paso")

        cabecera = ttk.Frame(frame, style="Card.TFrame", padding=(10, 10))
        cabecera.pack(side="top", fill="x")
        ttk.Label(cabecera, text="Parser:",
                   style="Card.TLabel").pack(side="left")
        self._parser_paso_var = tk.StringVar(value="LL(1)")
        combo = ttk.Combobox(
            cabecera, textvariable=self._parser_paso_var,
            values=("LL(1)", "SLR(1)", "LALR(1)"),
            state="readonly", width=10)
        combo.pack(side="left", padx=6)
        combo.bind("<<ComboboxSelected>>",
                    lambda _ev: self._refrescar_panel_parser_paso())

        contenedor = ttk.Frame(frame, style="Card.TFrame")
        contenedor.pack(fill="both", expand=True, padx=8, pady=(0, 8))
        self._parser_paso_text = tk.Text(
            contenedor, wrap="none",
            font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE),
            background=Tema.BG_PANEL, foreground=Tema.FG,
            insertbackground=Tema.FG,
            selectbackground=Tema.BG_ACTIVE,
            borderwidth=0, relief="flat",
            padx=12, pady=10, state="disabled")
        sy = ttk.Scrollbar(contenedor, orient="vertical",
                            command=self._parser_paso_text.yview)
        sx = ttk.Scrollbar(contenedor, orient="horizontal",
                            command=self._parser_paso_text.xview)
        self._parser_paso_text.configure(
            yscrollcommand=sy.set, xscrollcommand=sx.set)
        self._parser_paso_text.grid(row=0, column=0, sticky="nsew")
        sy.grid(row=0, column=1, sticky="ns")
        sx.grid(row=1, column=0, sticky="ew")
        contenedor.rowconfigure(0, weight=1)
        contenedor.columnconfigure(0, weight=1)
        frame._tab_titulo = "Parser paso a paso"
        self.paneles["parser_paso_tab"] = frame

    def _construir_barra_estado(self):
        barra = ttk.Frame(self.root, style="Status.TFrame", padding=0)
        barra.pack(side="bottom", fill="x")

        ttk.Separator(barra, orient="horizontal").pack(side="top", fill="x")

        contenido = ttk.Frame(barra, style="Status.TFrame", padding=(8, 4))
        contenido.pack(fill="x")

        self._lbl_pos     = ttk.Label(contenido, style="Status.TLabel",
                                       text="Línea 1, Col 1")
        self._lbl_tokens  = ttk.Label(contenido, style="Status.TLabel",
                                       text="Tokens: —")
        self._lbl_parser  = ttk.Label(contenido, style="Status.TLabel",
                                       text="Parser: —")
        self._lbl_estado  = ttk.Label(contenido, style="Status.TLabel",
                                       text="Estado: Listo")
        self._lbl_ej      = ttk.Label(
            contenido, style="Status.TLabel",
            text=f"Ejecutable: {obtener_ejecutable_real().name}")

        self._lbl_pos.pack(side="left", padx=(8, 12))
        ttk.Separator(contenido, orient="vertical").pack(
            side="left", fill="y", pady=2)
        self._lbl_tokens.pack(side="left", padx=12)
        ttk.Separator(contenido, orient="vertical").pack(
            side="left", fill="y", pady=2)
        self._lbl_parser.pack(side="left", padx=12)
        ttk.Separator(contenido, orient="vertical").pack(
            side="left", fill="y", pady=2)
        self._lbl_estado.pack(side="left", padx=12)
        self._lbl_ej.pack(side="right", padx=8)

    def _registrar_shortcuts(self):
        self.root.bind_all("<Control-s>", lambda _ev: self.guardar_activo())
        self.root.bind_all("<Control-S>", lambda _ev: self.guardar_todo())
        self.root.bind_all("<Control-Shift-S>",
                            lambda _ev: self.guardar_todo())
        self.root.bind_all("<Control-o>", lambda _ev: self.cargar_activo())
        self.root.bind_all("<F5>",       lambda _ev: self.compilar())
        self.root.bind_all("<F6>",       lambda _ev: self.ejecutar())
        self.root.bind_all("<Control-l>", lambda _ev: self.limpiar_salida())
        self.root.bind_all("<Control-f>", lambda _ev: self.abrir_buscar())

    # ── Helpers de UI ─────────────────────────────────────────────────

    def _enviar_evento(self, evento):
        try:
            widget = self.root.focus_get()
            if widget is not None:
                widget.event_generate(evento)
        except tk.TclError:
            pass

    def _set_toolbar(self, mensaje, color=None):
        self._toolbar_estado.configure(
            text=mensaje, foreground=color or Tema.ACCENT)
        self._lbl_estado.configure(text=f"Estado: {mensaje}")
        self.root.update_idletasks()

    def _actualizar_estado(self):
        if self._editor_activo is not None:
            l, c = self._editor_activo.linea_columna()
            self._lbl_pos.configure(text=f"Línea {l}, Col {c}")

        if self._ultimo_stdout:
            n = contar_tokens_filtrados(self._ultimo_stdout)
            self._lbl_tokens.configure(
                text=f"Tokens: {n}" if n is not None else "Tokens: —")
        else:
            self._lbl_tokens.configure(text="Tokens: —")

        if self._ultimo_veredictos:
            self._lbl_parser.configure(
                text=f"Parser: {parser_ganador(self._ultimo_veredictos)}")
        else:
            self._lbl_parser.configure(text="Parser: —")

    def _escribir(self, clave, texto):
        panel = self.paneles[clave]
        panel.configure(state="normal")
        panel.delete("1.0", tk.END)
        if texto:
            panel.insert("1.0", texto)
        else:
            placeholder = EMPTY_STATES.get(clave, "(vacio)")
            panel.insert("1.0", placeholder)
            try:
                panel.tag_add("muted", "1.0", "end")
            except tk.TclError:
                pass
        panel.configure(state="disabled")

    @staticmethod
    def _set_text_disabled(widget, texto):
        widget.configure(state="normal")
        widget.delete("1.0", tk.END)
        widget.insert("1.0", texto if texto else "(vacio)")
        widget.configure(state="disabled")

    # ── Sidebar Proyecto ──────────────────────────────────────────────

    def _on_proyecto_seleccion(self, _ev=None):
        sel = self._proyecto.selection()
        if not sel:
            return
        valores = self._proyecto.item(sel[0], "values")
        if not valores:
            return
        clave = valores[0]
        if clave in self._editores:
            self._editor_tabs.select(self._editor_tab_index[clave])

    def _on_proyecto_double(self, _ev=None):
        sel = self._proyecto.selection()
        if not sel:
            return
        valores = self._proyecto.item(sel[0], "values")
        if not valores:
            return
        clave = valores[0]
        if clave == "yalex":
            self.cargar_yalex()
        elif clave == "yapar":
            self.cargar_yapar()
        elif clave == "entrada":
            self.cargar_entrada()

    _ICONOS_PROYECTO = {
        "yalex":   "λ",
        "yapar":   "Σ",
        "entrada": "≡",
    }

    def _refrescar_proyecto(self):
        for clave, default in (("yalex",   "(sin .yal)"),
                                ("yapar",   "(sin .yapar)"),
                                ("entrada", "(sin entrada)")):
            iid = self._proyecto_items[clave]
            icono = self._ICONOS_PROYECTO[clave]
            ruta = self._editores[clave].ruta
            if ruta is None:
                self._proyecto.item(iid, text=f"{icono}  {default}")
            else:
                self._proyecto.item(iid, text=f"{icono}  {Path(ruta).name}")

    # ── Tabs de editor ────────────────────────────────────────────────

    def _on_editor_tab_changed(self, _ev=None):
        i = self._editor_tabs.index("current")
        for clave, idx in self._editor_tab_index.items():
            if idx == i:
                self._editor_activo = self._editores[clave]
                break
        self._actualizar_estado()

    def _on_editor_cursor(self, editor):
        self._editor_activo = editor
        self._actualizar_estado()

    def _on_editor_modificado(self, editor, modificado):
        """Actualiza el texto del tab para mostrar el indicador `•`."""
        for clave, ed in self._editores.items():
            if ed is editor:
                base = self._editor_tab_titulo_base[clave]
                etq = base + ("  •" if modificado else "")
                try:
                    self._editor_tabs.tab(
                        self._editor_tab_index[clave], text=etq)
                except tk.TclError:
                    pass
                break

    def _editor_actual_clave(self):
        for clave, ed in self._editores.items():
            if ed is self._editor_activo:
                return clave
        return None

    # ── Contraste para diálogos de archivo ───────────────────────────

    def _aplicar_estilo_dialogo_archivos(self):
        """Ajusta temporalmente los estilos ttk que usa tk_getOpenFile."""
        estilo = ttk.Style()
        respaldo = {
            "configure": {},
            "map": {},
        }
        for nombre, opciones in {
            "Treeview": ("background", "fieldbackground", "foreground"),
            "TEntry": ("fieldbackground", "foreground"),
        }.items():
            respaldo["configure"][nombre] = {
                op: estilo.lookup(nombre, f"-{op}") for op in opciones
            }

        for nombre, opcion in (
            ("Treeview", "background"),
            ("Treeview", "foreground"),
            ("TEntry", "selectbackground"),
            ("TEntry", "selectforeground"),
        ):
            respaldo["map"][(nombre, opcion)] = estilo.map(nombre,
                                                           query_opt=opcion)

        estilo.configure(
            "Treeview",
            background="#f5f7fa",
            fieldbackground="#f5f7fa",
            foreground="#18222d",
        )
        estilo.map(
            "Treeview",
            background=[("selected", Tema.ACCENT_DARK)],
            foreground=[("selected", "#ffffff")],
        )
        estilo.configure(
            "TEntry",
            fieldbackground="#ffffff",
            foreground="#18222d",
        )
        estilo.map(
            "TEntry",
            selectbackground=[("focus", Tema.ACCENT_DARK)],
            selectforeground=[("focus", "#ffffff")],
        )
        return respaldo

    def _restaurar_estilo_dialogo_archivos(self, respaldo):
        estilo = ttk.Style()
        for nombre, opciones in respaldo.get("configure", {}).items():
            estilo.configure(nombre, **opciones)
        for (nombre, opcion), valores in respaldo.get("map", {}).items():
            estilo.map(nombre, **{opcion: valores})

    def _dialogo_archivo(self, funcion, **kwargs):
        respaldo = self._aplicar_estilo_dialogo_archivos()
        try:
            return funcion(parent=self.root, **kwargs)
        finally:
            self._restaurar_estilo_dialogo_archivos(respaldo)

    # ── Cargar archivos ───────────────────────────────────────────────

    def cargar_yalex(self):
        self._cargar_dialogo(self._editores["yalex"], "Cargar archivo YALex")

    def cargar_yapar(self):
        self._cargar_dialogo(self._editores["yapar"], "Cargar archivo YAPar")

    def cargar_entrada(self):
        self._cargar_dialogo(self._editores["entrada"],
                              "Cargar archivo de entrada")

    def cargar_activo(self):
        clave = self._editor_actual_clave()
        if clave == "yalex":   self.cargar_yalex()
        elif clave == "yapar": self.cargar_yapar()
        else:                  self.cargar_entrada()

    def _cargar_dialogo(self, editor, titulo):
        ruta = self._dialogo_archivo(
            filedialog.askopenfilename,
            title=titulo,
            filetypes=editor.extensiones_dialogo,
            initialdir=str(INPUT_DIR if INPUT_DIR.exists() else PROJECT_ROOT),
        )
        if not ruta:
            return
        try:
            editor.cargar(ruta)
            self._set_toolbar(f"Cargado: {Path(ruta).name}", Tema.OK)
        except OSError as exc:
            self._reportar_error(f"No se pudo abrir {ruta}\n{exc}")
        self._refrescar_proyecto()
        # Cambiar a la tab del editor recién cargado
        for clave, ed in self._editores.items():
            if ed is editor:
                self._editor_tabs.select(self._editor_tab_index[clave])
                break

    # ── Guardar ───────────────────────────────────────────────────────

    def guardar_activo(self):
        clave = self._editor_actual_clave()
        if clave is None:
            return
        self._guardar_editor(clave)

    def guardar_como_activo(self):
        clave = self._editor_actual_clave()
        if clave is None:
            return
        editor = self._editores[clave]
        ruta = self._dialogo_archivo(
            filedialog.asksaveasfilename,
            title=f"Guardar como ({clave})",
            initialdir=str(INPUT_DIR if INPUT_DIR.exists() else PROJECT_ROOT),
            filetypes=editor.extensiones_dialogo)
        if not ruta:
            return
        try:
            editor.guardar(ruta)
            self._set_toolbar(f"Guardado como: {Path(ruta).name}", Tema.OK)
        except OSError as exc:
            self._reportar_error(f"No se pudo guardar\n{exc}")
        self._refrescar_proyecto()

    def _guardar_editor(self, clave):
        editor = self._editores[clave]
        if editor.vacio() and editor.ruta is None:
            return
        try:
            if editor.ruta is None:
                ruta = self._dialogo_archivo(
                    filedialog.asksaveasfilename,
                    title=f"Guardar como ({clave})",
                    initialdir=str(INPUT_DIR if INPUT_DIR.exists() else
                                    PROJECT_ROOT),
                    filetypes=editor.extensiones_dialogo)
                if not ruta:
                    return
                editor.guardar(ruta)
            else:
                editor.guardar()
            self._set_toolbar(f"Guardado: {editor.ruta.name}", Tema.OK)
        except OSError as exc:
            self._reportar_error(f"No se pudo guardar\n{exc}")
        self._refrescar_proyecto()

    def guardar_todo(self):
        for clave in self._editores:
            self._guardar_editor(clave)
        return "break"

    # ── Buscar (mínimo viable) ────────────────────────────────────────

    def abrir_buscar(self):
        ed = self._editor_activo
        if ed is None:
            return
        diag = tk.Toplevel(self.root)
        diag.title("Buscar")
        diag.configure(background=Tema.BG_PANEL)
        diag.transient(self.root)
        diag.geometry("+%d+%d" % (
            self.root.winfo_rootx() + 200,
            self.root.winfo_rooty() + 120))
        ttk.Label(diag, text="Texto:", style="Card.TLabel").pack(
            side="left", padx=8, pady=8)
        entry = ttk.Entry(diag, width=30)
        entry.pack(side="left", padx=4, pady=8)
        entry.focus_set()

        def buscar(_ev=None):
            patron = entry.get()
            if not patron:
                return
            ed.text.tag_remove("__find__", "1.0", tk.END)
            inicio = "1.0"
            count_var = tk.IntVar()
            primero = None
            while True:
                pos = ed.text.search(patron, inicio, stopindex=tk.END,
                                       count=count_var, nocase=True)
                if not pos:
                    break
                fin = f"{pos}+{count_var.get()}c"
                ed.text.tag_add("__find__", pos, fin)
                if primero is None:
                    primero = pos
                inicio = fin
            ed.text.tag_configure(
                "__find__",
                background=Tema.ACCENT_DARK,
                foreground=Tema.FG)
            if primero:
                ed.text.see(primero)
                ed.text.mark_set("insert", primero)
            diag.destroy()

        ttk.Button(diag, text="Buscar", command=buscar).pack(
            side="left", padx=8, pady=8)
        diag.bind("<Return>", buscar)
        diag.bind("<Escape>", lambda _ev: diag.destroy())

    # ── Compilar ──────────────────────────────────────────────────────

    def compilar(self):
        if self.proceso_activo:
            messagebox.showinfo("Compis IDE",
                                  "Ya hay un proceso en ejecucion.")
            return
        self.proceso_activo = True
        self._set_toolbar("Compilando…", Tema.WARN)
        self._iniciar_spinner()
        threading.Thread(target=self._compilar_thread, daemon=True).start()

    def _compilar_thread(self):
        try:
            proc = subprocess.run(
                COMANDO_COMPILACION,
                cwd=str(PROJECT_ROOT),
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
            )
        except FileNotFoundError as exc:
            self.root.after(0, lambda: self._fin_compilacion_fallida(
                f"No se pudo ejecutar g++.\n{exc}\n"
                "Verifica que g++ esté en el PATH "
                "(MSYS2 UCRT64 en Windows)."))
            return
        except OSError as exc:
            self.root.after(0, lambda: self._fin_compilacion_fallida(
                f"Error al ejecutar el compilador:\n{exc}"))
            return

        salida = "$ " + " ".join(COMANDO_COMPILACION) + "\n\n"
        salida += proc.stdout
        if proc.stderr:
            salida += "\n--- stderr ---\n" + proc.stderr
        self.root.after(0, lambda: self._fin_compilacion(
            proc.returncode, salida, proc.stderr))

    def _fin_compilacion(self, codigo, salida, stderr):
        self._detener_spinner()
        self._escribir("salida", salida)
        self._colorizar_consola(self.paneles["salida"])
        if codigo == 0 and obtener_ejecutable_real().exists():
            self._escribir("errores", "")
            self._set_toolbar("Compilación exitosa", Tema.OK)
            self._seleccionar_seccion("salida")
        else:
            self._escribir(
                "errores",
                f"Compilación falló (codigo {codigo}).\n\n"
                f"{stderr or salida}")
            self._colorizar_consola(self.paneles["errores"])
            self._seleccionar_seccion("errores")
            self._set_toolbar("Compilación con errores", Tema.ERR)
        self.proceso_activo = False

    def _fin_compilacion_fallida(self, mensaje):
        self._detener_spinner()
        self._reportar_error(mensaje)
        self.proceso_activo = False

    # ── Ejecutar análisis ─────────────────────────────────────────────

    def ejecutar(self):
        if self.proceso_activo:
            messagebox.showinfo("Compis IDE",
                                  "Ya hay un proceso en ejecucion.")
            return
        faltantes = [c for c, e in self._editores.items() if e.ruta is None]
        if faltantes:
            self._reportar_error(
                "Cargá los tres archivos antes de ejecutar:\n"
                "  • YALex (.yal)\n"
                "  • YAPar (.yapar)\n"
                "  • Entrada (.txt)\n"
                f"Faltan: {', '.join(faltantes)}")
            return
        for editor in self._editores.values():
            if not Path(editor.ruta).exists():
                self._reportar_error(
                    f"El archivo {editor.ruta} no existe en disco.\n"
                    "Guardá antes de ejecutar.")
                return
        ejecutable = obtener_ejecutable_real()
        if not ejecutable.exists():
            sugeridos = "\n  ".join(str(c) for c in _candidatos_ejecutable())
            self._reportar_error(
                f"No se encontró el ejecutable.\n\n"
                f"Buscado en:\n  {sugeridos}\n\n"
                "Presioná Compilar antes de ejecutar.")
            return
        try:
            for editor in self._editores.values():
                editor.guardar()
        except OSError as exc:
            self._reportar_error(
                f"No se pudieron guardar los cambios antes de ejecutar:\n"
                f"{exc}")
            return

        self.proceso_activo = True
        self._set_toolbar("Ejecutando análisis…", Tema.WARN)
        self._iniciar_spinner()
        threading.Thread(target=self._ejecutar_thread, daemon=True).start()

    def _ejecutar_thread(self):
        argv = [
            str(obtener_ejecutable_real()),
            str(self._editores["yalex"].ruta),
            str(self._editores["yapar"].ruta),
            str(self._editores["entrada"].ruta),
        ]
        try:
            proc = subprocess.run(
                argv, cwd=str(PROJECT_ROOT),
                capture_output=True, text=True,
                encoding="utf-8", errors="replace")
        except OSError as exc:
            self.root.after(0, lambda: self._fin_ejecucion_fallida(
                f"No se pudo ejecutar el compilador:\n{exc}"))
            return
        self.root.after(0, lambda: self._fin_ejecucion(proc))

    def _fin_ejecucion(self, proc):
        self._detener_spinner()
        stdout, stderr, codigo = proc.stdout, proc.stderr, proc.returncode
        self._ultimo_stdout = stdout
        self._ultimo_stderr = stderr
        self._ultimo_codigo = codigo

        salida_total = stdout
        if stderr:
            salida_total += "\n--- stderr ---\n" + stderr
        self._escribir("salida", salida_total)
        self._colorizar_consola(self.paneles["salida"])

        secciones = dividir_salida(stdout, stderr)
        for clave in ("tokens", "validaciones", "ff", "traza_ll1"):
            self._escribir(clave, secciones.get(clave, ""))

        # Versión texto de las tablas (raw)
        for clave in ("ll1", "slr1", "lalr1"):
            self._escribir(f"{clave}_text", secciones.get(clave, ""))

        # Vista cruda del LR(0) (texto)
        self._escribir("lr0", secciones.get("lr0", ""))

        # Errores estructurados
        self._renderizar_errores(secciones.get("errores", ""), stderr)

        # Veredictos
        veredictos = extraer_veredictos(stdout)
        self._ultimo_veredictos = veredictos
        bloque_resumen = secciones.get("resumen", "")
        cuerpo_resultado = formatear_veredictos(veredictos, codigo)
        if bloque_resumen:
            cuerpo_resultado += "\n\n" + bloque_resumen
        self._escribir("resultado", cuerpo_resultado)
        self._resaltar_veredictos(self.paneles["resultado"])

        # Tablas visuales LL/SLR/LALR
        self._cargar_tablas_json()

        # LR(0) embebido (Graphviz)
        self._cargar_lr0_dot()

        # Parser paso a paso
        self._refrescar_panel_parser_paso()

        # Conflictos en panel texto
        for clave in ("ll1_text", "slr1_text", "lalr1_text", "errores"):
            self._marcar_conflictos(self.paneles[clave])

        # Estado
        
        algun_aceptado  = any(v["resultado"].startswith("ACEPTADO")
                                for v in veredictos.values())
        algun_rechazado = any(v["resultado"] == "RECHAZADO"
                                for v in veredictos.values())
        if algun_aceptado and not algun_rechazado:
            self._set_toolbar("Análisis ACEPTADO", Tema.OK)
            self._seleccionar_seccion("resultado")
        elif algun_aceptado:
            self._set_toolbar(
                "Análisis ACEPTADO parcial — ver Resultado", Tema.WARN)
            self._seleccionar_seccion("resultado")
        elif algun_rechazado or codigo != 0:
            self._set_toolbar(
                f"Análisis RECHAZADO (codigo {codigo})", Tema.ERR)
            self._seleccionar_seccion("errores" if stderr or codigo != 0
                                       else "resultado")
        else:
            self._set_toolbar(f"Análisis terminó (codigo {codigo})")
            self._seleccionar_seccion("salida")

        self._actualizar_estado()
        self.proceso_activo = False

    def _fin_ejecucion_fallida(self, mensaje):
        self._detener_spinner()
        self._reportar_error(mensaje)
        self.proceso_activo = False

    # ── Coloreado de consola y conflictos ─────────────────────────────

    def _colorizar_consola(self, widget):
        widget.configure(state="normal")
        for tag in ("ok", "err", "warn", "info", "muted", "encabezado",
                     "conflicto_sr", "conflicto_rr",
                     "veredicto_ok", "veredicto_ko"):
            widget.tag_remove(tag, "1.0", tk.END)
        lineas = widget.get("1.0", "end-1c").splitlines()
        for idx, linea in enumerate(lineas, start=1):
            baja = linea.lower()
            stripped = linea.lstrip()
            tag = None
            if stripped.startswith("===") or stripped.startswith("FASE"):
                tag = "encabezado"
            elif "aceptado" in baja:
                tag = "ok"
            elif ("rechazado" in baja or stripped.upper().startswith("ERROR")
                  or "shift/reduce" in baja or "reduce/reduce" in baja):
                tag = "err"
            elif "advertencia" in baja or "warn" in baja:
                tag = "warn"
            elif (stripped.startswith("[LR0-DOT]")
                  or stripped.startswith("[TABLAS-JSON]")
                  or stripped.startswith("Tokens")
                  or stripped.startswith("Estados")):
                tag = "info"
            elif stripped.startswith("("):
                tag = "muted"
            if tag:
                widget.tag_add(tag, f"{idx}.0", f"{idx}.end")
        widget.configure(state="disabled")

    def _marcar_conflictos(self, widget):
        widget.configure(state="normal")
        widget.tag_remove("conflicto_sr", "1.0", tk.END)
        widget.tag_remove("conflicto_rr", "1.0", tk.END)
        lineas = widget.get("1.0", "end-1c").splitlines()
        for idx, linea in enumerate(lineas, start=1):
            baja = linea.lower()
            if "shift/reduce" in baja:
                widget.tag_add("conflicto_sr", f"{idx}.0", f"{idx}.end")
            elif "reduce/reduce" in baja:
                widget.tag_add("conflicto_rr", f"{idx}.0", f"{idx}.end")
        widget.configure(state="disabled")

    def _resaltar_veredictos(self, widget):
        widget.configure(state="normal")
        widget.tag_remove("veredicto_ok", "1.0", tk.END)
        widget.tag_remove("veredicto_ko", "1.0", tk.END)
        lineas = widget.get("1.0", "end-1c").splitlines()
        for idx, linea in enumerate(lineas, start=1):
            if "ACEPTADO" in linea:
                widget.tag_add("veredicto_ok", f"{idx}.0", f"{idx}.end")
            elif "RECHAZADO" in linea:
                widget.tag_add("veredicto_ko", f"{idx}.0", f"{idx}.end")
        widget.configure(state="disabled")

    def _renderizar_errores(self, contenido, stderr):
        widget = self.paneles["errores"]
        widget.configure(state="normal")
        widget.delete("1.0", tk.END)
        if not contenido and not stderr:
            widget.insert("1.0", "(sin errores)")
            widget.tag_add("muted", "1.0", "end")
            widget.configure(state="disabled")
            return

        widget.insert("end", "ERRORES Y CONFLICTOS\n", ("encabezado",))
        widget.insert("end", "─" * 60 + "\n\n", ("muted",))
        for linea in contenido.splitlines():
            tag = ("err",)
            baja = linea.lower()
            if "shift/reduce" in baja:
                tag = ("conflicto_sr",)
            elif "reduce/reduce" in baja:
                tag = ("conflicto_rr",)
            elif "advertencia" in baja:
                tag = ("warn",)
            widget.insert("end", linea + "\n", tag)
        if stderr:
            widget.insert("end", "\n--- stderr ---\n", ("muted",))
            widget.insert("end", stderr, ("err",))
        widget.configure(state="disabled")

    # ── Cargar tablas (JSON) ──────────────────────────────────────────

    def _cargar_tablas_json(self):
        if not TABLAS_JSON_PATH.exists():
            self._tabla_ll1.limpiar()
            self._tabla_slr1.limpiar()
            self._tabla_lalr1.limpiar()
            return
        try:
            datos = json.loads(TABLAS_JSON_PATH.read_text(
                encoding="utf-8", errors="replace"))
        except (OSError, json.JSONDecodeError):
            return
        self._tablas_json = datos

        # LL(1): conflictos por celda no se exportan al JSON (la tabla almacena
        # solo prod_idx por celda). Igual no resaltamos nada en LL.
        self._tabla_ll1.cargar_tabla_ll1(datos.get("ll1", {}), set())
        self._tabla_slr1.cargar_tabla_lr(datos.get("slr1", {}), "SLR(1)")
        self._tabla_lalr1.cargar_tabla_lr(datos.get("lalr1", {}), "LALR(1)")

    # ── LR(0): cargar y renderizar ────────────────────────────────────

    def _cargar_lr0_dot(self):
        canvas = self._lr0_canvas
        canvas.delete("placeholder")
        if (LR0_PNG_PATH.exists()
                and LR0_DOT_PATH.exists()
                and LR0_PNG_PATH.stat().st_mtime >=
                    LR0_DOT_PATH.stat().st_mtime):
            self._mostrar_lr0_png()
        else:
            canvas.delete("all")
            canvas.create_text(
                12, 12, anchor="nw",
                text=("Listo para renderizar. Presioná 'Renderizar PNG' o "
                      "Visualizar → Renderizar autómata LR(0)."),
                font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE),
                fill=Tema.FG_MUTED, tags=("placeholder",),
            )

    def renderizar_lr0(self):
        if not LR0_DOT_PATH.exists():
            self._set_toolbar(
                f"No existe {LR0_DOT_PATH.name}. Ejecutá un análisis primero.",
                Tema.WARN)
            return
        ruta_dot = shutil.which("dot")
        if ruta_dot is None:
            self._set_toolbar(
                "No se encontró `dot` (Graphviz) en el PATH.", Tema.WARN)
            return
        try:
            proc = subprocess.run(
                [ruta_dot, "-Tpng", str(LR0_DOT_PATH),
                 "-o", str(LR0_PNG_PATH)],
                capture_output=True, text=True,
                encoding="utf-8", errors="replace")
        except OSError as exc:
            self._set_toolbar(f"Error invocando dot: {exc}", Tema.ERR)
            return
        if proc.returncode != 0 or not LR0_PNG_PATH.exists():
            self._set_toolbar(
                f"dot falló (code {proc.returncode}).", Tema.ERR)
            return
        self._mostrar_lr0_png()
        self._set_toolbar(
            f"LR(0) renderizado: {LR0_PNG_PATH.name}", Tema.OK)

    def _mostrar_lr0_png(self):
        canvas = self._lr0_canvas
        canvas.delete("all")
        try:
            imagen = tk.PhotoImage(file=str(LR0_PNG_PATH))
        except tk.TclError as exc:
            canvas.create_text(
                12, 12, anchor="nw",
                text=("No se pudo cargar el PNG con Tk: " + str(exc) +
                      "\nProbá 'Abrir externo (PNG)'."),
                font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE),
                fill=Tema.ERR)
            self._lr0_imagen = None
            return
        self._lr0_imagen = imagen
        canvas.create_image(0, 0, image=imagen, anchor="nw")
        canvas.configure(
            scrollregion=(0, 0, imagen.width(), imagen.height()))

    def _lr0_on_wheel(self, ev):
        delta = -1 if ev.delta > 0 else 1
        self._lr0_canvas.yview_scroll(delta, "units")

    def _lr0_on_wheel_zoom(self, ev):
        # Zoom suave con Ctrl+rueda usando subsample/zoom
        if self._lr0_imagen is None:
            return
        # PhotoImage no soporta zoom continuo; aproximamos con zoom() en pasos.
        # Para evitar consumo enorme de memoria, lo dejamos como pista visual:
        del ev
        return

    # ── Parser paso a paso ────────────────────────────────────────────

    def _refrescar_panel_parser_paso(self):
        stdout = self._ultimo_stdout
        if not stdout:
            self._set_text_disabled(
                self._parser_paso_text,
                "(sin datos) Ejecutá un análisis para ver la traza.")
            return
        seleccion = self._parser_paso_var.get()
        secciones = dividir_salida(stdout, "")
        if seleccion == "LL(1)":
            cuerpo = secciones.get("traza_ll1", "")
        elif seleccion == "SLR(1)":
            cuerpo = self._extraer_evaluacion(stdout, "Evaluacion SLR(1)")
        else:
            cuerpo = self._extraer_evaluacion(stdout, "Evaluacion LALR(1)")
        if not cuerpo.strip():
            cuerpo = ("(no se encontró traza para " + seleccion +
                       " en la última corrida)")
        self._set_text_disabled(self._parser_paso_text, cuerpo)

    @staticmethod
    def _extraer_evaluacion(stdout, titulo_clave):
        for titulo, cuerpo in extraer_bloques(stdout):
            if titulo_clave in titulo:
                return f"=== {titulo} ===\n{cuerpo}".rstrip()
        return ""

    # ── Exportar tablas ───────────────────────────────────────────────

    def exportar_tablas(self):
        if not TABLAS_JSON_PATH.exists():
            messagebox.showinfo(
                "Compis IDE",
                "Aún no se generó output/tablas.json.\n"
                "Ejecutá un análisis y volvé a intentarlo.")
            return
        ruta = self._dialogo_archivo(
            filedialog.asksaveasfilename,
            title="Exportar tablas",
            defaultextension=".json",
            filetypes=(("JSON (todas las tablas)", "*.json"),
                        ("CSV (genera varios archivos)", "*.csv")),
            initialfile="tablas.json",
            initialdir=str(OUTPUT_DIR if OUTPUT_DIR.exists() else PROJECT_ROOT),
        )
        if not ruta:
            return
        destino = Path(ruta)
        try:
            datos = json.loads(TABLAS_JSON_PATH.read_text(
                encoding="utf-8", errors="replace"))
        except (OSError, json.JSONDecodeError) as exc:
            self._reportar_error(
                f"No se pudo leer {TABLAS_JSON_PATH}:\n{exc}")
            return
        try:
            if destino.suffix.lower() == ".csv":
                generados = _exportar_csv(datos, destino)
                self._set_toolbar(
                    f"Exportados {len(generados)} archivos CSV "
                    f"en {destino.parent}", Tema.OK)
            else:
                destino.write_text(
                    json.dumps(datos, ensure_ascii=False, indent=2),
                    encoding="utf-8")
                self._set_toolbar(f"Exportado JSON: {destino.name}", Tema.OK)
        except OSError as exc:
            self._reportar_error(f"No se pudo escribir en {destino}:\n{exc}")

    # ── Abrir externo ─────────────────────────────────────────────────

    def _abrir_archivo(self, ruta):
        ruta = Path(ruta)
        if not ruta.exists():
            self._set_toolbar(f"No existe: {ruta.name}", Tema.WARN)
            return
        try:
            # Windows nativo: startfile delega al visor por defecto.
            if os.name == "nt":
                os.startfile(str(ruta))  # type: ignore[attr-defined]
                return

            # WSL: tres estrategias por orden de robustez.
            if IS_WSL:
                # 1) wslview (paquete `wslu`): la opción más limpia.
                if shutil.which("wslview") is not None:
                    subprocess.run(["wslview", str(ruta)])
                    return
                # 2) explorer.exe + ruta Windows: funciona para abrir el
                #    archivo con su asociación en Windows (mejor para
                #    .png, .dot, .pdf...).
                ruta_win = ruta_windows_desde_wsl(ruta)
                if ruta_win is not None and shutil.which("explorer.exe"):
                    # explorer.exe siempre retorna 1 aunque tenga exito;
                    # ignoramos el código.
                    subprocess.run(["explorer.exe", ruta_win],
                                     check=False)
                    return
                # 3) Fallback: xdg-open (funciona si la distro tiene un
                #    handler X registrado).
                if shutil.which("xdg-open"):
                    subprocess.run(["xdg-open", str(ruta)])
                    return
                self._set_toolbar(
                    "Instalá `wslu` (wslview) o configurá xdg-open para "
                    "abrir archivos en WSL.", Tema.WARN)
                return

            # macOS
            if getattr(os, "uname", None) and os.uname().sysname == "Darwin":
                subprocess.run(["open", str(ruta)])
                return

            # Linux nativo
            subprocess.run(["xdg-open", str(ruta)])
        except OSError as exc:
            self._set_toolbar(f"No se pudo abrir {ruta.name}: {exc}",
                                Tema.ERR)

    # ── Limpieza y selección de pestañas ──────────────────────────────

    def limpiar_salida(self):
        for clave, panel in self.paneles.items():
            if isinstance(panel, tk.Text):
                self._escribir(clave, "")
        self._tabla_ll1.limpiar()
        self._tabla_slr1.limpiar()
        self._tabla_lalr1.limpiar()
        self._lr0_imagen = None
        self._lr0_canvas.delete("all")
        self._lr0_canvas.create_text(
            12, 12, anchor="nw",
            text="(sin imagen)",
            font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE),
            fill=Tema.FG_MUTED)
        self._set_text_disabled(self._parser_paso_text, "")
        self._ultimo_stdout = ""
        self._ultimo_stderr = ""
        self._ultimo_codigo = None
        self._ultimo_veredictos = None
        self._actualizar_estado()
        self._set_toolbar("Consola limpia")

    # Mapa de claves -> notebook donde vive, para selección rápida
    def _seleccionar_seccion(self, clave):
        """Selecciona la pestaña adecuada en el notebook que la contiene."""
        # Mapa especial para claves que no son simples paneles texto:
        mapeo_grupo = {
            "salida":      0, "tokens":      0, "validaciones": 0,
            "resultado":   0, "errores":     0,
            "ff":          1, "ll1":         1, "slr1":         1,
            "lalr1":       1, "traza_ll1":   1,
            "lr0":         2,
        }
        if clave not in mapeo_grupo:
            return
        self._grupos.select(mapeo_grupo[clave])

        # Buscar en el sub-notebook
        if clave in ("salida", "tokens", "validaciones",
                      "resultado", "errores"):
            panel = self.paneles[clave]
            nb = panel._notebook
            for i in range(len(nb.tabs())):
                if nb.tab(i, "text") == panel._tab_titulo:
                    nb.select(i)
                    return
        elif clave in ("ff", "traza_ll1"):
            panel = self.paneles[clave]
            nb = panel._notebook
            for i in range(len(nb.tabs())):
                if nb.tab(i, "text") == panel._tab_titulo:
                    nb.select(i)
                    return
        elif clave == "ll1":
            nb = self._tabla_ll1._notebook
            for i in range(len(nb.tabs())):
                if nb.tab(i, "text") == self._tabla_ll1._tab_titulo:
                    nb.select(i); return
        elif clave == "slr1":
            nb = self._tabla_slr1._notebook
            for i in range(len(nb.tabs())):
                if nb.tab(i, "text") == self._tabla_slr1._tab_titulo:
                    nb.select(i); return
        elif clave == "lalr1":
            nb = self._tabla_lalr1._notebook
            for i in range(len(nb.tabs())):
                if nb.tab(i, "text") == self._tabla_lalr1._tab_titulo:
                    nb.select(i); return
        elif clave == "lr0":
            # Primera tab del grupo 3 (Autómata LR(0))
            for sub in self._grupos.winfo_children():
                pass

    # ── Drag & drop de archivos ───────────────────────────────────────

    def _registrar_drag_and_drop(self):
        """Registra los objetivos de drop. Sin tkinterdnd2 deja un aviso."""
        if not DND_DISPONIBLE:
            self._set_toolbar(
                "Tip: `pip install tkinterdnd2` para arrastrar archivos",
                Tema.FG_MUTED)
            return

        objetivos = [self.root, self._proyecto, self._editor_tabs]
        for ed in self._editores.values():
            objetivos.append(ed.text)

        for w in objetivos:
            try:
                w.drop_target_register(DND_FILES)  # type: ignore[attr-defined]
                w.dnd_bind("<<Drop>>", self._on_drop)  # type: ignore[attr-defined]
                w.dnd_bind("<<DropEnter>>", self._on_drop_enter)  # type: ignore[attr-defined]
                w.dnd_bind("<<DropLeave>>", self._on_drop_leave)  # type: ignore[attr-defined]
            except (tk.TclError, AttributeError):
                # Algún widget puede no soportar drop_target en versiones
                # antiguas de Tcl/Tk; lo saltamos silenciosamente.
                continue

    def _on_drop_enter(self, _ev=None):
        try:
            self._toolbar.configure(style="Drop.TFrame")
        except tk.TclError:
            pass
        self._set_toolbar("⤓  Soltá los archivos para importarlos…",
                            Tema.WARN)

    def _on_drop_leave(self, _ev=None):
        try:
            self._toolbar.configure(style="Toolbar.TFrame")
        except tk.TclError:
            pass
        self._actualizar_estado()

    def _on_drop(self, event):
        """Maneja el evento de soltar uno o varios archivos sobre la IDE."""
        try:
            self._toolbar.configure(style="Toolbar.TFrame")
        except tk.TclError:
            pass
        try:
            paths = self.root.tk.splitlist(event.data)
        except (AttributeError, tk.TclError):
            paths = event.data.split()

        importados = []
        rechazados = []
        for raw in paths:
            normalizada = normalizar_ruta_arrastrada(raw)
            ruta = Path(normalizada)
            if not ruta.exists() or not ruta.is_file():
                rechazados.append(
                    f"{raw} -> {normalizada}" if normalizada != str(raw)
                    else str(raw))
                continue
            destino = self._clasificar_por_extension(ruta)
            try:
                self._editores[destino].cargar(ruta)
            except OSError as exc:
                rechazados.append(f"{ruta.name} ({exc})")
                continue
            importados.append((destino, ruta.name))

        self._refrescar_proyecto()

        if importados:
            # Foco al último importado
            ultima_clave = importados[-1][0]
            self._editor_tabs.select(self._editor_tab_index[ultima_clave])
            resumen = ", ".join(f"{n} → {c}" for c, n in importados)
            self._set_toolbar(f"Importado por DnD: {resumen}", Tema.OK)
        if rechazados:
            mensaje = (
                "No se pudieron importar:\n  " + "\n  ".join(rechazados))
            if not importados:
                self._reportar_error(mensaje)
            else:
                # Mantener notificación discreta en la barra
                self._lbl_estado.configure(
                    text="Estado: algunos archivos no se importaron")
        return event.action if hasattr(event, "action") else None

    def _clasificar_por_extension(self, ruta):
        """Decide a qué editor enviar un archivo según su extensión."""
        ext = ruta.suffix.lower()
        if ext in (".yal", ".yalex"):
            return "yalex"
        if ext in (".yapar", ".yalp", ".grammar"):
            return "yapar"
        # Si el archivo se llama exactamente como uno de los editores actuales
        # mantenemos la asignación.
        nombre = ruta.name.lower()
        if "yalex" in nombre or nombre.endswith(".yal"):
            return "yalex"
        if "yapar" in nombre or "grammar" in nombre:
            return "yapar"
        return "entrada"

    # ── Errores ───────────────────────────────────────────────────────

    def _reportar_error(self, mensaje):
        widget = self.paneles["errores"]
        self._set_text_disabled(widget, mensaje)
        self._seleccionar_seccion("errores")
        self._set_toolbar("Error", Tema.ERR)

    # ── Persistencia entre sesiones ───────────────────────────────────

    def _on_cerrar(self):
        try:
            self._guardar_estado()
        except Exception:  # pragma: no cover - cerrar nunca debe fallar
            pass
        # Cancelar callbacks `after` pendientes de los editores para evitar
        # "invalid command name" tras destroy().
        for ed in self._editores.values():
            if ed._resaltado_after is not None:
                try:
                    ed.text.after_cancel(ed._resaltado_after)
                except (tk.TclError, ValueError):
                    pass
                ed._resaltado_after = None
        self.root.destroy()

    def _guardar_estado(self):
        estado = {
            "geometria":   self.root.geometry(),
            "archivos": {
                clave: (str(ed.ruta) if ed.ruta else None)
                for clave, ed in self._editores.items()
            },
            "editor_tab_activo": self._editor_actual_clave(),
            "grupo_resultados":  self._grupos.index("current"),
        }
        try:
            ESTADO_IDE_PATH.write_text(
                json.dumps(estado, ensure_ascii=False, indent=2),
                encoding="utf-8")
        except OSError:
            return

    def _cargar_estado(self):
        if not ESTADO_IDE_PATH.exists():
            return
        try:
            datos = json.loads(
                ESTADO_IDE_PATH.read_text(encoding="utf-8",
                                            errors="replace"))
        except (OSError, json.JSONDecodeError):
            return

        # Geometría
        geo = datos.get("geometria")
        if isinstance(geo, str) and "x" in geo:
            try:
                self.root.geometry(geo)
            except tk.TclError:
                pass

        # Archivos
        archivos = datos.get("archivos") or {}
        cargados = []
        for clave, ruta in archivos.items():
            if clave not in self._editores or not ruta:
                continue
            p = Path(ruta)
            if not p.exists():
                continue
            try:
                self._editores[clave].cargar(p)
                cargados.append((clave, p.name))
            except OSError:
                continue

        # Refrescar sidebar y estado
        self._refrescar_proyecto()

        # Tab activo
        clave_activa = datos.get("editor_tab_activo")
        if clave_activa in self._editor_tab_index:
            try:
                self._editor_tabs.select(self._editor_tab_index[clave_activa])
            except tk.TclError:
                pass

        # Grupo de resultados
        grupo = datos.get("grupo_resultados")
        if isinstance(grupo, int):
            try:
                self._grupos.select(grupo)
            except tk.TclError:
                pass

        if cargados:
            resumen = ", ".join(f"{c}={n}" for c, n in cargados)
            self._set_toolbar(f"Sesión restaurada: {resumen}", Tema.OK)
            self._actualizar_estado()

    # ── Ayuda ─────────────────────────────────────────────────────────

    def _mostrar_atajos(self):
        messagebox.showinfo(
            "Atajos",
            "Ctrl+S          Guardar archivo activo\n"
            "Ctrl+Shift+S    Guardar todo\n"
            "Ctrl+O          Cargar (en el tab activo)\n"
            "Ctrl+L          Limpiar consola\n"
            "Ctrl+F          Buscar en el editor\n"
            "F5              Compilar\n"
            "F6              Ejecutar análisis"
        )

    def _mostrar_acerca(self):
        messagebox.showinfo(
            "Acerca de",
            "Compis IDE\n"
            "Pipeline: YALex + YAPar + LL(1) + LR(0) + SLR(1) + LALR(1)\n"
            "Diseño de Lenguajes — UVG"
        )


# ──────────────────────────────────────────────────────────────────────
# Exportación CSV (compartida con la lógica del back vía tablas.json)
# ──────────────────────────────────────────────────────────────────────

def _escribir_csv(ruta, encabezado, filas):
    with open(ruta, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(encabezado)
        for fila in filas:
            w.writerow(fila)


def _exportar_csv(datos, destino_base):
    destino_base = Path(destino_base)
    carpeta = destino_base.parent
    base = destino_base.stem
    generados = []

    for clave in ("first", "follow"):
        ff = datos.get(clave, {})
        if not isinstance(ff, dict):
            continue
        ruta = carpeta / f"{base}_{clave}.csv"
        filas = [(nt, ",".join(ts)) for nt, ts in ff.items()]
        _escribir_csv(ruta, ("no_terminal", clave), filas)
        generados.append(ruta)

    ll1 = datos.get("ll1", {})
    if isinstance(ll1, dict) and "filas" in ll1:
        terms = ll1.get("terminales", []) or []
        ruta = carpeta / f"{base}_ll1.csv"
        filas = []
        for nt, fila in ll1["filas"].items():
            filas.append([nt] + [str(fila.get(t, "")) for t in terms])
        _escribir_csv(ruta, ["NT"] + list(terms), filas)
        generados.append(ruta)

    for clave in ("slr1", "lalr1"):
        tab = datos.get(clave, {})
        if not isinstance(tab, dict) or "action" not in tab:
            continue
        terms = tab.get("terminales", []) or []
        nterms = tab.get("no_terminales", []) or []
        ruta_action = carpeta / f"{base}_{clave}_action.csv"
        filas = []
        for i, fila in enumerate(tab["action"]):
            filas.append([i] + [fila.get(t, "") for t in terms])
        _escribir_csv(ruta_action, ["estado"] + list(terms), filas)
        generados.append(ruta_action)

        ruta_goto = carpeta / f"{base}_{clave}_goto.csv"
        filas = []
        for i, fila in enumerate(tab.get("goto", [])):
            filas.append([i] + [fila.get(nt, "") for nt in nterms])
        _escribir_csv(ruta_goto, ["estado"] + list(nterms), filas)
        generados.append(ruta_goto)

        conflictos = tab.get("conflictos", []) or []
        if conflictos:
            ruta_conf = carpeta / f"{base}_{clave}_conflictos.csv"
            filas = [(c.get("estado", ""), c.get("terminal", ""),
                       c.get("descripcion", "")) for c in conflictos]
            _escribir_csv(ruta_conf,
                            ("estado", "terminal", "descripcion"),
                            filas)
            generados.append(ruta_conf)

    return generados


# ──────────────────────────────────────────────────────────────────────
# Entrada
# ──────────────────────────────────────────────────────────────────────

def main():
    if DND_DISPONIBLE and TkinterDnD is not None:
        root = TkinterDnD.Tk()
    else:
        root = tk.Tk()
    IDE(root)
    root.mainloop()


if __name__ == "__main__":
    main()
