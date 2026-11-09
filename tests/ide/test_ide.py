#!/usr/bin/env python3
"""Prueba automatizada de la IDE (ide/ide_app.py) con Tkinter real.

Abre la IDE de verdad (necesita un display: WSLg o X11), la maneja como lo
haria un usuario (cargar archivos, pegar texto en el editor, F5/F6/F7,
ejecutar en MARS) y revisa lo que muestra:

  - escenario A: cargar una clase sola, compilarla, pestana MIPS vigente;
  - escenario D: clase con errores, todos los diagnosticos visibles y la
    pestana MIPS sin ensamblador (no se muestra uno viejo);
  - escenario E: pegar una clase en el editor (sin archivo), compilarla y
    ejecutarla en MARS;
  - recompilar tras corregir un error, archivo vacio, ruta con espacios y
    tildes, archivo que ya no existe, AST/tabla de simbolos/TAC llenos,
    F5 (make build), F7 con resumen PASS/FAIL/SKIP, MARS colgado (la
    interfaz sigue respondiendo) y MARS ausente (error visible).

Sin display la prueba queda en SKIP; con COMPISCRIPT_ESTRICTO=1 es un fallo.
El estado de la IDE (.ide_state.json) se redirige a un temporal para no
tocar el del usuario.

Uso: python3 tests/ide/test_ide.py
"""

from __future__ import annotations

import json
import os
import shutil
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(RAIZ / "ide"))
sys.path.insert(0, str(RAIZ / "tests" / "mips"))
import mars  # noqa: E402

ESTRICTO = os.environ.get("COMPISCRIPT_ESTRICTO") == "1"

try:
    import tkinter as tk
    root = tk.Tk()
except Exception as exc:  # sin display o sin python3-tk
    print(f"SKIP    ide: no se puede abrir una ventana ({exc})")
    print("---- ide: 1 ejecutados, 0 PASS, 0 FAIL, 1 SKIP")
    sys.exit(1 if ESTRICTO else 0)

import ide_app  # noqa: E402

TMP = Path(tempfile.mkdtemp(prefix="compiscript-ide-"))
ide_app.ESTADO_IDE_PATH = TMP / "estado.json"
CFG, MOTIVO_MARS = mars.detectar_mars()

ide = ide_app.IDE(root)
editor = ide._editores["fuente"]
mensajes_error = []
_reportar = ide._reportar_error
ide._reportar_error = lambda m: (mensajes_error.append(m), _reportar(m))

resultados = []  # (estado, nombre, detalle)


def check(cond, nombre, detalle=""):
    resultados.append(("PASS" if cond else "FAIL", nombre, "" if cond else detalle))


def skip(nombre, motivo):
    resultados.append(("SKIP", nombre, motivo))


def panel(clave):
    return ide.paneles[clave].get("1.0", "end-1c")


def esperar(cond, siguiente, limite=180):
    inicio = time.time()

    def paso():
        if cond() or time.time() - inicio > limite:
            siguiente()
        else:
            root.after(100, paso)
    root.after(100, paso)


def compilar_y(siguiente):
    ide.ejecutar()
    esperar(lambda: not ide.proceso_activo, siguiente)


def mars_y(siguiente):
    ide.ejecutar_mars()
    esperar(lambda: not ide._mars_activo, siguiente)


def salida_mars():
    # Sin las lineas que agrega la IDE ("[terminó con código 0]").
    return [l for l in panel("mars").splitlines() if l and not l.startswith("[")]


def poner_texto(texto):
    editor.ruta = None
    editor.text.delete("1.0", "end")
    editor.text.insert("1.0", texto)


E2E = RAIZ / "tests" / "mips" / "e2e"
CLASE_B = (E2E / "B_clase_ejecutada.cps").read_text(encoding="utf-8")
SALIDA_B = ["Ana", "120", "1", "Luis", "60", "2", "121"]


# --- Escenario A: clase sola -------------------------------------------------
def a1():
    editor.cargar(str(E2E / "A_clase_completa.cps"))
    compilar_y(a2)


def a2():
    check(ide_app.ASM_PATH.exists(), "A: genera output/programa.asm")
    check(".text" in panel("mips") and "main:" in panel("mips"), "A: pestana MIPS con el ensamblador",
          panel("mips")[:200])
    check("A_clase_completa.cps" in ide._mips_estado.cget("text"), "A: la pestana dice de que fuente es",
          ide._mips_estado.cget("text"))
    check(not ide._ultimos_diagnosticos, "A: sin diagnosticos", str(ide._ultimos_diagnosticos))
    check("func main" in panel("tac"), "A: pestana TAC llena")
    check("AST:" in ide._ultimo_stdout and "Tabla de simbolos:" in ide._ultimo_stdout,
          "A: AST y tabla de simbolos generados")
    d1()


# --- Escenario D: clase invalida ---------------------------------------------
def d1():
    editor.cargar(str(E2E / "invalid" / "D_clase_invalida.cps"))
    compilar_y(d2)


def d2():
    codigos = sorted(d["codigo"] for d in ide._ultimos_diagnosticos)
    check(codigos == ["SEM002", "SEM010", "SEM013"], "D: los tres errores en Diagnosticos", str(codigos))
    check(not ide_app.ASM_PATH.exists(), "D: no queda el .asm de la compilacion anterior (A)")
    check(panel("mips").startswith("No se genera MIPS"), "D: la pestana MIPS avisa que no hay ensamblador",
          panel("mips")[:200])
    check(ide._mips_estado.cget("text") == "Sin ensamblador.", "D: indicador de no vigencia",
          ide._mips_estado.cget("text"))
    e1()


# --- Escenario E: pegar en el editor (sin archivo) -----------------------------
def e1():
    editor.ruta = None
    editor.text.delete("1.0", "end")
    root.clipboard_clear()
    root.clipboard_append(CLASE_B)
    editor.text.focus_set()
    editor.text.event_generate("<<Paste>>")
    root.update()
    check(editor.text.get("1.0", "end-1c").strip() == CLASE_B.strip(), "E: el texto pegado llega al editor")
    compilar_y(e2)


def e2():
    check(ide_app.ASM_PATH.exists() and not ide._ultimos_diagnosticos,
          "E: compila lo pegado sin guardarlo a mano", str(ide._ultimos_diagnosticos))
    if CFG is None:
        skip("E: ejecucion en MARS", MOTIVO_MARS)
        r1()
        return
    mars_y(e3)


def e3():
    check(salida_mars() == SALIDA_B, "E: MARS ejecuta lo pegado con la salida esperada", str(salida_mars()))
    r1()


# --- Recompilar tras corregir -------------------------------------------------
def r1():
    poner_texto("let x: integer = 5;\nprint(x + y);\n")
    compilar_y(r2)


def r2():
    check([d["codigo"] for d in ide._ultimos_diagnosticos] == ["SEM001"], "recompilar: primero el error",
          str(ide._ultimos_diagnosticos))
    check(not ide_app.ASM_PATH.exists(), "recompilar: sin .asm mientras hay error")
    editor.text.delete("2.0", "2.end")
    editor.text.insert("2.0", "print(x + 1);")
    compilar_y(r3)


def r3():
    check(ide_app.ASM_PATH.exists() and not ide._ultimos_diagnosticos, "recompilar: corregido, hay .asm")
    if CFG is None:
        skip("recompilar: MARS", MOTIVO_MARS)
        v1()
        return
    mars_y(lambda: (check(salida_mars() == ["6"], "recompilar: MARS imprime 6", str(salida_mars())), v1()))


# --- Archivo vacio, ruta con espacios y tildes, archivo inexistente -------------
def v1():
    vacio = TMP / "vacio.cps"
    vacio.write_text("", encoding="utf-8")
    editor.cargar(str(vacio))
    compilar_y(v2)


def v2():
    check(ide_app.ASM_PATH.exists() and not ide._ultimos_diagnosticos, "archivo vacio: compila (programa vacio)")
    carpeta = TMP / "carpeta con espacios" / "ñandú"
    carpeta.mkdir(parents=True)
    fuente = carpeta / "clase áé.cps"
    fuente.write_text(CLASE_B, encoding="utf-8")
    editor.cargar(str(fuente))
    compilar_y(v3)


def v3():
    check(ide_app.ASM_PATH.exists() and not ide._ultimos_diagnosticos, "ruta con espacios y tildes: compila",
          str(ide._ultimos_diagnosticos) + ide._ultimo_stderr)
    borrado = TMP / "borrado.cps"
    borrado.write_text("print(1);\n", encoding="utf-8")
    editor.cargar(str(borrado))
    borrado.unlink()
    mensajes_error.clear()
    ide.ejecutar()
    root.after(200, v4)


def v4():
    check(any("no existe" in m for m in mensajes_error), "archivo que ya no existe: error visible",
          str(mensajes_error))
    check(not ide.proceso_activo, "archivo que ya no existe: no queda un proceso colgado")
    f5()


# --- F5 (make build) y F7 (pruebas) -------------------------------------------
def f5():
    ide.compilar()
    esperar(lambda: not ide.proceso_activo, f5b)


def f5b():
    check("exitosa" in ide._toolbar_estado.cget("text"), "F5: make build termina bien",
          ide._toolbar_estado.cget("text"))
    # F7 con una salida de prueba conocida (correr `make test` entero aqui
    # tardaria minutos): 1 FAIL y 2 SKIP tienen que verse separados.
    falso = ("print('---- e2e: 5 ejecutados, 2 PASS, 1 FAIL, 2 SKIP');"
             "print('10 ok, 0 fallos'); raise SystemExit(1)")
    ide_app.COMANDO_PRUEBAS[:] = [sys.executable, "-c", falso]
    ide.correr_pruebas()
    esperar(lambda: not ide.proceso_activo, f7b)


def f7b():
    estado = ide._toolbar_estado.cget("text")
    check("12 PASS, 1 FAIL, 2 SKIP" in estado, "F7: resumen con PASS, FAIL y SKIP separados", estado)
    check(panel("pruebas").startswith("RESUMEN: 12 PASS, 1 FAIL, 2 SKIP"), "F7: resumen arriba de la salida",
          panel("pruebas")[:120])
    m1()


# --- MARS colgado y MARS ausente ----------------------------------------------
latidos = {"n": 0}


def latido():
    latidos["n"] += 1
    if ide._mars_activo:
        root.after(200, latido)


def m1():
    if CFG is None:
        skip("MARS colgado", MOTIVO_MARS)
        m3()
        return
    poner_texto("let i: integer = 0;\nwhile (true) {\n  i = i + 1;\n}\n")
    compilar_y(m2)


def m2():
    ide_app.MARS_TIMEOUT_S = 3
    latidos["n"] = 0
    inicio = time.time()
    ide.ejecutar_mars()
    latido()
    esperar(lambda: not ide._mars_activo,
            lambda: (check(time.time() - inicio < 15 and "detenido" in panel("mars"),
                           "MARS colgado: se detiene por tiempo", panel("mars")),
                     check(latidos["n"] >= 5, "MARS colgado: la interfaz sigue respondiendo",
                           f"latidos={latidos['n']}"),
                     m3()))


def m3():
    original = ide_app.resolver_mars
    ide_app.resolver_mars = lambda *_: None
    mensajes_error.clear()
    poner_texto("print(1);\n")
    compilar_y(lambda: m4(original))


def m4(original):
    ide.ejecutar_mars()
    ide_app.resolver_mars = original
    check(any("No se encontró MARS" in m for m in mensajes_error), "MARS ausente: error visible",
          str(mensajes_error))
    check(not ide._mars_activo, "MARS ausente: no queda nada ejecutando")
    fin()


def fin():
    root.destroy()


root.after(300, a1)
root.mainloop()
shutil.rmtree(TMP, ignore_errors=True)

for estado, nombre, detalle in resultados:
    print(f"{estado:<6}  ide/{nombre}" + (f"\n          {detalle}" if detalle else ""))
n = {k: sum(1 for r in resultados if r[0] == k) for k in ("PASS", "FAIL", "SKIP")}
(RAIZ / "tests" / "results").mkdir(parents=True, exist_ok=True)
(RAIZ / "tests" / "results" / "ide.json").write_text(json.dumps({
    "generado": datetime.now(timezone.utc).isoformat(timespec="seconds"),
    "resumen": {"suite": "ide", "ejecutados": len(resultados), "pass": n["PASS"],
                "fail": n["FAIL"], "skip": n["SKIP"]},
    "casos": [{"test_id": nombre, "estado": estado, "motivo": detalle}
              for estado, nombre, detalle in resultados]}, ensure_ascii=False, indent=2) + "\n",
    encoding="utf-8")
print(f"---- ide: {len(resultados)} ejecutados, {n['PASS']} PASS, {n['FAIL']} FAIL, {n['SKIP']} SKIP")
sys.exit(1 if n["FAIL"] or (ESTRICTO and n["SKIP"]) else 0)
