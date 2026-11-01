"""IDE del interprete SQL (`sqlint`), Construccion de Compiladores, UVG 2026.

La IDE NO reimplementa nada del interprete: solo habla con el binario
`sqlint` mediante `--json` y `--emit` (contrato A.7 de docs/sql) y reparte
lo que devuelve en pestanas. Nunca lee los CSV de la base directamente.

    Editor SQL  ->  sqlint --json                 -> Resultados, Plan, Errores
                    sqlint --json --emit tokens   -> Tokens
                    sqlint --json --emit parse-tree / ast-dot / ast
                                                  -> Arbol sintactico, AST (Graphviz)
                    sqlint --json --emit catalog  -> panel Catalogo

Por defecto se ejecuta sobre una COPIA de la base elegida (en
output/ide/bases/), para que probar consultas no modifique los archivos de
`databases/escuela`, que estan versionados. La copia se reinicia desde el
menu Base de datos.

Partes reutilizadas de la IDE de Compiscript (ide/ide_app.py en `main`):
la deteccion de WSL, el editor con numeros de linea, el tema oscuro, el
arrastrar y soltar (opcional, con tkinterdnd2) y el estado persistente.

Uso (desde la raiz del repo, en Linux o WSL con python3-tk):
    make            # compila sqlint
    python3 ide/sql_ide.py
"""

from __future__ import annotations

import json
import os
import queue
import re
import shutil
import subprocess
import threading
import tkinter as tk
import tkinter.font as tkfont
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

# Arrastrar y soltar: dependencia opcional. Sin ella la IDE funciona igual.
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

PROJECT_ROOT    = Path(__file__).resolve().parent.parent
SQLINT_PATH     = PROJECT_ROOT / "sqlint"
DB_POR_DEFECTO  = PROJECT_ROOT / "databases" / "escuela"
EJEMPLOS_DIR    = PROJECT_ROOT / "ide" / "ejemplos"
TRABAJO_DIR     = PROJECT_ROOT / "output" / "ide"      # gitignoreado (output/)
COPIAS_DIR      = TRABAJO_DIR / "bases"
ESTADO_IDE_PATH = PROJECT_ROOT / ".sql_ide_state.json"  # gitignoreado

COMANDO_COMPILACION = ["make"]


def _detectar_wsl():
    """True si la IDE corre dentro de WSL/WSL2."""
    if os.name == "nt":
        return False
    if os.environ.get("WSL_DISTRO_NAME") or os.environ.get("WSL_INTEROP"):
        return True
    try:
        with open("/proc/version", "r", encoding="utf-8", errors="replace") as f:
            return "microsoft" in f.read().lower()
    except OSError:
        return False


IS_WSL = _detectar_wsl()
_PATRON_DRIVE_LETTER = re.compile(r"^[A-Za-z]:[\\/]")
_PATRON_UNC_WSL = re.compile(r"^\\\\wsl(?:\.localhost|\$)[\\/]", re.IGNORECASE)


def normalizar_ruta_arrastrada(raw):
    """Limpia una ruta soltada sobre la ventana y la lleva al formato del SO
    actual (en WSL, C:\\x -> /mnt/c/x y \\\\wsl.localhost\\Distro\\x -> /x)."""
    s = str(raw).strip().strip("{}").strip('"').strip("'")
    if not s or not IS_WSL:
        return s
    if _PATRON_DRIVE_LETTER.match(s) or _PATRON_UNC_WSL.match(s):
        wp = shutil.which("wslpath")
        if wp is not None:
            try:
                proc = subprocess.run([wp, "-u", s], capture_output=True,
                                      text=True, timeout=2)
                if proc.returncode == 0 and proc.stdout.strip():
                    return proc.stdout.strip()
            except (OSError, subprocess.TimeoutExpired):
                pass
        if _PATRON_DRIVE_LETTER.match(s):
            resto = s[2:].replace("\\", "/")
            return f"/mnt/{s[0].lower()}{resto if resto.startswith('/') else '/' + resto}"
        partes = s.replace("\\", "/").split("/")
        try:
            i = next(i for i, p in enumerate(partes) if p.lower().startswith("wsl"))
            return "/" + "/".join(partes[i + 2:])
        except (StopIteration, IndexError):
            pass
    return s


def ruta_para_sqlint(p):
    """Ruta tal como la tiene que recibir sqlint. Dentro del repo se pasa
    relativa (sqlint corre con cwd = raiz), lo que ademas funciona igual si
    la IDE corre en Python de Windows y sqlint en WSL."""
    p = Path(p).resolve()
    try:
        return p.relative_to(PROJECT_ROOT).as_posix()
    except ValueError:
        pass
    if os.name == "nt":
        try:
            proc = subprocess.run(["wsl", "wslpath", "-u", str(p)],
                                  capture_output=True, text=True, timeout=5)
            if proc.returncode == 0 and proc.stdout.strip():
                return proc.stdout.strip()
        except (OSError, subprocess.TimeoutExpired):
            pass
    return str(p)


def comando_sqlint(args):
    """sqlint es un binario de Linux: en Python de Windows se llama via wsl."""
    if os.name == "nt":
        return ["wsl", "./sqlint", *args]
    return [str(SQLINT_PATH), *args]


def sqlint_disponible():
    return SQLINT_PATH.exists()


def correr_sqlint(args, timeout=60):
    """Corre sqlint con cwd = raiz del repo. Devuelve (codigo, stdout, stderr).
    Codigo de sqlint: 0 sin diagnosticos, 1 con diagnosticos, 2 mal uso."""
    proc = subprocess.run(comando_sqlint(args), cwd=str(PROJECT_ROOT),
                          capture_output=True, text=True, encoding="utf-8",
                          errors="replace", timeout=timeout)
    return proc.returncode, proc.stdout, proc.stderr


def correr_sqlint_json(args, timeout=60):
    """Igual que correr_sqlint, con --json, y parsea la salida. Devuelve
    (datos, error): datos es el dict del JSON, o None si no se pudo parsear
    (en ese caso error explica que paso)."""
    codigo, out, err = correr_sqlint(["--json", *args], timeout)
    try:
        return json.loads(out), None
    except json.JSONDecodeError:
        detalle = (err or out or "").strip()
        return None, f"sqlint terminó con código {codigo} sin JSON válido.\n{detalle}"


# ──────────────────────────────────────────────────────────────────────
# Tema oscuro y fuentes (los mismos de la IDE de Compiscript)
# ──────────────────────────────────────────────────────────────────────

class Tema:
    BG          = "#17191d"
    BG_PANEL    = "#20242a"
    BG_PANEL_2  = "#282d35"
    BG_BUTTON   = "#2f353d"
    BG_BUTTON_H = "#3a424c"
    BG_ACTIVE   = "#183a52"
    BORDE_SOFT  = "#1f2329"

    FG          = "#edf2f7"
    FG_MUTED    = "#97a3b6"

    ACCENT      = "#5fb3f3"
    ACCENT_DARK = "#145b86"

    OK          = "#7ad27f"
    WARN        = "#f2d06b"
    ERR         = "#ff8f7a"
    ERR_BG      = "#5a1f1f"

    CURRENT_LINE  = "#222831"
    MATCH_BRACKET = "#305978"
    DND_HINT      = "#145b86"

    SYN_KEYWORD = "#569cd6"
    SYN_TYPE    = "#c586c0"
    SYN_FUNC    = "#4ec9b0"
    SYN_STRING  = "#ce9178"
    SYN_COMMENT = "#6a9955"
    SYN_NUMBER  = "#b5cea8"
    SYN_OP      = "#dcdcaa"


_KW = (r"SELECT|FROM|WHERE|INSERT|INTO|VALUES|UPDATE|SET|DELETE|CREATE|DROP|"
       r"TABLE|INDEX|ON|AS|AND|OR|NOT|IS|IN|EXISTS|INNER|JOIN|GROUP|BY|"
       r"HAVING|ORDER|ASC|DESC|PRIMARY|KEY|DISTINCT")
# Orden: comentarios y cadenas primero (tienen prioridad sobre lo demas).
PATRONES_SQL = [
    ("com",  re.compile(r"/\*[\s\S]*?\*/")),
    ("com",  re.compile(r"--[^\n]*")),
    ("str",  re.compile(r"'(?:''|[^'])*'?")),
    ("kw",   re.compile(rf"\b(?:{_KW})\b", re.IGNORECASE)),
    ("type", re.compile(r"\b(?:INT|FLOAT|VARCHAR|BOOLEAN|DATE|TRUE|FALSE|NULL)\b",
                        re.IGNORECASE)),
    ("func", re.compile(r"\b(?:COUNT|SUM|AVG|MIN|MAX)\b", re.IGNORECASE)),
    ("num",  re.compile(r"(?<![\w.])(?:\d+\.\d*|\.\d+|\d+)")),
    ("op",   re.compile(r"[(),;.*=<>!+\-/%]")),
]

FUENTE_MONO_NAME = "TkFixedFont"
FUENTE_MONO_SIZE = 11
FUENTE_UI_NAME   = "Segoe UI"
FUENTE_UI_SIZE   = 9


def _resolver_fuente(prefer, por_defecto):
    disponibles = {f.lower() for f in tkfont.families()}
    for nombre in prefer:
        if nombre.lower() in disponibles:
            return nombre
    return por_defecto


# ──────────────────────────────────────────────────────────────────────
# Editor con numeros de linea y resaltado SQL
# ──────────────────────────────────────────────────────────────────────

class EditorSQL(ttk.Frame):
    """Text oscuro con gutter de numeros de linea, resaltado de SQL, linea
    actual, pareo de parentesis y marcas de error subrayadas."""

    def __init__(self, master, on_cursor_change=None, on_modificado_change=None):
        super().__init__(master, style="Editor.TFrame")
        self._on_cursor_change = on_cursor_change
        self._on_modificado_change = on_modificado_change
        self.ruta = None
        self._modificado = False
        self._cargando = False
        self._resaltado_after = None

        self._gutter = tk.Canvas(self, width=46, highlightthickness=0,
                                 background=Tema.BG_PANEL_2, borderwidth=0)
        self._gutter.grid(row=0, column=0, sticky="ns")

        mono = (FUENTE_MONO_NAME, FUENTE_MONO_SIZE)
        self.text = tk.Text(
            self, wrap="none", undo=True, font=mono,
            background=Tema.BG, foreground=Tema.FG, insertbackground=Tema.FG,
            selectbackground=Tema.BG_ACTIVE, selectforeground=Tema.FG,
            borderwidth=0, relief="flat", padx=12, pady=10,
            tabs=("4c",), tabstyle="wordprocessor")
        self.text.grid(row=0, column=1, sticky="nsew")

        self._scroll_y = ttk.Scrollbar(self, orient="vertical", command=self._yview)
        self._scroll_y.grid(row=0, column=2, sticky="ns")
        scroll_x = ttk.Scrollbar(self, orient="horizontal", command=self.text.xview)
        scroll_x.grid(row=1, column=1, sticky="ew")
        self.text.configure(yscrollcommand=self._on_yscroll, xscrollcommand=scroll_x.set)
        self.rowconfigure(0, weight=1)
        self.columnconfigure(1, weight=1)

        for ev in ("<KeyRelease>", "<MouseWheel>", "<Button-4>", "<Button-5>",
                   "<ButtonRelease-1>", "<Configure>", "<<Modified>>"):
            self.text.bind(ev, self._on_evento, add="+")
        self.text.bind("<Return>", self._on_return, add="+")

        bold = (FUENTE_MONO_NAME, FUENTE_MONO_SIZE, "bold")
        self.text.tag_configure("syn_kw", foreground=Tema.SYN_KEYWORD, font=bold)
        self.text.tag_configure("syn_type", foreground=Tema.SYN_TYPE)
        self.text.tag_configure("syn_func", foreground=Tema.SYN_FUNC)
        self.text.tag_configure("syn_str", foreground=Tema.SYN_STRING)
        self.text.tag_configure("syn_com", foreground=Tema.SYN_COMMENT,
                                font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE, "italic"))
        self.text.tag_configure("syn_num", foreground=Tema.SYN_NUMBER)
        self.text.tag_configure("syn_op", foreground=Tema.SYN_OP)
        self.text.tag_configure("current_line", background=Tema.CURRENT_LINE)
        self.text.tag_configure("match_bracket", background=Tema.MATCH_BRACKET)
        # Errores: subrayado rojo en cada diagnostico; el que se eligio con
        # doble clic ademas con fondo.
        self.text.tag_configure("err_mark", underline=True, foreground=Tema.ERR)
        self.text.tag_configure("err_focus", background=Tema.ERR_BG,
                                underline=True, foreground=Tema.ERR)
        self.text.tag_lower("current_line")
        self.text.tag_raise("err_mark")
        self.text.tag_raise("err_focus")
        self.text.tag_raise("sel")

    # ── Scroll y gutter ───────────────────────────────────────────────

    def _yview(self, *args):
        self.text.yview(*args)
        self._redibujar_gutter()

    def _on_yscroll(self, first, last):
        self._scroll_y.set(first, last)
        self._redibujar_gutter()

    def _redibujar_gutter(self):
        c = self._gutter
        c.delete("all")
        try:
            primera = int(self.text.index("@0,0").split(".")[0])
            ultima = int(self.text.index(f"@0,{self.text.winfo_height()}").split(".")[0])
            total = int(self.text.index("end-1c").split(".")[0])
        except (tk.TclError, ValueError):
            return
        ancho = max(46, 12 + len(str(max(total, 1))) * 9)
        if int(c["width"]) != ancho:
            c.configure(width=ancho)
        for i in range(primera, min(ultima, total) + 1):
            bbox = self.text.bbox(f"{i}.0")
            if bbox:
                c.create_text(ancho - 6, bbox[1] + 1, anchor="ne", text=str(i),
                              fill=Tema.FG_MUTED,
                              font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE - 1))

    # ── Eventos ───────────────────────────────────────────────────────

    def _on_evento(self, _ev=None):
        self._redibujar_gutter()
        self._resaltar_linea_actual()
        self._resaltar_parentesis()
        self._programar_resaltado()
        try:
            modificado = bool(self.text.edit_modified())
        except tk.TclError:
            modificado = False
        if modificado and not self._cargando:
            # Editar invalida las marcas de error de la corrida anterior.
            self.limpiar_errores()
            self._marcar_modificado(True)
        if self._on_cursor_change is not None:
            self._on_cursor_change(self)
        try:
            self.text.edit_modified(False)
        except tk.TclError:
            pass

    def _on_return(self, _ev=None):
        linea = self.text.get("insert linestart", "insert")
        indent = linea[:len(linea) - len(linea.lstrip(" \t"))]
        self.text.insert("insert", "\n" + indent)
        self.text.see("insert")
        self._redibujar_gutter()
        return "break"

    def _resaltar_linea_actual(self):
        self.text.tag_remove("current_line", "1.0", "end")
        self.text.tag_add("current_line", "insert linestart", "insert lineend +1c")

    def _resaltar_parentesis(self):
        t = self.text
        t.tag_remove("match_bracket", "1.0", "end")
        cursor = t.index("insert")
        previo = t.get(f"{cursor} -1c") if cursor != "1.0" else ""
        actual = t.get(cursor)
        if previo in "()" and previo:
            idx, ch = f"{cursor} -1c", previo
        elif actual in "()" and actual:
            idx, ch = cursor, actual
        else:
            return
        pareja, paso = (")", 1) if ch == "(" else ("(", -1)
        contenido = t.get("1.0", "end-1c")
        i = len(t.get("1.0", idx))
        nivel = 0
        while 0 <= i < len(contenido):
            if contenido[i] == ch:
                nivel += 1
            elif contenido[i] == pareja:
                nivel -= 1
                if nivel == 0:
                    t.tag_add("match_bracket", idx, f"{idx} +1c")
                    t.tag_add("match_bracket", f"1.0 + {i} chars", f"1.0 + {i + 1} chars")
                    return
            i += paso

    # ── Resaltado de sintaxis (con debounce) ──────────────────────────

    def _programar_resaltado(self, retraso_ms=150):
        if self._resaltado_after is not None:
            try:
                self.text.after_cancel(self._resaltado_after)
            except (tk.TclError, ValueError):
                pass
        self._resaltado_after = self.text.after(retraso_ms, self.resaltar)

    def resaltar(self):
        self._resaltado_after = None
        for tag in ("syn_kw", "syn_type", "syn_func", "syn_str", "syn_com",
                    "syn_num", "syn_op"):
            self.text.tag_remove(tag, "1.0", "end")
        contenido = self.text.get("1.0", "end-1c")
        ocupados = []

        def libre(a, b):
            return all(b <= s or a >= e for s, e in ocupados)

        for prioritario in (True, False):
            for nombre, patron in PATRONES_SQL:
                if (nombre in ("com", "str")) != prioritario:
                    continue
                for m in patron.finditer(contenido):
                    a, b = m.span()
                    if a == b or not libre(a, b):
                        continue
                    self.text.tag_add(f"syn_{nombre}", f"1.0 + {a} chars", f"1.0 + {b} chars")
                    if prioritario:
                        ocupados.append((a, b))

    # ── Marcas de error ───────────────────────────────────────────────

    def limpiar_errores(self):
        self.text.tag_remove("err_mark", "1.0", "end")
        self.text.tag_remove("err_focus", "1.0", "end")

    def marcar_error(self, linea, columna, largo, foco=False):
        """Subraya `largo` caracteres desde (linea, columna), 1-based."""
        inicio = f"{max(linea, 1)}.{max(columna - 1, 0)}"
        fin = f"{inicio} + {max(largo, 1)} chars"
        # Si el error esta al final de la linea (p. ej. falta algo), se marca
        # el ultimo caracter para que se vea.
        if self.text.compare(inicio, ">=", f"{inicio} lineend"):
            inicio, fin = f"{inicio} -1c", inicio
        self.text.tag_add("err_focus" if foco else "err_mark", inicio, fin)

    def ir_a(self, linea, columna):
        idx = f"{max(linea, 1)}.{max(columna - 1, 0)}"
        self.text.tag_remove("err_focus", "1.0", "end")
        self.text.mark_set("insert", idx)
        self.text.see(idx)
        self.text.focus_set()
        self._on_evento()

    # ── Archivos ──────────────────────────────────────────────────────

    def contenido(self):
        return self.text.get("1.0", "end-1c")

    def poner_texto(self, contenido, ruta=None):
        self._cargando = True
        self.text.delete("1.0", "end")
        self.text.insert("1.0", contenido)
        self.text.edit_reset()
        self.text.edit_modified(False)
        self._cargando = False
        self.ruta = Path(ruta) if ruta else None
        self.limpiar_errores()
        self._marcar_modificado(False)
        self.resaltar()
        self.text.mark_set("insert", "1.0")
        self._on_evento()

    def cargar(self, ruta):
        self.poner_texto(Path(ruta).read_text(encoding="utf-8", errors="replace"), ruta)

    def guardar(self, ruta=None):
        if ruta is not None:
            self.ruta = Path(ruta)
        self.ruta.write_text(self.contenido(), encoding="utf-8")
        self._marcar_modificado(False)

    @property
    def modificado(self):
        return self._modificado

    def _marcar_modificado(self, valor):
        if valor != self._modificado:
            self._modificado = valor
            if self._on_modificado_change is not None:
                self._on_modificado_change(self)

    def linea_columna(self):
        linea, col = self.text.index("insert").split(".")
        return int(linea), int(col) + 1

    def seleccion(self):
        """(texto, linea_inicio, columna_inicio) de la seleccion, o None."""
        try:
            inicio = self.text.index("sel.first")
            texto = self.text.get("sel.first", "sel.last")
        except tk.TclError:
            return None
        if not texto.strip():
            return None
        linea, col = inicio.split(".")
        return texto, int(linea), int(col)


# ──────────────────────────────────────────────────────────────────────
# Widgets auxiliares
# ──────────────────────────────────────────────────────────────────────

def tabla(master, columnas, show="headings"):
    """Treeview con scrollbars. `columnas`: [(id, titulo, ancho, estira)]."""
    frame = ttk.Frame(master, style="Card.TFrame")
    tree = ttk.Treeview(frame, show=show, style="Tabla.Treeview",
                        columns=tuple(c[0] for c in columnas))
    sy = ttk.Scrollbar(frame, orient="vertical", command=tree.yview)
    sx = ttk.Scrollbar(frame, orient="horizontal", command=tree.xview)
    tree.configure(yscrollcommand=sy.set, xscrollcommand=sx.set)
    tree.grid(row=0, column=0, sticky="nsew")
    sy.grid(row=0, column=1, sticky="ns")
    sx.grid(row=1, column=0, sticky="ew")
    frame.rowconfigure(0, weight=1)
    frame.columnconfigure(0, weight=1)
    for cid, titulo, ancho, estira in columnas:
        tree.heading(cid, text=titulo)
        tree.column(cid, width=ancho, minwidth=40, anchor="w", stretch=estira)
    return frame, tree


def texto_solo_lectura(master):
    frame = ttk.Frame(master, style="Card.TFrame")
    t = tk.Text(frame, wrap="none", state="disabled",
                font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE - 1),
                background=Tema.BG_PANEL, foreground=Tema.FG,
                borderwidth=0, padx=12, pady=10, insertbackground=Tema.FG)
    sy = ttk.Scrollbar(frame, orient="vertical", command=t.yview)
    sx = ttk.Scrollbar(frame, orient="horizontal", command=t.xview)
    t.configure(yscrollcommand=sy.set, xscrollcommand=sx.set)
    t.grid(row=0, column=0, sticky="nsew")
    sy.grid(row=0, column=1, sticky="ns")
    sx.grid(row=1, column=0, sticky="ew")
    frame.rowconfigure(0, weight=1)
    frame.columnconfigure(0, weight=1)
    return frame, t


def poner_texto(widget, contenido):
    widget.configure(state="normal")
    widget.delete("1.0", "end")
    widget.insert("1.0", contenido)
    widget.configure(state="disabled")


class VisorGrafo(ttk.Frame):
    """Muestra un .dot renderizado con Graphviz como PNG en un canvas con
    scroll y zoom. Sin Graphviz (o si dot falla) muestra un texto alterno."""

    ZOOMS = (("25%", 4, 1), ("33%", 3, 1), ("50%", 2, 1), ("100%", 1, 1), ("200%", 1, 2))

    def __init__(self, master, nombre):
        super().__init__(master, style="Card.TFrame")
        self._nombre = nombre
        self._original = None
        self._mostrada = None
        self._zoom = 3

        barra = ttk.Frame(self, style="Card.TFrame")
        barra.pack(fill="x", padx=8, pady=(6, 0))
        ttk.Button(barra, text="−", width=3, command=lambda: self._cambiar_zoom(-1)).pack(side="left")
        ttk.Button(barra, text="+", width=3, command=lambda: self._cambiar_zoom(1)).pack(side="left", padx=(4, 0))
        self._lbl_zoom = ttk.Label(barra, text="100%", style="Card.TLabel")
        self._lbl_zoom.pack(side="left", padx=8)
        self._lbl_info = ttk.Label(barra, text="", style="Muted.TLabel")
        self._lbl_info.pack(side="left", padx=8)

        self._stack = ttk.Frame(self, style="Card.TFrame")
        self._stack.pack(fill="both", expand=True)

        self._canvas_frame = ttk.Frame(self._stack, style="Card.TFrame")
        self._canvas = tk.Canvas(self._canvas_frame, background="#ffffff", highlightthickness=0)
        sy = ttk.Scrollbar(self._canvas_frame, orient="vertical", command=self._canvas.yview)
        sx = ttk.Scrollbar(self._canvas_frame, orient="horizontal", command=self._canvas.xview)
        self._canvas.configure(yscrollcommand=sy.set, xscrollcommand=sx.set)
        self._canvas.grid(row=0, column=0, sticky="nsew")
        sy.grid(row=0, column=1, sticky="ns")
        sx.grid(row=1, column=0, sticky="ew")
        self._canvas_frame.rowconfigure(0, weight=1)
        self._canvas_frame.columnconfigure(0, weight=1)
        self._canvas.bind("<MouseWheel>", lambda e: self._canvas.yview_scroll(-1 if e.delta > 0 else 1, "units"))
        self._canvas.bind("<Button-4>", lambda e: self._canvas.yview_scroll(-1, "units"))
        self._canvas.bind("<Button-5>", lambda e: self._canvas.yview_scroll(1, "units"))
        self._canvas.bind("<ButtonPress-1>", lambda e: self._canvas.scan_mark(e.x, e.y))
        self._canvas.bind("<B1-Motion>", lambda e: self._canvas.scan_dragto(e.x, e.y, gain=1))

        self._texto_frame, self._texto = texto_solo_lectura(self._stack)
        self.mostrar_texto("  ▷  Ejecutá una consulta (F5) para ver el " + nombre + ".")

    def mostrar_texto(self, contenido, info=""):
        self._canvas_frame.pack_forget()
        self._texto_frame.pack(fill="both", expand=True)
        poner_texto(self._texto, contenido)
        self._lbl_info.configure(text=info)
        self._original = self._mostrada = None

    def mostrar_dot(self, dot, alterno):
        """Renderiza `dot`. Si no se puede, muestra `alterno` (texto)."""
        if not dot:
            self.mostrar_texto(alterno)
            return
        ruta_dot = shutil.which("dot")
        if ruta_dot is None:
            self.mostrar_texto(alterno, "Graphviz (dot) no está instalado: se muestra el texto.")
            return
        TRABAJO_DIR.mkdir(parents=True, exist_ok=True)
        dot_path = TRABAJO_DIR / f"{self._nombre.replace(' ', '_')}.dot"
        png_path = dot_path.with_suffix(".png")
        dot_path.write_text(dot, encoding="utf-8")
        try:
            proc = subprocess.run([ruta_dot, "-Tpng", str(dot_path), "-o", str(png_path)],
                                  capture_output=True, text=True, timeout=60)
            ok = proc.returncode == 0 and png_path.exists()
        except (OSError, subprocess.TimeoutExpired):
            ok = False
        if not ok:
            self.mostrar_texto(alterno, "dot falló: se muestra el texto.")
            return
        try:
            self._original = tk.PhotoImage(file=str(png_path))
        except tk.TclError as exc:
            self.mostrar_texto(alterno, f"Tk no pudo abrir el PNG ({exc}).")
            return
        self._texto_frame.pack_forget()
        self._canvas_frame.pack(fill="both", expand=True)
        self._lbl_info.configure(text=f"{self._original.width()}×{self._original.height()} px  ·  arrastrá para moverte")
        # Zoom inicial: el mayor que quepa a lo ancho (un arbol sintactico
        # suele ser mucho mas ancho que alto); como minimo 25%.
        self.update_idletasks()
        ancho = max(self._canvas.winfo_width(), 200)
        self._zoom = 0
        for i, (_, sub, zoom) in enumerate(self.ZOOMS):
            if self._original.width() * zoom // sub <= ancho:
                self._zoom = i
        self._aplicar_zoom()
        # La raiz queda arriba al centro: se centra la vista horizontalmente.
        img_ancho = self._mostrada.width()
        if img_ancho > ancho:
            self._canvas.xview_moveto((img_ancho - ancho) / 2 / img_ancho)
        self._canvas.yview_moveto(0)

    def _cambiar_zoom(self, paso):
        if self._original is None:
            return
        self._zoom = min(max(self._zoom + paso, 0), len(self.ZOOMS) - 1)
        self._aplicar_zoom()

    def _aplicar_zoom(self):
        etiqueta, sub, zoom = self.ZOOMS[self._zoom]
        img = self._original
        if sub > 1:
            img = img.subsample(sub)
        if zoom > 1:
            img = img.zoom(zoom)
        self._mostrada = img
        self._canvas.delete("all")
        self._canvas.create_image(0, 0, image=img, anchor="nw")
        self._canvas.configure(scrollregion=(0, 0, img.width(), img.height()))
        self._lbl_zoom.configure(text=etiqueta)


# ──────────────────────────────────────────────────────────────────────
# Aplicacion
# ──────────────────────────────────────────────────────────────────────

class SqlIDE:
    def __init__(self, root):
        global FUENTE_MONO_NAME, FUENTE_UI_NAME
        self.root = root
        FUENTE_MONO_NAME = _resolver_fuente(
            ("JetBrains Mono", "Cascadia Mono", "Consolas", "DejaVu Sans Mono",
             "Menlo", "Courier New"), "TkFixedFont")
        FUENTE_UI_NAME = _resolver_fuente(
            ("Segoe UI", "Cantarell", "Ubuntu", "DejaVu Sans", "Helvetica"), "TkDefaultFont")

        root.title("SQL IDE — sqlint")
        root.geometry("1400x880")
        root.minsize(1000, 640)
        root.configure(background=Tema.BG)
        self._configurar_estilo()

        self.db_dir = DB_POR_DEFECTO
        self.usar_copia = tk.BooleanVar(value=True)
        self.ocupado = False
        self._diagnosticos = []   # ya ajustados a coordenadas del editor

        self._construir_menu()
        self._construir_toolbar()
        self._construir_cuerpo()
        self._construir_barra_estado()
        self._registrar_atajos()
        self._registrar_drag_and_drop()

        # Los hilos de trabajo (sqlint, make) no tocan la interfaz: dejan una
        # funcion en esta cola y el hilo principal la ejecuta. Tkinter no es
        # seguro entre hilos, ni siquiera para root.after().
        self._cola = queue.Queue()
        self.root.after(50, self._procesar_cola)

        self.root.protocol("WM_DELETE_WINDOW", self._on_cerrar)
        self.root.after(50, self._cargar_estado)

    def _en_hilo(self, trabajo, al_terminar):
        """Corre trabajo() en un hilo y despues al_terminar(resultado) en el
        hilo principal."""
        def envoltura():
            resultado = trabajo()
            self._cola.put(lambda: al_terminar(resultado))
        threading.Thread(target=envoltura, daemon=True).start()

    def _procesar_cola(self):
        try:
            while True:
                self._cola.get_nowait()()
        except queue.Empty:
            pass
        self.root.after(50, self._procesar_cola)

    # ── Estilo ────────────────────────────────────────────────────────

    def _configurar_estilo(self):
        s = ttk.Style()
        try:
            s.theme_use("clam")
        except tk.TclError:
            pass
        ui = (FUENTE_UI_NAME, FUENTE_UI_SIZE)
        ui_b = (FUENTE_UI_NAME, FUENTE_UI_SIZE, "bold")
        s.configure(".", background=Tema.BG, foreground=Tema.FG,
                    fieldbackground=Tema.BG_PANEL, bordercolor=Tema.BORDE_SOFT,
                    troughcolor=Tema.BG_PANEL_2, font=ui)
        for nombre, bg in (("TFrame", Tema.BG), ("Card.TFrame", Tema.BG_PANEL),
                           ("Editor.TFrame", Tema.BG), ("Side.TFrame", Tema.BG_PANEL_2),
                           ("Toolbar.TFrame", Tema.BG_PANEL), ("Drop.TFrame", Tema.DND_HINT),
                           ("Status.TFrame", Tema.BG_PANEL_2)):
            s.configure(nombre, background=bg)
        s.configure("TLabel", background=Tema.BG, foreground=Tema.FG)
        s.configure("Card.TLabel", background=Tema.BG_PANEL, foreground=Tema.FG)
        s.configure("Toolbar.TLabel", background=Tema.BG_PANEL, foreground=Tema.FG_MUTED)
        s.configure("Muted.TLabel", background=Tema.BG_PANEL, foreground=Tema.FG_MUTED)
        s.configure("Side.TLabel", background=Tema.BG_PANEL_2, foreground=Tema.FG,
                    font=(FUENTE_UI_NAME, FUENTE_UI_SIZE + 1, "bold"))
        s.configure("Status.TLabel", background=Tema.BG_PANEL_2,
                    foreground=Tema.FG_MUTED, padding=(10, 4))
        s.configure("Titulo.TLabel", background=Tema.BG_PANEL, foreground=Tema.ACCENT, font=ui_b)
        s.configure("Error.TLabel", background=Tema.BG_PANEL, foreground=Tema.ERR, font=ui_b)
        s.configure("TButton", background=Tema.BG_BUTTON, foreground=Tema.FG,
                    bordercolor=Tema.BG_BUTTON, lightcolor=Tema.BG_BUTTON,
                    darkcolor=Tema.BG_BUTTON, focuscolor=Tema.ACCENT,
                    padding=(12, 6), relief="flat")
        s.map("TButton", background=[("active", Tema.BG_BUTTON_H), ("pressed", Tema.ACCENT_DARK)],
              foreground=[("disabled", Tema.FG_MUTED)])
        s.configure("Accent.TButton", background=Tema.ACCENT_DARK, bordercolor=Tema.ACCENT_DARK,
                    lightcolor=Tema.ACCENT_DARK, darkcolor=Tema.ACCENT_DARK)
        s.map("Accent.TButton", background=[("active", Tema.ACCENT), ("disabled", Tema.BG_BUTTON)])
        s.configure("TCheckbutton", background=Tema.BG_PANEL, foreground=Tema.FG_MUTED)
        s.map("TCheckbutton", background=[("active", Tema.BG_PANEL)])
        s.configure("TScrollbar", background=Tema.BG_BUTTON, troughcolor=Tema.BG_PANEL,
                    bordercolor=Tema.BORDE_SOFT, arrowcolor=Tema.FG_MUTED)
        s.configure("TNotebook", background=Tema.BG, borderwidth=0, tabmargins=(8, 8, 8, 0))
        s.configure("TNotebook.Tab", background=Tema.BG_PANEL, foreground=Tema.FG_MUTED,
                    padding=(14, 7), borderwidth=0, lightcolor=Tema.BG_PANEL,
                    darkcolor=Tema.BG_PANEL)
        s.map("TNotebook.Tab", background=[("selected", Tema.BG_PANEL_2), ("active", Tema.BG_BUTTON_H)],
              foreground=[("selected", Tema.FG), ("active", Tema.FG)])
        s.configure("Tabla.Treeview", background=Tema.BG_PANEL, foreground=Tema.FG,
                    fieldbackground=Tema.BG_PANEL, rowheight=22, borderwidth=0,
                    font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE - 1))
        s.configure("Tabla.Treeview.Heading", background=Tema.BG_PANEL_2, foreground=Tema.ACCENT,
                    bordercolor=Tema.BG_PANEL_2, font=ui_b, padding=(8, 6))
        s.map("Tabla.Treeview", background=[("selected", Tema.BG_ACTIVE)],
              foreground=[("selected", Tema.FG)])
        s.map("Tabla.Treeview.Heading", background=[("active", Tema.BG_BUTTON_H)])
        s.configure("TPanedwindow", background=Tema.BG)
        s.configure("Sash", sashthickness=6)

    # ── Menu y toolbar ────────────────────────────────────────────────

    def _menu(self, barra, titulo):
        m = tk.Menu(barra, tearoff=0, background=Tema.BG_PANEL, foreground=Tema.FG,
                    activebackground=Tema.BG_ACTIVE, activeforeground=Tema.FG)
        barra.add_cascade(label=titulo, menu=m)
        return m

    def _construir_menu(self):
        barra = tk.Menu(self.root, tearoff=0, background=Tema.BG_PANEL, foreground=Tema.FG,
                        activebackground=Tema.BG_ACTIVE, activeforeground=Tema.FG)
        m = self._menu(barra, "Archivo")
        m.add_command(label="Nuevo", accelerator="Ctrl+N", command=self.nuevo)
        m.add_command(label="Abrir…", accelerator="Ctrl+O", command=self.abrir)
        m.add_command(label="Guardar", accelerator="Ctrl+S", command=self.guardar)
        m.add_command(label="Guardar como…", accelerator="Ctrl+Shift+S", command=self.guardar_como)
        m.add_separator()
        m.add_command(label="Salir", command=self._on_cerrar)

        m = self._menu(barra, "Ejecutar")
        m.add_command(label="Ejecutar todo", accelerator="F5", command=self.ejecutar_todo)
        m.add_command(label="Ejecutar selección", accelerator="Ctrl+Enter",
                      command=self.ejecutar_seleccion)
        m.add_command(label="Solo analizar (sin ejecutar)", accelerator="F6",
                      command=lambda: self.ejecutar_todo(solo_analizar=True))
        m.add_separator()
        m.add_command(label="Compilar sqlint (make)", command=self.compilar)

        m = self._menu(barra, "Base de datos")
        m.add_command(label="Elegir directorio…", command=self.elegir_base)
        m.add_checkbutton(label="Trabajar sobre una copia (no modifica el original)",
                          variable=self.usar_copia, command=self._on_cambio_copia)
        m.add_command(label="Reiniciar la copia desde el original", command=self.reiniciar_copia)
        m.add_command(label="Refrescar catálogo", command=self.refrescar_catalogo)

        m = self._menu(barra, "Ejemplos")
        ejemplos = sorted(EJEMPLOS_DIR.glob("*.sql")) if EJEMPLOS_DIR.exists() else []
        if not ejemplos:
            m.add_command(label="(no hay ejemplos en ide/ejemplos)", state="disabled")
        for ruta in ejemplos:
            m.add_command(label=ruta.stem.replace("_", " "),
                          command=lambda r=ruta: self.cargar_ejemplo(r))

        m = self._menu(barra, "Ayuda")
        m.add_command(label="Atajos de teclado", command=self._mostrar_atajos)
        m.add_command(label="Acerca de", command=self._mostrar_acerca)
        self.root.configure(menu=barra)

    def _construir_toolbar(self):
        self._toolbar = ttk.Frame(self.root, style="Toolbar.TFrame", padding=(8, 6))
        self._toolbar.pack(fill="x")
        self._btn_ejecutar = ttk.Button(self._toolbar, text="▶  Ejecutar todo",
                                        style="Accent.TButton", command=self.ejecutar_todo)
        self._btn_ejecutar.pack(side="left")
        self._btn_seleccion = ttk.Button(self._toolbar, text="Ejecutar selección",
                                         command=self.ejecutar_seleccion)
        self._btn_seleccion.pack(side="left", padx=(6, 0))
        self._btn_analizar = ttk.Button(self._toolbar, text="Solo analizar",
                                        command=lambda: self.ejecutar_todo(solo_analizar=True))
        self._btn_analizar.pack(side="left", padx=(6, 0))

        ttk.Separator(self._toolbar, orient="vertical").pack(side="left", fill="y", padx=12)
        ttk.Label(self._toolbar, text="Base:", style="Toolbar.TLabel").pack(side="left")
        self._lbl_base = ttk.Label(self._toolbar, text="", style="Card.TLabel")
        self._lbl_base.pack(side="left", padx=(6, 6))
        ttk.Button(self._toolbar, text="Cambiar…", command=self.elegir_base).pack(side="left")
        ttk.Checkbutton(self._toolbar, text="usar copia", variable=self.usar_copia,
                        command=self._on_cambio_copia).pack(side="left", padx=(10, 0))
        ttk.Button(self._toolbar, text="Reiniciar copia",
                   command=self.reiniciar_copia).pack(side="left", padx=(6, 0))

        self._lbl_mensaje = ttk.Label(self._toolbar, text="", style="Toolbar.TLabel")
        self._lbl_mensaje.pack(side="right")

    # ── Cuerpo: catalogo | editor / pestanas ─────────────────────────

    def _construir_cuerpo(self):
        self._paned_h = ttk.PanedWindow(self.root, orient="horizontal")
        self._paned_h.pack(fill="both", expand=True)

        lateral = ttk.Frame(self._paned_h, style="Side.TFrame")
        ttk.Label(lateral, text="Catálogo", style="Side.TLabel").pack(anchor="w", padx=10, pady=(8, 4))
        marco, self._catalogo = tabla(lateral, [("tipo", "Tipo", 110, False),
                                                ("info", "Restricciones", 120, True)],
                                      show="tree headings")
        self._catalogo.heading("#0", text="Nombre")
        self._catalogo.column("#0", width=150, stretch=False)
        self._catalogo.tag_configure("tabla", foreground=Tema.ACCENT)
        self._catalogo.tag_configure("indice", foreground=Tema.OK)
        self._catalogo.tag_configure("vacio", foreground=Tema.FG_MUTED)
        marco.pack(fill="both", expand=True)
        self._paned_h.add(lateral, weight=1)

        self._paned_v = ttk.PanedWindow(self._paned_h, orient="vertical")
        self._paned_h.add(self._paned_v, weight=5)

        self.editor = EditorSQL(self._paned_v, on_cursor_change=lambda _e: self._actualizar_estado(),
                                on_modificado_change=lambda _e: self._actualizar_titulo())
        self._paned_v.add(self.editor, weight=3)

        self._tabs = ttk.Notebook(self._paned_v)
        self._paned_v.add(self._tabs, weight=2)

        # Resultados: una sub-pestana por sentencia.
        self._tab_resultados = ttk.Frame(self._tabs, style="Card.TFrame")
        self._tabs.add(self._tab_resultados, text="Resultados")
        self._resultados = ttk.Notebook(self._tab_resultados)
        self._resultados_vacio = ttk.Label(
            self._tab_resultados, style="Muted.TLabel",
            text="  ▷  Ejecutá el script (F5) o una selección (Ctrl+Enter) para ver los resultados.")
        self._resultados_vacio.pack(anchor="nw", padx=10, pady=10)

        # Errores: doble clic lleva a la posicion en el editor.
        marco, self._errores = tabla(self._tabs, [
            ("fase", "Fase", 90, False), ("codigo", "Código", 80, False),
            ("linea", "Línea", 55, False), ("col", "Col", 50, False),
            ("mensaje", "Mensaje", 600, True)])
        self._errores.tag_configure("error", foreground=Tema.ERR)
        self._errores.bind("<Double-1>", self._on_error_doble_clic)
        self._tabs.add(marco, text="Errores")

        marco, self._plan = texto_solo_lectura(self._tabs)
        self._plan.tag_configure("titulo", foreground=Tema.ACCENT,
                                 font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE - 1, "bold"))
        self._plan.tag_configure("indice", foreground=Tema.OK,
                                 font=(FUENTE_MONO_NAME, FUENTE_MONO_SIZE - 1, "bold"))
        self._plan.tag_configure("muted", foreground=Tema.FG_MUTED)
        poner_texto(self._plan, "  ▷  Aquí aparece cómo se ejecutó cada sentencia: "
                                "SeqScan (recorrido completo) o IndexScan (usa un índice).")
        self._tabs.add(marco, text="Plan")

        marco, self._tokens = tabla(self._tabs, [
            ("n", "#", 50, False), ("tipo", "Tipo", 160, False),
            ("lexema", "Lexema", 260, True), ("linea", "Línea", 60, False),
            ("col", "Col", 60, False)])
        self._tabs.add(marco, text="Tokens")

        self._arbol = VisorGrafo(self._tabs, "árbol sintáctico")
        self._tabs.add(self._arbol, text="Árbol sintáctico")
        self._ast = VisorGrafo(self._tabs, "AST")
        self._tabs.add(self._ast, text="AST")

    def _construir_barra_estado(self):
        barra = ttk.Frame(self.root, style="Status.TFrame")
        barra.pack(fill="x", side="bottom")
        self._lbl_archivo = ttk.Label(barra, text="", style="Status.TLabel")
        self._lbl_archivo.pack(side="left")
        self._lbl_resumen = ttk.Label(barra, text="", style="Status.TLabel")
        self._lbl_resumen.pack(side="left")
        self._lbl_cursor = ttk.Label(barra, text="", style="Status.TLabel")
        self._lbl_cursor.pack(side="right")

    def _registrar_atajos(self):
        r = self.root
        r.bind("<F5>", lambda e: self.ejecutar_todo())
        r.bind("<F6>", lambda e: self.ejecutar_todo(solo_analizar=True))
        r.bind("<Control-n>", lambda e: self.nuevo())
        r.bind("<Control-o>", lambda e: self.abrir())
        r.bind("<Control-s>", lambda e: self.guardar())
        r.bind("<Control-S>", lambda e: self.guardar_como())
        # En el Text, Ctrl+Enter insertaria un salto de linea: se corta.
        self.editor.text.bind("<Control-Return>", lambda e: (self.ejecutar_seleccion(), "break")[1])
        r.bind("<Control-Return>", lambda e: self.ejecutar_seleccion())

    # ── Estado visual ─────────────────────────────────────────────────

    def _mensaje(self, texto, color=Tema.FG_MUTED):
        self._lbl_mensaje.configure(text=texto, foreground=color)

    def _base_efectiva(self):
        """Directorio que recibe sqlint: la copia de trabajo o el original."""
        if not self.usar_copia.get():
            return self.db_dir
        copia = COPIAS_DIR / self.db_dir.name
        if not copia.exists():
            COPIAS_DIR.mkdir(parents=True, exist_ok=True)
            if self.db_dir.exists():
                shutil.copytree(self.db_dir, copia)
            else:
                copia.mkdir(parents=True)
        return copia

    def _actualizar_titulo(self):
        nombre = self.editor.ruta.name if self.editor.ruta else "(sin guardar)"
        marca = " •" if self.editor.modificado else ""
        self.root.title(f"{nombre}{marca} — SQL IDE")
        self._lbl_archivo.configure(text=f"{nombre}{marca}")

    def _actualizar_estado(self):
        linea, col = self.editor.linea_columna()
        self._lbl_cursor.configure(text=f"Ln {linea}, Col {col}")

    def _actualizar_base(self):
        try:
            nombre = self.db_dir.relative_to(PROJECT_ROOT).as_posix()
        except ValueError:
            nombre = str(self.db_dir)
        sufijo = "  (copia)" if self.usar_copia.get() else "  (ORIGINAL: se modifica)"
        self._lbl_base.configure(text=nombre + sufijo,
                                 foreground=Tema.FG if self.usar_copia.get() else Tema.WARN)

    def _set_ocupado(self, valor):
        self.ocupado = valor
        estado = "disabled" if valor else "normal"
        for b in (self._btn_ejecutar, self._btn_seleccion, self._btn_analizar):
            b.configure(state=estado)
        self.root.configure(cursor="watch" if valor else "")

    # ── Archivos ──────────────────────────────────────────────────────

    def _confirmar_descartar(self):
        if not self.editor.modificado:
            return True
        r = messagebox.askyesnocancel("Cambios sin guardar", "¿Guardar los cambios antes de continuar?")
        if r is None:
            return False
        return self.guardar() if r else True

    def nuevo(self):
        if self._confirmar_descartar():
            self.editor.poner_texto("")
            self._actualizar_titulo()

    def abrir(self, ruta=None):
        if not self._confirmar_descartar():
            return
        if ruta is None:
            ruta = filedialog.askopenfilename(
                title="Abrir script SQL", initialdir=str(PROJECT_ROOT),
                filetypes=[("Scripts SQL", "*.sql"), ("Todos", "*.*")])
            if not ruta:
                return
        try:
            self.editor.cargar(ruta)
        except OSError as exc:
            messagebox.showerror("No se pudo abrir", str(exc))
            return
        self._actualizar_titulo()
        self._mensaje(f"Abierto: {Path(ruta).name}", Tema.OK)

    def guardar(self):
        if self.editor.ruta is None:
            return self.guardar_como()
        try:
            self.editor.guardar()
        except OSError as exc:
            messagebox.showerror("No se pudo guardar", str(exc))
            return False
        self._actualizar_titulo()
        self._mensaje(f"Guardado: {self.editor.ruta.name}", Tema.OK)
        return True

    def guardar_como(self):
        ruta = filedialog.asksaveasfilename(
            title="Guardar script SQL", defaultextension=".sql", initialdir=str(PROJECT_ROOT),
            filetypes=[("Scripts SQL", "*.sql"), ("Todos", "*.*")])
        if not ruta:
            return False
        try:
            self.editor.guardar(ruta)
        except OSError as exc:
            messagebox.showerror("No se pudo guardar", str(exc))
            return False
        self._actualizar_titulo()
        return True

    def cargar_ejemplo(self, ruta):
        # Los ejemplos se abren como texto nuevo (sin ruta), para no
        # sobrescribir el archivo de ejemplo al guardar.
        if not self._confirmar_descartar():
            return
        self.editor.poner_texto(Path(ruta).read_text(encoding="utf-8"))
        self._actualizar_titulo()
        self._mensaje(f"Ejemplo: {ruta.stem.replace('_', ' ')}", Tema.OK)

    # ── Base de datos ─────────────────────────────────────────────────

    def elegir_base(self):
        ruta = filedialog.askdirectory(title="Directorio de la base de datos",
                                       initialdir=str(self.db_dir.parent), mustexist=False)
        if not ruta:
            return
        self.db_dir = Path(ruta).resolve()
        self._actualizar_base()
        self.refrescar_catalogo()

    def _on_cambio_copia(self):
        self._actualizar_base()
        self.refrescar_catalogo()

    def reiniciar_copia(self):
        copia = COPIAS_DIR / self.db_dir.name
        if copia.exists():
            shutil.rmtree(copia)
        self._base_efectiva()
        self._mensaje(f"Copia de '{self.db_dir.name}' reiniciada desde el original", Tema.OK)
        self.refrescar_catalogo()

    def refrescar_catalogo(self):
        if not sqlint_disponible():
            self._poblar_catalogo(None, "Falta compilar sqlint (make).")
            return
        db = ruta_para_sqlint(self._base_efectiva())

        def trabajo():
            try:
                return correr_sqlint_json(["--db", db, "--emit", "catalog"])
            except (OSError, subprocess.TimeoutExpired) as exc:
                return None, f"No se pudo ejecutar sqlint: {exc}"

        self._en_hilo(trabajo, lambda r: self._poblar_catalogo(*r))

    def _poblar_catalogo(self, datos, error=None):
        t = self._catalogo
        t.delete(*t.get_children())
        catalogo = (datos or {}).get("catalog") if datos else None
        if not catalogo:
            mensaje = error or "; ".join(d.get("message", "") for d in (datos or {}).get("diagnostics", []))
            t.insert("", "end", text="(sin catálogo)", values=("", mensaje or ""), tags=("vacio",))
            return
        tablas = catalogo.get("tables", [])
        if not tablas:
            t.insert("", "end", text="(base vacía)", tags=("vacio",))
        for tab in tablas:
            nodo = t.insert("", "end", text=tab["name"], open=True, tags=("tabla",),
                            values=(f"{len(tab['columns'])} columnas", ""))
            for col in tab["columns"]:
                flags = [f for f, on in (("PK", col.get("primaryKey")),
                                         ("NOT NULL", col.get("notNull"))) if on]
                t.insert(nodo, "end", text=col["name"], values=(col["type"], ", ".join(flags)))
            for idx in tab.get("indexes", []):
                t.insert(nodo, "end", text=idx["name"], tags=("indice",),
                         values=("ÍNDICE", f"sobre {idx['column']}"))

    # ── Ejecucion ─────────────────────────────────────────────────────

    def ejecutar_todo(self, solo_analizar=False):
        self._correr(self.editor.contenido(), 1, 0, solo_analizar)

    def ejecutar_seleccion(self):
        sel = self.editor.seleccion()
        if sel is None:
            self._mensaje("Seleccioná el texto a ejecutar (o usá F5 para todo el script).", Tema.WARN)
            return
        texto, linea, col = sel
        self._correr(texto, linea, col, False)

    def compilar(self, al_terminar=None):
        if self.ocupado:
            return
        self._set_ocupado(True)
        self._mensaje("Compilando sqlint (make)…", Tema.WARN)

        def trabajo():
            try:
                proc = subprocess.run(COMANDO_COMPILACION, cwd=str(PROJECT_ROOT),
                                      capture_output=True, text=True, encoding="utf-8",
                                      errors="replace")
                return proc.returncode, proc.stdout + proc.stderr
            except OSError as exc:
                return -1, f"No se pudo ejecutar make: {exc}"

        self._en_hilo(trabajo, lambda r: self._fin_compilar(r[0], r[1], al_terminar))

    def _fin_compilar(self, codigo, salida, al_terminar):
        self._set_ocupado(False)
        if codigo != 0 or not sqlint_disponible():
            self._mensaje("La compilación falló (ver Errores)", Tema.ERR)
            self._errores.delete(*self._errores.get_children())
            for linea in [l for l in salida.splitlines() if l.strip()][-30:]:
                self._errores.insert("", "end", values=("make", "", "", "", linea), tags=("error",))
            self._tabs.select(1)
            return
        self._mensaje("sqlint compilado", Tema.OK)
        if al_terminar is not None:
            al_terminar()

    def _correr(self, texto, linea_base, col_base, solo_analizar):
        """Corre `texto` (todo el script o una seleccion que empieza en
        linea_base/col_base del editor) y reparte la salida en las pestanas."""
        if self.ocupado:
            return
        if not texto.strip():
            self._mensaje("El editor está vacío.", Tema.WARN)
            return
        if not sqlint_disponible():
            self.compilar(al_terminar=lambda: self._correr(texto, linea_base, col_base, solo_analizar))
            return

        TRABAJO_DIR.mkdir(parents=True, exist_ok=True)
        script = TRABAJO_DIR / "script_actual.sql"
        script.write_text(texto, encoding="utf-8")
        arch = ruta_para_sqlint(script)
        db = ruta_para_sqlint(self._base_efectiva())
        self._set_ocupado(True)
        self._mensaje("Analizando…" if solo_analizar else "Ejecutando…", Tema.WARN)

        def trabajo():
            r = {}
            try:
                base = ["--db", db]
                # Primero la ejecucion (es la unica llamada que toca el disco);
                # las vistas solo parsean.
                r["run"] = correr_sqlint_json(base + (["--no-exec"] if solo_analizar else []) + [arch])
                r["tokens"] = correr_sqlint_json(base + ["--emit", "tokens", arch])
                r["parse"] = correr_sqlint_json(base + ["--emit", "parse-tree", arch])
                r["ast_dot"] = correr_sqlint_json(base + ["--emit", "ast-dot", arch])
                r["ast"] = correr_sqlint_json(base + ["--emit", "ast", arch])
                r["catalog"] = correr_sqlint_json(base + ["--emit", "catalog"])
            except (OSError, subprocess.TimeoutExpired) as exc:
                r["fallo"] = f"No se pudo ejecutar sqlint: {exc}"
            return r

        self._en_hilo(trabajo, lambda r: self._fin_correr(r, linea_base, col_base, solo_analizar))

    def _a_editor(self, linea, col, linea_base, col_base):
        """Coordenadas del script corrido -> coordenadas del editor (solo
        cambian al ejecutar una seleccion que no empieza en 1:1)."""
        if linea == 1:
            col += col_base
        return linea + linea_base - 1, col

    def _fin_correr(self, r, linea_base, col_base, solo_analizar):
        self._set_ocupado(False)
        if "fallo" in r:
            self._mensaje(r["fallo"], Tema.ERR)
            return

        datos, error = r["run"]
        if datos is None:
            self._mostrar_errores([{"phase": "ide", "code": "", "line": 0, "col": 0,
                                    "length": 1, "message": error}], linea_base, col_base)
            self._tabs.select(1)
            self._mensaje("sqlint no devolvió una respuesta válida", Tema.ERR)
            return

        diags = datos.get("diagnostics", [])
        sentencias = datos.get("statements", [])
        self._mostrar_errores(diags, linea_base, col_base)
        self._mostrar_resultados(sentencias, linea_base, solo_analizar, len(diags))
        self._mostrar_plan(sentencias, linea_base)
        self._mostrar_tokens(r["tokens"], linea_base, col_base)
        self._mostrar_grafos(r)
        cat, cat_error = r["catalog"]
        self._poblar_catalogo(cat, cat_error)

        ok = sum(1 for s in sentencias if s.get("status") == "ok")
        if diags:
            self._tabs.select(1)
            self._mensaje(f"{len(diags)} error(es)", Tema.ERR)
        else:
            self._tabs.select(0)
            self._mensaje("Sin errores semánticos" if solo_analizar
                          else f"{ok} sentencia(s) ejecutada(s)", Tema.OK)
        self._lbl_resumen.configure(
            text=f"·  {len(sentencias)} sentencia(s), {len(diags)} error(es)")

    # ── Volcado a las pestanas ────────────────────────────────────────

    def _mostrar_errores(self, diags, linea_base, col_base):
        self._errores.delete(*self._errores.get_children())
        self.editor.limpiar_errores()
        self._diagnosticos = []
        for d in diags:
            linea, col = d.get("line", 0), d.get("col", 0)
            if linea > 0:
                linea, col = self._a_editor(linea, col, linea_base, col_base)
                self.editor.marcar_error(linea, col, d.get("length", 1))
            ajustado = dict(d, line=linea, col=col)
            self._diagnosticos.append(ajustado)
            self._errores.insert("", "end", iid=str(len(self._diagnosticos) - 1),
                                 values=(d.get("phase", ""), d.get("code", ""),
                                         linea or "", col or "", d.get("message", "")),
                                 tags=("error",))
        titulo = f"Errores ({len(diags)})" if diags else "Errores"
        self._tabs.tab(1, text=titulo)

    def _on_error_doble_clic(self, _ev=None):
        sel = self._errores.selection()
        if not sel or not sel[0].isdigit():
            return
        d = self._diagnosticos[int(sel[0])]
        if d.get("line", 0) <= 0:
            return
        self.editor.ir_a(d["line"], d["col"])
        self.editor.marcar_error(d["line"], d["col"], d.get("length", 1), foco=True)

    def _mostrar_resultados(self, sentencias, linea_base, solo_analizar, n_errores):
        for tab in self._resultados.tabs():
            self._resultados.forget(tab)
        if not sentencias:
            if solo_analizar and n_errores == 0:
                texto = "  ✓  Sin errores léxicos, sintácticos ni semánticos (no se ejecutó nada)."
            elif n_errores:
                texto = (f"  ✗  No se ejecutó ninguna sentencia: hay {n_errores} error(es). "
                         "Ver la pestaña Errores.")
            else:
                texto = "  El script no tiene sentencias."
            self._resultados.pack_forget()
            self._resultados_vacio.configure(text=texto)
            self._resultados_vacio.pack(anchor="nw", padx=10, pady=10)
            return
        self._resultados_vacio.pack_forget()
        self._resultados.pack(fill="both", expand=True)
        for s in sentencias:
            self._resultados.add(self._panel_sentencia(s, linea_base),
                                 text=f"{s.get('index', 0) + 1}: {s.get('kind', '?')}")
        if n_errores:
            # La sentencia que fallo en semantica no aparece en `statements`:
            # se avisa al final, que es donde se detuvo el script.
            aviso = ttk.Frame(self._resultados, style="Card.TFrame")
            ttk.Label(aviso, style="Error.TLabel",
                      text=f"  El script se detuvo: {n_errores} error(es). "
                           "Ver la pestaña Errores.").pack(anchor="nw", padx=10, pady=10)
            self._resultados.add(aviso, text="✗ detenido")

    def _panel_sentencia(self, s, linea_base):
        panel = ttk.Frame(self._resultados, style="Card.TFrame")
        linea = s.get("line", 1) + linea_base - 1
        error = s.get("status") != "ok"
        ttk.Label(panel, style="Error.TLabel" if error else "Titulo.TLabel",
                  text=f"{s.get('kind', '?')} · línea {linea} · {s.get('message', '')}"
                  ).pack(anchor="w", padx=10, pady=(8, 2))
        if s.get("plan"):
            ttk.Label(panel, style="Muted.TLabel", text="Plan: " + s["plan"].splitlines()[0]
                      ).pack(anchor="w", padx=10, pady=(0, 6))
        columnas = s.get("columns") or []
        if not columnas:
            return panel
        ids = [f"c{i}" for i in range(len(columnas))]
        marco, grilla = tabla(panel, [(cid, c["name"], 140, True) for cid, c in zip(ids, columnas)])
        for cid, c in zip(ids, columnas):
            grilla.heading(cid, text=f"{c['name']}  ({c['type']})")
        grilla.tag_configure("null", foreground=Tema.FG_MUTED)
        for fila in s.get("rows", []):
            valores = ["NULL" if v is None else
                       ("TRUE" if v is True else "FALSE" if v is False else v) for v in fila]
            grilla.insert("", "end", values=valores,
                          tags=("null",) if all(v is None for v in fila) else ())
        marco.pack(fill="both", expand=True, padx=6, pady=(0, 6))
        return panel

    def _mostrar_plan(self, sentencias, linea_base):
        t = self._plan
        t.configure(state="normal")
        t.delete("1.0", "end")
        if not sentencias:
            t.insert("end", "  (no se ejecutó ninguna sentencia)", ("muted",))
        for s in sentencias:
            linea = s.get("line", 1) + linea_base - 1
            t.insert("end", f"{s.get('index', 0) + 1}. {s.get('kind', '?')} (línea {linea})\n", ("titulo",))
            plan = s.get("plan") or "(sin plan: la sentencia no lee filas)"
            for l in plan.splitlines():
                tag = ("indice",) if "IndexScan" in l else (("muted",) if not s.get("plan") else ())
                t.insert("end", f"    {l}\n", tag)
            t.insert("end", "\n")
        t.configure(state="disabled")

    def _mostrar_tokens(self, resultado, linea_base, col_base):
        self._tokens.delete(*self._tokens.get_children())
        datos, _error = resultado
        for i, tok in enumerate((datos or {}).get("tokens") or [], start=1):
            linea, col = self._a_editor(tok.get("line", 0), tok.get("col", 0), linea_base, col_base)
            self._tokens.insert("", "end", values=(i, tok.get("type", ""), tok.get("text", ""), linea, col))

    def _mostrar_grafos(self, r):
        parse, _ = r["parse"]
        ast_dot, _ = r["ast_dot"]
        ast_txt, _ = r["ast"]
        hay_errores = bool((parse or {}).get("diagnostics"))
        sin_arbol = ("  No hay árbol: el script tiene errores léxicos o sintácticos."
                     if hay_errores else "  (vacío)")
        dot_parse = (parse or {}).get("dot") or ""
        self._arbol.mostrar_dot(dot_parse if not hay_errores else "", dot_parse or sin_arbol)
        texto_ast = (ast_txt or {}).get("ast") or sin_arbol
        self._ast.mostrar_dot((ast_dot or {}).get("dot") or "", texto_ast)

    # ── Arrastrar y soltar ────────────────────────────────────────────

    def _registrar_drag_and_drop(self):
        if not DND_DISPONIBLE:
            return
        for w in (self.root, self.editor.text):
            try:
                w.drop_target_register(DND_FILES)  # type: ignore[attr-defined]
                w.dnd_bind("<<Drop>>", self._on_drop)  # type: ignore[attr-defined]
            except (tk.TclError, AttributeError):
                continue

    def _on_drop(self, event):
        for raw in self.root.tk.splitlist(event.data):
            ruta = Path(normalizar_ruta_arrastrada(raw))
            if ruta.is_file():
                self.abrir(ruta)
                break
        return getattr(event, "action", None)

    # ── Persistencia ──────────────────────────────────────────────────

    def _guardar_estado(self):
        estado = {
            "geometria": self.root.geometry(),
            "archivo": str(self.editor.ruta) if self.editor.ruta else None,
            "base": str(self.db_dir),
            "usar_copia": self.usar_copia.get(),
        }
        try:
            ESTADO_IDE_PATH.write_text(json.dumps(estado, ensure_ascii=False, indent=2),
                                       encoding="utf-8")
        except OSError:
            pass

    def _cargar_estado(self):
        datos = {}
        if ESTADO_IDE_PATH.exists():
            try:
                datos = json.loads(ESTADO_IDE_PATH.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                datos = {}
        geo = datos.get("geometria")
        if isinstance(geo, str) and "x" in geo:
            try:
                self.root.geometry(geo)
            except tk.TclError:
                pass
        if datos.get("base"):
            self.db_dir = Path(datos["base"])
        if isinstance(datos.get("usar_copia"), bool):
            self.usar_copia.set(datos["usar_copia"])
        self._actualizar_base()
        archivo = datos.get("archivo")
        if archivo and Path(archivo).is_file():
            try:
                self.editor.cargar(archivo)
            except OSError:
                pass
        self._actualizar_titulo()
        self._actualizar_estado()
        self.refrescar_catalogo()
        if not sqlint_disponible():
            self._mensaje("sqlint no está compilado: se compila al ejecutar (o Ejecutar → Compilar).",
                          Tema.WARN)

    def _on_cerrar(self):
        if not self._confirmar_descartar():
            return
        self._guardar_estado()
        self.root.destroy()

    # ── Ayuda ─────────────────────────────────────────────────────────

    def _mostrar_atajos(self):
        messagebox.showinfo("Atajos de teclado",
                            "F5               Ejecutar todo el script\n"
                            "Ctrl+Enter       Ejecutar la selección\n"
                            "F6               Solo analizar (no toca la base)\n"
                            "Ctrl+N / Ctrl+O  Nuevo / Abrir\n"
                            "Ctrl+S           Guardar\n"
                            "Ctrl+Shift+S     Guardar como\n\n"
                            "Doble clic en un error: lleva a su posición en el editor.")

    def _mostrar_acerca(self):
        messagebox.showinfo("Acerca de",
                            "SQL IDE para sqlint\n"
                            "Intérprete SQL con almacenamiento en archivos\n"
                            "Construcción de Compiladores, UVG 2026")


def main():
    root = TkinterDnD.Tk() if DND_DISPONIBLE and TkinterDnD is not None else tk.Tk()
    SqlIDE(root)
    root.mainloop()


if __name__ == "__main__":
    main()
