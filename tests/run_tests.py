#!/usr/bin/env python3
"""Bateria de pruebas de Compiscript.

Suites:
  semantica  tests/fixtures/valid/*.cps   deben compilar sin errores (codigo 0)
             tests/fixtures/invalid/*.cps deben fallar (codigo 1) con
                                          exactamente los codigos que piden
                                          sus lineas '// expect: SEMxxx'
  tac        tests/tac/*.cps              la salida de 'compiscript --tac'
                                          debe ser identica a <caso>.tac
             tests/tac/errores/*.cps      no debe aparecer TAC y deben salir
                                          los codigos de '// expect:'

Uso:
  python3 tests/run_tests.py [--bin ./compiscript] [semantica|tac|todo]
  python3 tests/run_tests.py --bin ./compiscript tac --update

--update regenera los .tac esperados con la salida actual. Revisar SIEMPRE
el 'git diff' de los .tac antes de commitear: si no, un bug se vuelve "lo
esperado".
"""

import argparse
import difflib
import os
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
FIXTURES = RAIZ / "tests" / "fixtures"
TAC_DIR = RAIZ / "tests" / "tac"

# '// expect: SEM003' (una linea por cada error esperado)
PATRON_EXPECT = re.compile(r"^\s*//\s*expect:\s*([A-Z]{3}\d{3})\s*$")
# '  [SEM003] error 2:1: mensaje', el formato de printDiagnostics en main.cpp
PATRON_DIAG = re.compile(r"^\s*\[([A-Z]{3}\d{3})\]\s+(error|warning)\s+\d+:\d+:")

COLOR = sys.stdout.isatty() and os.environ.get("NO_COLOR") is None


def _c(texto, codigo):
    return f"\033[{codigo}m{texto}\033[0m" if COLOR else texto


def ok(msg):
    print(f"{_c('OK    ', '32')}  {msg}")


def fallo(msg, detalle=None):
    print(f"{_c('FALLO ', '31;1')}  {msg}")
    if detalle:
        for linea in detalle.rstrip("\n").splitlines():
            print(f"          {linea}")


def rel(p):
    return p.relative_to(RAIZ).as_posix()


def esperados(cps):
    codigos = []
    for linea in cps.read_text(encoding="utf-8").splitlines():
        m = PATRON_EXPECT.match(linea)
        if m:
            codigos.append(m.group(1))
    return codigos


def correr(binario, *args):
    proc = subprocess.run([binario, *args], cwd=RAIZ, capture_output=True,
                          text=True, encoding="utf-8", errors="replace")
    return proc.returncode, proc.stdout.replace("\r\n", "\n"), proc.stderr


def codigos_emitidos(stdout):
    return [m.group(1) for m in map(PATRON_DIAG.match, stdout.splitlines()) if m]


def comparar_codigos(esperado, obtenido):
    """None si coinciden (como multiconjunto), o un texto con la diferencia."""
    if Counter(esperado) == Counter(obtenido):
        return None
    return (f"esperados: {', '.join(sorted(esperado)) or '(ninguno)'}\n"
            f"obtenidos: {', '.join(sorted(obtenido)) or '(ninguno)'}")


class Resultado:
    def __init__(self):
        self.ok = 0
        self.fallos = []

    def bien(self, msg):
        self.ok += 1
        ok(msg)

    def mal(self, msg, detalle=None):
        self.fallos.append(msg)
        fallo(msg, detalle)


def titulo(texto):
    print()
    print(_c(f"== {texto} ==", "1"))


# --------------------------------------------------------------------------
# Suite semantica
# --------------------------------------------------------------------------

def suite_semantica(binario, r):
    titulo("Semantica: casos validos (tests/fixtures/valid)")
    for cps in sorted((FIXTURES / "valid").glob("*.cps")):
        codigo, out, err = correr(binario, cps.as_posix())
        if codigo == 0:
            r.bien(rel(cps))
        else:
            diags = "\n".join(l for l in out.splitlines() if PATRON_DIAG.match(l))
            r.mal(f"{rel(cps)}: se esperaba que compilara sin errores",
                  diags or err or f"codigo de salida {codigo}")

    titulo("Semantica: casos fallidos (tests/fixtures/invalid)")
    for cps in sorted((FIXTURES / "invalid").glob("*.cps")):
        exp = esperados(cps)
        if not exp:
            r.mal(f"{rel(cps)}: no tiene ninguna linea '// expect: CODIGO'")
            continue
        codigo, out, err = correr(binario, cps.as_posix())
        if codigo == 0:
            r.mal(f"{rel(cps)}: se esperaba un error y compilo",
                  f"esperados: {', '.join(exp)}")
            continue
        diferencia = comparar_codigos(exp, codigos_emitidos(out))
        if diferencia:
            r.mal(f"{rel(cps)}: codigos distintos a los esperados", diferencia)
        else:
            r.bien(f"{rel(cps)}  [{', '.join(exp)}]")


# --------------------------------------------------------------------------
# Suite de TAC (golden files)
# --------------------------------------------------------------------------

def suite_tac(binario, r, actualizar=False):
    titulo("TAC: casos exitosos (tests/tac/*.cps vs .tac)")
    for cps in sorted(TAC_DIR.glob("*.cps")):
        esperado_path = cps.with_suffix(".tac")
        codigo, out, err = correr(binario, "--tac", cps.as_posix())
        if codigo != 0:
            diags = "\n".join(l for l in out.splitlines() if PATRON_DIAG.match(l))
            r.mal(f"{rel(cps)}: no compilo (se esperaba TAC)",
                  diags or err or f"codigo de salida {codigo}")
            continue
        if "TODO(" in out:
            pendientes = "\n".join(sorted({l.strip() for l in out.splitlines() if "TODO(" in l}))
            r.mal(f"{rel(cps)}: el TAC tiene construcciones sin implementar", pendientes)
            continue
        if actualizar:
            anterior = esperado_path.read_text(encoding="utf-8") if esperado_path.exists() else None
            esperado_path.write_text(out, encoding="utf-8", newline="\n")
            estado = "creado" if anterior is None else ("actualizado" if anterior != out else "sin cambios")
            r.bien(f"{rel(esperado_path)} ({estado})")
            continue
        if not esperado_path.exists():
            r.mal(f"{rel(cps)}: falta {esperado_path.name}",
                  "genera los esperados con 'make test-tac-update' y revisa el diff")
            continue
        esperado = esperado_path.read_text(encoding="utf-8").replace("\r\n", "\n")
        if esperado == out:
            r.bien(rel(cps))
        else:
            diff = "".join(difflib.unified_diff(
                esperado.splitlines(keepends=True), out.splitlines(keepends=True),
                fromfile=f"{esperado_path.name} (esperado)",
                tofile=f"{cps.name} (obtenido)"))
            r.mal(f"{rel(cps)}: el TAC difiere del esperado", diff)

    titulo("TAC: casos fallidos (tests/tac/errores)")
    for cps in sorted((TAC_DIR / "errores").glob("*.cps")):
        exp = esperados(cps)
        if not exp:
            r.mal(f"{rel(cps)}: no tiene ninguna linea '// expect: CODIGO'")
            continue
        codigo, out, err = correr(binario, "--tac", cps.as_posix())
        problemas = []
        if codigo == 0:
            problemas.append("compilo sin errores (codigo de salida 0)")
        # Con errores, --tac solo imprime 'Diagnosticos:' y sus lineas.
        sobrantes = [l for l in out.splitlines()
                     if l.strip() and l.strip() != "Diagnosticos:" and not PATRON_DIAG.match(l)]
        if sobrantes:
            problemas.append("aparecio salida que no es un diagnostico (¿TAC?):\n  "
                             + "\n  ".join(sobrantes[:5]))
        diferencia = comparar_codigos(exp, codigos_emitidos(out))
        if diferencia:
            problemas.append(diferencia)
        if problemas:
            r.mal(f"{rel(cps)}: no se comporto como caso fallido", "\n".join(problemas))
        else:
            r.bien(f"{rel(cps)}  [{', '.join(exp)}] sin TAC")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("suite", nargs="?", default="todo", choices=["semantica", "tac", "todo"])
    ap.add_argument("--bin", default="./compiscript",
                    help="ruta al ejecutable (por defecto ./compiscript)")
    ap.add_argument("--update", action="store_true",
                    help="regenera los .tac esperados (solo suite tac)")
    args = ap.parse_args()

    binario = str((RAIZ / args.bin).resolve()) if not os.path.isabs(args.bin) else args.bin
    if not Path(binario).exists():
        print(f"No existe el ejecutable {args.bin}. Compila primero con 'make'.")
        return 2

    r = Resultado()
    if args.suite in ("semantica", "todo") and not args.update:
        suite_semantica(binario, r)
    if args.suite in ("tac", "todo"):
        suite_tac(binario, r, actualizar=args.update)

    print()
    print("----")
    resumen = f"{r.ok} ok, {len(r.fallos)} fallos"
    print(_c(resumen, "32;1" if not r.fallos else "31;1"))
    if r.fallos:
        print("Fallaron:")
        for f in r.fallos:
            print(f"  - {f}")
    return 0 if not r.fallos else 1


if __name__ == "__main__":
    sys.exit(main())
