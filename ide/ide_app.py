"""IDE  para el compilador Compis (YALex + YAPar + LL(1)).

 carga, edita,
guarda, compila y ejecuta, mostrando la salida ya producida por el
proyecto en paneles separados.
"""

import os
import subprocess
import threading
from pathlib import Path

import tkinter as tk
from tkinter import filedialog, messagebox, ttk


PROJECT_ROOT = Path(__file__).resolve().parent.parent
INPUT_DIR = PROJECT_ROOT / "input"
EJECUTABLE_NOMBRE = "compilador.exe" if os.name == "nt" else "compilador"
EJECUTABLE_PATH = PROJECT_ROOT / EJECUTABLE_NOMBRE

COMANDO_COMPILACION = [
    "g++", "-std=c++17", "-O2", "-DCOMPILAR_CON_ORQUESTADOR",
    "-o", str(EJECUTABLE_PATH),
    str(PROJECT_ROOT / "Main.cpp"),
    str(PROJECT_ROOT / "lexer" / "YalexParser.cpp"),
    str(PROJECT_ROOT / "Parser" / "YaparParser.cpp"),
    str(PROJECT_ROOT / "Parser" / "Grammar.cpp"),
    str(PROJECT_ROOT / "Parser" / "FirstFollow.cpp"),
    str(PROJECT_ROOT / "Parser" / "LL1Table.cpp"),
    str(PROJECT_ROOT / "Parser" / "LR0.cpp"),
    str(PROJECT_ROOT / "Parser" / "SLR1.cpp"),
    str(PROJECT_ROOT / "Parser" / "LALR1.cpp"),
]

FUENTE_MONO = ("Consolas", 10)
FUENTE_TITULO = ("Segoe UI", 9, "bold")


# ──────────────────────────────────────────────────────────────────────
# Editor de texto plano
# ──────────────────────────────────────────────────────────────────────

class EditorPanel(ttk.Frame):
    """Editor de texto plano con titulo y seguimiento de ruta."""

    def __init__(self, master, titulo_base, extensiones_dialogo):
        super().__init__(master)
        self.ruta = None
        self._titulo_base = titulo_base
        self.extensiones_dialogo = extensiones_dialogo

        self.titulo_var = tk.StringVar(value=titulo_base)
        ttk.Label(self, textvariable=self.titulo_var,
                  font=FUENTE_TITULO).pack(anchor="w", padx=4, pady=2)

        contenedor = ttk.Frame(self)
        contenedor.pack(fill="both", expand=True, padx=4, pady=(0, 4))

        self.text = tk.Text(
            contenedor, wrap="none", undo=True,
            font=FUENTE_MONO,
            background="white", foreground="black",
            insertbackground="black",
            borderwidth=1, relief="solid",
        )
        scroll_y = ttk.Scrollbar(contenedor, orient="vertical",
                                 command=self.text.yview)
        scroll_x = ttk.Scrollbar(contenedor, orient="horizontal",
                                 command=self.text.xview)
        self.text.configure(yscrollcommand=scroll_y.set,
                            xscrollcommand=scroll_x.set)

        self.text.grid(row=0, column=0, sticky="nsew")
        scroll_y.grid(row=0, column=1, sticky="ns")
        scroll_x.grid(row=1, column=0, sticky="ew")
        contenedor.rowconfigure(0, weight=1)
        contenedor.columnconfigure(0, weight=1)

    def cargar(self, ruta):
        ruta = Path(ruta)
        with open(ruta, "r", encoding="utf-8", errors="replace") as f:
            contenido = f.read()
        self.text.delete("1.0", tk.END)
        self.text.insert("1.0", contenido)
        self.ruta = ruta
        self._refrescar_titulo()

    def guardar(self, nueva_ruta=None):
        if nueva_ruta is not None:
            self.ruta = Path(nueva_ruta)
        if self.ruta is None:
            raise ValueError("Editor sin ruta asignada")
        contenido = self.text.get("1.0", "end-1c")
        with open(self.ruta, "w", encoding="utf-8") as f:
            f.write(contenido)
        self._refrescar_titulo()

    def vacio(self):
        return not self.text.get("1.0", "end-1c").strip()

    def _refrescar_titulo(self):
        if self.ruta is None:
            self.titulo_var.set(self._titulo_base)
            return
        try:
            rel = self.ruta.relative_to(PROJECT_ROOT)
            self.titulo_var.set(f"{self._titulo_base}  —  {rel}")
        except ValueError:
            self.titulo_var.set(f"{self._titulo_base}  —  {self.ruta}")


# ──────────────────────────────────────────────────────────────────────
# Particionado de la salida del compilador en secciones
# ──────────────────────────────────────────────────────────────────────

SEPARADOR_LINEA = "=" * 50

# Mapa: clave de panel -> lista de subcadenas que pueden aparecer en el
# titulo de un encabezado impreso por Main.cpp (`separador()`).
MAPA_SECCIONES = [
    ("tokens",       ["Analisis lexico"]),
    ("validaciones", ["Lectura del .yapar",
                       "Tabla de simbolos",
                       "Gramatica"]),
    ("ff",           ["FIRST", "FOLLOW"]),
    ("ll1",          ["Tabla LL(1)"]),
    ("traza",        ["Parsing LL(1)"]),
    ("resultado",    ["Resumen", "Automata LR", "Evaluacion"]),
]


def extraer_bloques(stdout):
    """Devuelve lista de (titulo, cuerpo) detectando los separadores
    impresos por `separador()` en `Main.cpp`."""
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
    """Reparte la salida del compilador en las secciones que muestra la IDE."""
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
                or "RECHAZADO" in u):
            errores.append(linea)
    secciones["errores"] = "\n".join(errores)
    return secciones


# ──────────────────────────────────────────────────────────────────────
# Aplicacion principal
# ──────────────────────────────────────────────────────────────────────

class IDE:

    def __init__(self, root):
        self.root = root
        self.root.title("Compis IDE — YALex + YAPar + LL(1)")
        self.root.geometry("1280x820")
        self.root.minsize(960, 600)

        self._construir_toolbar()
        self._construir_editores()
        self._construir_resultados()
        self._construir_barra_estado()

        self.proceso_activo = False

    # ── Construccion de UI ────────────────────────────────────────────

    def _construir_toolbar(self):
        barra = ttk.Frame(self.root, padding=4)
        barra.pack(side="top", fill="x")

        botones = [
            ("Cargar YALex",      self.cargar_yalex),
            ("Cargar YAPar",      self.cargar_yapar),
            ("Cargar entrada",    self.cargar_entrada),
            ("|",                 None),
            ("Guardar YALex",     lambda: self._guardar_editor("yalex")),
            ("Guardar YAPar",     lambda: self._guardar_editor("yapar")),
            ("Guardar entrada",   lambda: self._guardar_editor("entrada")),
            ("Guardar todo",      self.guardar_todo),
            ("|",                 None),
            ("Compilar",          self.compilar),
            ("Ejecutar análisis", self.ejecutar),
            ("Limpiar salida",    self.limpiar_salida),
        ]
        for texto, comando in botones:
            if comando is None:
                ttk.Separator(barra, orient="vertical").pack(
                    side="left", fill="y", padx=4)
                continue
            ttk.Button(barra, text=texto, command=comando).pack(
                side="left", padx=2)

    def _construir_editores(self):
        panel = ttk.PanedWindow(self.root, orient="horizontal")
        panel.pack(fill="both", expand=True, padx=4, pady=2)

        self.editor_yalex = EditorPanel(
            panel, "YALex (.yal)",
            [("YALex", "*.yal *.yalex"), ("Todos", "*.*")])
        self.editor_yapar = EditorPanel(
            panel, "YAPar (.yapar)",
            [("YAPar", "*.yapar"), ("Todos", "*.*")])
        self.editor_entrada = EditorPanel(
            panel, "Entrada",
            [("Texto", "*.txt"), ("Todos", "*.*")])

        panel.add(self.editor_yalex, weight=1)
        panel.add(self.editor_yapar, weight=1)
        panel.add(self.editor_entrada, weight=1)

        self._editores = {
            "yalex":   self.editor_yalex,
            "yapar":   self.editor_yapar,
            "entrada": self.editor_entrada,
        }

    def _construir_resultados(self):
        marco = ttk.Frame(self.root)
        marco.pack(side="bottom", fill="x", padx=4, pady=4)
        marco.configure(height=320)
        marco.pack_propagate(False)

        self.notebook = ttk.Notebook(marco)
        self.notebook.pack(fill="both", expand=True)

        self.paneles = {}
        for clave, titulo in [
            ("salida",       "Salida completa"),
            ("tokens",       "Tokens"),
            ("validaciones", "YAPar / Validaciones"),
            ("ff",           "FIRST / FOLLOW"),
            ("ll1",          "Tabla LL(1)"),
            ("traza",        "Traza Parser"),
            ("resultado",    "Resultado"),
            ("errores",      "Errores"),
        ]:
            text = self._crear_panel_lectura(self.notebook, titulo)
            self.paneles[clave] = text

    def _crear_panel_lectura(self, notebook, titulo):
        frame = ttk.Frame(notebook)
        notebook.add(frame, text=titulo)
        text = tk.Text(
            frame, wrap="none", font=FUENTE_MONO,
            background="white", foreground="black",
            borderwidth=1, relief="solid", state="disabled",
        )
        scroll_y = ttk.Scrollbar(frame, orient="vertical", command=text.yview)
        scroll_x = ttk.Scrollbar(frame, orient="horizontal", command=text.xview)
        text.configure(yscrollcommand=scroll_y.set, xscrollcommand=scroll_x.set)
        text.grid(row=0, column=0, sticky="nsew")
        scroll_y.grid(row=0, column=1, sticky="ns")
        scroll_x.grid(row=1, column=0, sticky="ew")
        frame.rowconfigure(0, weight=1)
        frame.columnconfigure(0, weight=1)
        text._tab_frame = frame
        text._tab_titulo = titulo
        return text

    def _construir_barra_estado(self):
        barra = ttk.Frame(self.root, relief="sunken", padding=(6, 2))
        barra.pack(side="bottom", fill="x")
        self.estado_var = tk.StringVar(value="Listo")
        ttk.Label(barra, textvariable=self.estado_var).pack(side="left")
        ttk.Label(
            barra,
            text=f"Ejecutable esperado: {EJECUTABLE_PATH.name}",
        ).pack(side="right")

    # ── Utilidades de panel ───────────────────────────────────────────

    def _escribir(self, clave, texto):
        panel = self.paneles[clave]
        panel.configure(state="normal")
        panel.delete("1.0", tk.END)
        panel.insert("1.0", texto if texto else "(vacio)")
        panel.configure(state="disabled")

    def _set_estado(self, texto):
        self.estado_var.set(texto)
        self.root.update_idletasks()

    def _seleccionar_tab(self, clave):
        panel = self.paneles[clave]
        for i in range(len(self.notebook.tabs())):
            if self.notebook.tab(i, "text") == panel._tab_titulo:
                self.notebook.select(i)
                return

    def limpiar_salida(self):
        for clave in self.paneles:
            self._escribir(clave, "")
        self._set_estado("Salida limpiada")

    # ── Cargar archivos ───────────────────────────────────────────────

    def cargar_yalex(self):
        self._cargar_dialogo(self.editor_yalex, "Cargar archivo YALex")

    def cargar_yapar(self):
        self._cargar_dialogo(self.editor_yapar, "Cargar archivo YAPar")

    def cargar_entrada(self):
        self._cargar_dialogo(self.editor_entrada, "Cargar archivo de entrada")

    def _cargar_dialogo(self, editor, titulo):
        ruta = filedialog.askopenfilename(
            title=titulo,
            filetypes=editor.extensiones_dialogo,
            initialdir=str(INPUT_DIR if INPUT_DIR.exists() else PROJECT_ROOT),
        )
        if not ruta:
            return
        try:
            editor.cargar(ruta)
            self._set_estado(f"Cargado: {ruta}")
        except OSError as exc:
            self._reportar_error(f"No se pudo abrir {ruta}\n{exc}")

    # ── Guardar ───────────────────────────────────────────────────────

    def _guardar_editor(self, clave):
        editor = self._editores[clave]
        if editor.vacio() and editor.ruta is None:
            return
        try:
            if editor.ruta is None:
                ruta = filedialog.asksaveasfilename(
                    title=f"Guardar como ({clave})",
                    initialdir=str(INPUT_DIR if INPUT_DIR.exists() else PROJECT_ROOT),
                    filetypes=editor.extensiones_dialogo,
                )
                if not ruta:
                    return
                editor.guardar(ruta)
            else:
                editor.guardar()
            self._set_estado(f"Guardado: {editor.ruta}")
        except OSError as exc:
            self._reportar_error(f"No se pudo guardar\n{exc}")

    def guardar_todo(self):
        for clave in self._editores:
            self._guardar_editor(clave)

    # ── Compilar ──────────────────────────────────────────────────────

    def compilar(self):
        if self.proceso_activo:
            messagebox.showinfo("Compis IDE",
                                "Ya hay un proceso en ejecucion.")
            return
        self.proceso_activo = True
        self._set_estado("Compilando...")
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
                "Verifica que g++ este en el PATH "
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

        self.root.after(0, lambda: self._fin_compilacion(proc.returncode, salida,
                                                         proc.stderr))

    def _fin_compilacion(self, codigo, salida, stderr):
        self._escribir("salida", salida)
        if codigo == 0 and EJECUTABLE_PATH.exists():
            self._escribir("errores", "")
            self._set_estado("Compilacion exitosa")
            self._seleccionar_tab("salida")
        else:
            self._escribir(
                "errores",
                f"Compilacion fallo (codigo {codigo}).\n\n{stderr or salida}")
            self._seleccionar_tab("errores")
            self._set_estado("Compilacion con errores")
        self.proceso_activo = False

    def _fin_compilacion_fallida(self, mensaje):
        self._reportar_error(mensaje)
        self.proceso_activo = False

    # ── Ejecutar analisis ─────────────────────────────────────────────

    def ejecutar(self):
        if self.proceso_activo:
            messagebox.showinfo("Compis IDE",
                                "Ya hay un proceso en ejecucion.")
            return

        faltantes = [c for c, e in self._editores.items() if e.ruta is None]
        if faltantes:
            self._reportar_error(
                "Cargá los tres archivos antes de ejecutar:\n"
                "  - YALex (.yal)\n"
                "  - YAPar (.yapar)\n"
                "  - Entrada (.txt)\n"
                f"Faltan: {', '.join(faltantes)}"
            )
            return

        for editor in self._editores.values():
            if not Path(editor.ruta).exists():
                self._reportar_error(
                    f"El archivo {editor.ruta} no existe en disco. "
                    "Guardá antes de ejecutar.")
                return

        if not EJECUTABLE_PATH.exists():
            self._reportar_error(
                f"No se encontró el ejecutable en:\n  {EJECUTABLE_PATH}\n\n"
                "Pulsá Compilar antes de ejecutar.")
            return

        try:
            for editor in self._editores.values():
                editor.guardar()
        except OSError as exc:
            self._reportar_error(
                f"No se pudieron guardar los cambios antes de ejecutar:\n{exc}")
            return

        self.proceso_activo = True
        self._set_estado("Ejecutando analisis...")
        threading.Thread(target=self._ejecutar_thread, daemon=True).start()

    def _ejecutar_thread(self):
        argv = [
            str(EJECUTABLE_PATH),
            str(self.editor_yalex.ruta),
            str(self.editor_yapar.ruta),
            str(self.editor_entrada.ruta),
        ]
        try:
            proc = subprocess.run(
                argv,
                cwd=str(PROJECT_ROOT),
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
            )
        except OSError as exc:
            self.root.after(0, lambda: self._fin_ejecucion_fallida(
                f"No se pudo ejecutar el compilador:\n{exc}"))
            return

        self.root.after(0, lambda: self._fin_ejecucion(proc))

    def _fin_ejecucion(self, proc):
        stdout, stderr, codigo = proc.stdout, proc.stderr, proc.returncode
        salida_total = stdout
        if stderr:
            salida_total += "\n--- stderr ---\n" + stderr

        self._escribir("salida", salida_total)
        secciones = dividir_salida(stdout, stderr)
        for clave in ("tokens", "validaciones", "ff", "ll1", "traza",
                      "resultado", "errores"):
            self._escribir(clave, secciones.get(clave, ""))

        resumen = self._construir_resumen(stdout, codigo)
        actuales = self.paneles["resultado"].get("1.0", "end-1c")
        if actuales == "(vacio)":
            actuales = ""
        self._escribir(
            "resultado",
            (actuales + ("\n\n" if actuales else "") + resumen).strip())

        if "ACEPTADO" in stdout:
            self._set_estado("Analisis ACEPTADO")
            self._seleccionar_tab("resultado")
        elif "RECHAZADO" in stdout or codigo != 0:
            self._set_estado(f"Analisis RECHAZADO (codigo {codigo})")
            self._seleccionar_tab("errores" if stderr or codigo != 0
                                  else "resultado")
        else:
            self._set_estado(f"Analisis termino (codigo {codigo})")
            self._seleccionar_tab("salida")

        self.proceso_activo = False

    def _fin_ejecucion_fallida(self, mensaje):
        self._reportar_error(mensaje)
        self.proceso_activo = False

    def _construir_resumen(self, stdout, codigo):
        if "ACEPTADO" in stdout:
            veredicto = "ACEPTADO"
        elif "RECHAZADO" in stdout:
            veredicto = "RECHAZADO"
        else:
            veredicto = "SIN VEREDICTO"
        return (
            f"Veredicto del compilador: {veredicto}\n"
            f"Codigo de salida del proceso: {codigo}"
        )

    # ── Errores ───────────────────────────────────────────────────────

    def _reportar_error(self, mensaje):
        self._escribir("errores", mensaje)
        self._seleccionar_tab("errores")
        self._set_estado("Error")


# ──────────────────────────────────────────────────────────────────────
# Entrada
# ──────────────────────────────────────────────────────────────────────

def main():
    root = tk.Tk()
    try:
        estilo = ttk.Style()
        if "clam" in estilo.theme_names():
            estilo.theme_use("clam")
    except tk.TclError:
        pass
    IDE(root)
    root.mainloop()


if __name__ == "__main__":
    main()
