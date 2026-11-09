#!/usr/bin/env python3
"""Pruebas del backend MIPS: integracion (CLI) y extremo a extremo (MARS).

Todo pasa por la CLI publica (`./compiscript --mips <archivo> -o <salida>`):
ningun caso usa un .asm escrito a mano.

Suites:
  integracion   Sin MARS. Programas validos (tests/mips/valid, tests/mips/e2e)
                deben dar exit 0 y un .asm no vacio con .text y main. Los
                invalidos (tests/mips/invalid, tests/fixtures/invalid,
                tests/tac/errores) deben dar exit != 0, exactamente los
                codigos de sus lineas '// expect: XXX000' y NINGUN .asm:
                antes de compilar se deja un .asm "viejo" en la ruta de
                salida y se verifica que el compilador lo borre. MARS no se
                invoca para ellos.
  e2e           Con MARS. Cada programa valido se compila y se ejecuta; la
                salida del programa debe ser EXACTAMENTE la de '// expect-out:'
                (sin esa cabecera, ninguna), el codigo de salida el de
                '// expect-exit: N' (0 por defecto) y debe terminar con un
                syscall de salida (ver tests/mips/mars.py).
  todo          Las dos.

Sin MARS, la suite e2e queda en SKIP con el motivo (modo desarrollo). Con
--estricto (validacion de entrega) la falta de MARS es un error.

Cada caso corre en su propio directorio temporal. Los resultados quedan en
tests/results/mips_<suite>.json: por caso test_id, fuente, comando, etapa,
estado PASS/FAIL/SKIP, motivo, stdout esperado y obtenido, codigos de
salida, terminacion en MARS, errores y duracion.

Uso:
  python3 tests/mips/run_mips_tests.py [--bin ./compiscript] [--suite todo]
                                       [--estricto] [--filtro texto]
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
import time
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import mars  # noqa: E402

RAIZ = Path(__file__).resolve().parent.parent.parent
MIPS = Path(__file__).resolve().parent
RESULTADOS = RAIZ / "tests" / "results"

DIRS_VALIDOS = [MIPS / "valid", MIPS / "e2e"]
# Los programas de tests/tac (golden files del TAC) tambien se ejecutan en
# MARS: los que declaran su salida con '// expect-out:'.
DIR_TAC = RAIZ / "tests" / "tac"
DIRS_INVALIDOS = [MIPS / "invalid", MIPS / "invalid_backend", MIPS / "e2e" / "invalid",
                  RAIZ / "tests" / "fixtures" / "invalid", RAIZ / "tests" / "tac" / "errores"]

# CG001 tiene 2 letras; SEM/SYN tienen 3.
PATRON_EXPECT = re.compile(r"^\s*//\s*expect:\s*([A-Z]{2,3}\d{3})\s*$")
PATRON_OUT = re.compile(r"^\s*//\s*expect-out:\s*$")
PATRON_EXIT = re.compile(r"^\s*//\s*expect-exit:\s*(\d+)\s*$")
PATRON_DIAG = re.compile(r"^\s*\[([A-Z]{2,3}\d{3})\]\s+(error|warning)")

COLOR = sys.stdout.isatty() and os.environ.get("NO_COLOR") is None


def _c(t, code):
    return f"\033[{code}m{t}\033[0m" if COLOR else t


def imprimir(estado, msg, detalle=None):
    color = {"PASS": "32", "FAIL": "31;1", "SKIP": "33"}[estado]
    print(f"{_c(f'{estado:<6}', color)}  {msg}")
    for linea in (detalle or "").rstrip("\n").splitlines():
        print(f"          {linea}")


# ---------------------------------------------------------------------------
# Cabeceras de los casos
# ---------------------------------------------------------------------------

def codigos_esperados(texto):
    return [m.group(1) for m in map(PATRON_EXPECT.match, texto.splitlines()) if m]


def salida_esperada(texto):
    """Lineas de '// expect-out:' o None si no hay cabecera."""
    lineas = texto.splitlines()
    for i, ln in enumerate(lineas):
        if PATRON_OUT.match(ln):
            out = []
            for sig in lineas[i + 1:]:
                if not sig.startswith("//"):
                    break
                out.append(sig[3:] if sig.startswith("// ") else sig[2:])
            return out
    return None


def exit_esperado(texto):
    for ln in texto.splitlines():
        m = PATRON_EXIT.match(ln)
        if m:
            return int(m.group(1))
    return 0


def codigos_obtenidos(texto):
    return [m.group(1) for m in map(PATRON_DIAG.match, texto.splitlines()) if m]


def rel(p: Path) -> str:
    try:
        return str(p.relative_to(RAIZ))
    except ValueError:
        return str(p)


def casos_validos(filtro):
    yield from casos(DIRS_VALIDOS, filtro)
    for cps in sorted(DIR_TAC.glob("*.cps")):
        if filtro in rel(cps) and salida_esperada(cps.read_text(encoding="utf-8")) is not None:
            yield cps


def casos(dirs, filtro):
    for d in dirs:
        if d.is_dir():
            for cps in sorted(d.glob("*.cps")):
                if filtro in rel(cps):
                    yield cps


# ---------------------------------------------------------------------------
# Compilacion por la CLI
# ---------------------------------------------------------------------------

def compilar(binario, cps: Path, asm: Path, timeout=60):
    comando = [binario, "--mips", str(cps), "-o", str(asm)]
    inicio = time.monotonic()
    try:
        p = subprocess.run(comando, cwd=RAIZ, capture_output=True, text=True,
                           encoding="utf-8", errors="replace", timeout=timeout,
                           stdin=subprocess.DEVNULL)
        codigo, salida = p.returncode, (p.stdout + p.stderr).replace("\r\n", "\n")
    except subprocess.TimeoutExpired:
        codigo, salida = None, f"el compilador no termino en {timeout} s"
    return comando, codigo, salida, time.monotonic() - inicio


def registro(cps, suite, comando):
    return {"test_id": cps.stem, "suite": suite, "fuente": rel(cps),
            "comando": [rel(Path(c)) if c.startswith(str(RAIZ)) else c for c in comando],
            "etapa": "compilacion", "estado": "PASS", "motivo": "",
            "duracion_s": 0.0}


# ---------------------------------------------------------------------------
# Suites
# ---------------------------------------------------------------------------

def suite_integracion(binario, filtro):
    out = []
    for cps in casos_validos(filtro):
        with tempfile.TemporaryDirectory(prefix="mips-int-") as tmp:
            asm = Path(tmp) / f"{cps.stem}.asm"
            comando, codigo, salida, dur = compilar(binario, cps, asm)
            r = registro(cps, "integracion", comando)
            r.update(duracion_s=round(dur, 3), exit_obtenido=codigo)
            diags = codigos_obtenidos(salida)
            if codigo != 0 or diags:
                r.update(estado="FAIL", motivo="el compilador rechazo un programa valido",
                         errores=salida.splitlines()[-20:])
            elif not asm.is_file() or asm.stat().st_size == 0:
                r.update(estado="FAIL", motivo="exit 0 pero sin .asm (o vacio)")
            else:
                cuerpo = asm.read_text(encoding="utf-8", errors="replace")
                faltan = [x for x in (".data", ".text", "main:") if x not in cuerpo]
                if faltan:
                    r.update(estado="FAIL", motivo=f".asm incompleto, falta {faltan}")
                else:
                    r.update(motivo=f".asm de {len(cuerpo.splitlines())} lineas")
        out.append(r)

    for cps in casos(DIRS_INVALIDOS, filtro):
        texto = cps.read_text(encoding="utf-8")
        esperados = codigos_esperados(texto)
        with tempfile.TemporaryDirectory(prefix="mips-neg-") as tmp:
            asm = Path(tmp) / f"{cps.stem}.asm"
            # Un .asm de un intento anterior exitoso en la misma ruta: el
            # intento fallido tiene que borrarlo (N15).
            asm.write_text("# asm de una compilacion anterior\n", encoding="utf-8")
            comando, codigo, salida, dur = compilar(binario, cps, asm)
            r = registro(cps, "integracion", comando)
            obtenidos = codigos_obtenidos(salida)
            r.update(duracion_s=round(dur, 3), exit_obtenido=codigo,
                     codigos_esperados=esperados, codigos_obtenidos=obtenidos)
            if not esperados:
                r.update(estado="FAIL", motivo="falta '// expect:' en el caso")
            elif codigo in (0, None):
                r.update(estado="FAIL", motivo=f"debia fallar (exit {codigo})",
                         errores=salida.splitlines()[-20:])
            elif asm.exists():
                r.update(estado="FAIL", motivo="quedo un .asm (el viejo no se borro)")
            elif Counter(esperados) != Counter(obtenidos):
                r.update(estado="FAIL", motivo=f"codigos {obtenidos} != {esperados}",
                         errores=salida.splitlines()[-20:])
            else:
                r.update(motivo=f"{', '.join(obtenidos)} y sin .asm")
        out.append(r)
    return out


def suite_e2e(binario, filtro, cfg, motivo_sin_mars):
    out = []
    for cps in casos_validos(filtro):
        texto = cps.read_text(encoding="utf-8")
        esperado = salida_esperada(texto) or []
        exit_esp = exit_esperado(texto)
        with tempfile.TemporaryDirectory(prefix="mips-e2e-") as tmp:
            asm = Path(tmp) / f"{cps.stem}.asm"
            comando, codigo, salida, dur = compilar(binario, cps, asm)
            r = registro(cps, "e2e", comando)
            r.update(duracion_s=round(dur, 3), stdout_esperado=esperado,
                     exit_esperado=exit_esp)
            if codigo != 0 or not asm.is_file() or asm.stat().st_size == 0:
                r.update(estado="FAIL", motivo="no se genero el .asm",
                         errores=salida.splitlines()[-20:])
                out.append(r)
                continue
            if cfg is None:
                r.update(estado="SKIP", etapa="ejecucion", motivo=motivo_sin_mars)
                out.append(r)
                continue
            m = mars.ejecutar(cfg, asm)
            obtenido = m.stdout.rstrip("\n").split("\n") if m.stdout else []
            r.update(etapa="ejecucion", comando_mars=[rel(Path(c)) if c.startswith(str(RAIZ))
                                                       else c for c in m.comando[:-1]] + [asm.name],
                     stdout_obtenido=obtenido, exit_obtenido=m.codigo,
                     terminacion=m.terminacion, errores=m.errores,
                     duracion_s=round(dur + m.duracion_s, 3))
            if m.terminacion == "error_ensamblado":
                r.update(estado="FAIL", etapa="ensamblado", motivo="MARS rechazo el .asm")
            elif not m.termino_bien:
                r.update(estado="FAIL", motivo=f"no termino bien en MARS ({m.terminacion})")
            elif obtenido != esperado:
                r.update(estado="FAIL", motivo="la salida del programa no es la esperada")
            elif m.codigo != exit_esp:
                r.update(estado="FAIL", motivo=f"codigo de salida {m.codigo}, "
                                               f"se esperaba {exit_esp}")
            else:
                r.update(motivo=f"salida exacta ({len(obtenido)} lineas), exit {m.codigo}")
        out.append(r)
    return out


# ---------------------------------------------------------------------------

def resumir(nombre, regs):
    n = Counter(r["estado"] for r in regs)
    fallos = [r for r in regs if r["estado"] == "FAIL"]
    del_compilador = sum(1 for r in fallos if r["etapa"] == "compilacion")
    return {"suite": nombre, "ejecutados": len(regs), "pass": n["PASS"],
            "fail": n["FAIL"], "skip": n["SKIP"],
            "fallos_compilador": del_compilador,
            "fallos_mars": len(fallos) - del_compilador}


def guardar(nombre, regs, resumen, cfg, binario):
    RESULTADOS.mkdir(parents=True, exist_ok=True)
    datos = {
        "generado": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "compilador": rel(Path(binario)),
        "mars": {"version": cfg.version, "jar": rel(Path(cfg.jar)),
                 "max_pasos": mars.MAX_PASOS_DEFECTO,
                 "timeout_s": mars.TIMEOUT_DEFECTO} if cfg else None,
        "resumen": resumen,
        "casos": regs,
    }
    ruta = RESULTADOS / f"mips_{nombre}.json"
    ruta.write_text(json.dumps(datos, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return ruta


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=str(RAIZ / "compiscript"))
    ap.add_argument("--suite", choices=["integracion", "e2e", "todo"], default="todo")
    ap.add_argument("--estricto", action="store_true",
                    help="validacion de entrega: sin MARS es un error, no un SKIP")
    ap.add_argument("--filtro", default="", help="solo casos cuya ruta contenga este texto")
    args = ap.parse_args()
    binario = str(Path(args.bin).resolve())
    if not Path(binario).is_file():
        print(f"No existe el compilador {binario}: corre `make` primero.")
        return 2

    cfg, motivo = (None, "")
    if args.suite in ("e2e", "todo"):
        cfg, motivo = mars.detectar_mars()
        if cfg is None:
            if args.estricto:
                print(f"BLOQUEADO: la validacion estricta necesita MARS.\n  {motivo}")
                return 2
            imprimir("SKIP", f"MARS no disponible: {motivo}")
        else:
            print(f"MARS: {cfg.version} ({rel(Path(cfg.jar))}), "
                  f"limite {mars.MAX_PASOS_DEFECTO} pasos / {mars.TIMEOUT_DEFECTO:g} s")

    suites = []
    if args.suite in ("integracion", "todo"):
        suites.append(("integracion", suite_integracion(binario, args.filtro)))
    if args.suite in ("e2e", "todo"):
        suites.append(("e2e", suite_e2e(binario, args.filtro, cfg, motivo)))

    hubo_fallos = False
    for nombre, regs in suites:
        print(f"== {nombre}")
        for r in regs:
            detalle = None
            if r["estado"] == "FAIL":
                partes = []
                if "stdout_esperado" in r and "stdout_obtenido" in r:
                    partes += ["esperado:"] + r["stdout_esperado"] + ["obtenido:"] + r["stdout_obtenido"][-30:]
                partes += r.get("errores", [])
                detalle = "\n".join(partes)
            imprimir(r["estado"], f"{r['fuente']}: {r['motivo']}", detalle)
        resumen = resumir(nombre, regs)
        if not args.filtro:
            ruta = guardar(nombre, regs, resumen, cfg, binario)
            destino = f" -> {rel(ruta)}"
        else:
            destino = ""
        print(f"---- {nombre}: {resumen['ejecutados']} ejecutados, {resumen['pass']} PASS, "
              f"{resumen['fail']} FAIL, {resumen['skip']} SKIP (fallos del compilador: "
              f"{resumen['fallos_compilador']}, en MARS: {resumen['fallos_mars']}){destino}")
        hubo_fallos |= resumen["fail"] > 0
        if args.estricto and resumen["skip"] > 0:
            print(f"BLOQUEADO: la validacion estricta no admite SKIP ({resumen['skip']}).")
            hubo_fallos = True
    return 1 if hubo_fallos else 0


if __name__ == "__main__":
    sys.exit(main())
