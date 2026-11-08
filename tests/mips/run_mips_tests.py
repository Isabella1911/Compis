#!/usr/bin/env python3
"""Pruebas del backend MIPS (Fase 3).

Suites:
  valid/             deben producir .asm (exit 0). Si hay '// expect-out:' y
                     MARS_JAR esta definido, se ejecuta en MARS y se compara
                     la salida. Sin MARS: SKIP de ejecucion (visible).
  invalid/           errores de frontend/semantico: exit != 0, sin .asm,
                     codigos '// expect: SEMxxx|SYNxxx'
  invalid_backend/   errores del backend: exit != 0, sin .asm,
                     codigos '// expect: CGxxx'

Uso:
  python3 tests/mips/run_mips_tests.py [--bin ./compiscript]
  MARS_JAR=/ruta/Mars.jar java ...  (opcional)
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent.parent
MIPS = Path(__file__).resolve().parent
# CG001 tiene 2 letras; SEM/SYN tienen 3.
PATRON_EXPECT = re.compile(r"^\s*//\s*expect:\s*([A-Z]{2,3}\d{3})\s*$")
PATRON_OUT = re.compile(r"^\s*//\s*expect-out:\s*$")
PATRON_DIAG = re.compile(r"^\s*\[([A-Z]{2,3}\d{3})\]\s+(error|warning)")

COLOR = sys.stdout.isatty() and os.environ.get("NO_COLOR") is None


def _c(t, code):
    return f"\033[{code}m{t}\033[0m" if COLOR else t


def ok(msg):
    print(f"{_c('OK    ', '32')}  {msg}")


def skip(msg):
    print(f"{_c('SKIP  ', '33')}  {msg}")


def fallo(msg, detalle=None):
    print(f"{_c('FALLO ', '31;1')}  {msg}")
    if detalle:
        for linea in detalle.rstrip("\n").splitlines():
            print(f"          {linea}")


def esperados(cps: Path):
    return [m.group(1) for m in map(PATRON_EXPECT.match, cps.read_text(encoding="utf-8").splitlines()) if m]


def expect_out(cps: Path):
    lineas = cps.read_text(encoding="utf-8").splitlines()
    out = []
    capturando = False
    for ln in lineas:
        if PATRON_OUT.match(ln):
            capturando = True
            continue
        if capturando:
            if ln.startswith("// "):
                out.append(ln[3:])
            elif ln.startswith("//"):
                out.append(ln[2:])
            else:
                break
    return out


def correr(binario, *args, timeout=30):
    proc = subprocess.run(
        [binario, *args],
        cwd=RAIZ,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
    )
    return proc.returncode, proc.stdout.replace("\r\n", "\n"), proc.stderr.replace("\r\n", "\n")


def codigos(texto: str):
    return [m.group(1) for m in map(PATRON_DIAG.match, texto.splitlines()) if m]


def mars_disponible():
    jar = os.environ.get("MARS_JAR", "").strip()
    if not jar:
        cfg = RAIZ / "tests" / "mips" / "mars.path"
        if cfg.is_file():
            jar = cfg.read_text(encoding="utf-8").strip()
    if not jar or not Path(jar).is_file():
        return None
    java = shutil.which("java") or str(RAIZ / ".deps" / "jre17" / "bin" / "java")
    if not Path(java).exists() and shutil.which("java"):
        java = shutil.which("java")
    return java, jar


def ejecutar_mars(java_jar, asm: Path, timeout=20):
    java, jar = java_jar
    # nc = no copyright; me = MIPS exceptions to console
    proc = subprocess.run(
        [java, "-jar", jar, "nc", "me", str(asm)],
        cwd=asm.parent,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
    )
    out = (proc.stdout or "") + (proc.stderr or "")
    out = out.replace("\r\n", "\n")
    if "Assembler Error" in out or "Runtime exception" in out or "Error in" in out:
        return False, out, proc.returncode
    return True, out, proc.returncode


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=str(RAIZ / "compiscript"))
    args = ap.parse_args()
    binario = str(Path(args.bin).resolve())

    fallos = 0
    oks = 0
    skips = 0
    mars = mars_disponible()
    if mars is None:
        skip("MARS no disponible (definí MARS_JAR o tests/mips/mars.path). "
             "Se valida generación .asm; la ejecución se omite.")

    with tempfile.TemporaryDirectory(prefix="compiscript-mips-") as tmp:
        tmp = Path(tmp)

        for cps in sorted((MIPS / "valid").glob("*.cps")):
            asm = tmp / (cps.stem + ".asm")
            code, out, err = correr(binario, "--mips", str(cps), "-o", str(asm))
            if code != 0 or not asm.is_file() or asm.stat().st_size == 0:
                fallo(f"{cps.relative_to(RAIZ)}: debia generar .asm", err or out)
                fallos += 1
                continue
            texto = asm.read_text(encoding="utf-8", errors="replace")
            if ".text" not in texto or "main:" not in texto:
                fallo(f"{cps.relative_to(RAIZ)}: .asm incompleto")
                fallos += 1
                continue
            ok(f"{cps.relative_to(RAIZ)} → asm")
            oks += 1

            esperado = expect_out(cps)
            if esperado:
                if mars is None:
                    skip(f"{cps.relative_to(RAIZ)}: ejecucion MARS omitida")
                    skips += 1
                else:
                    bien, mout, _ = ejecutar_mars(mars, asm)
                    if not bien:
                        fallo(f"{cps.relative_to(RAIZ)}: MARS fallo", mout)
                        fallos += 1
                    else:
                        # Comparar lineas no vacias del final de la salida
                        obtenido = [ln for ln in mout.splitlines() if ln.strip() != ""]
                        # MARS a veces imprime banners; buscar subsecuencia
                        matched = False
                        for i in range(0, max(1, len(obtenido) - len(esperado) + 1)):
                            if obtenido[i : i + len(esperado)] == esperado:
                                matched = True
                                break
                        if not matched and obtenido[-len(esperado) :] == esperado:
                            matched = True
                        if matched:
                            ok(f"{cps.relative_to(RAIZ)} salida MARS")
                            oks += 1
                        else:
                            fallo(
                                f"{cps.relative_to(RAIZ)}: salida MARS distinta",
                                "esperado:\n"
                                + "\n".join(esperado)
                                + "\nobtenido:\n"
                                + "\n".join(obtenido[-20:]),
                            )
                            fallos += 1

        for carpeta in ("invalid", "invalid_backend"):
            d = MIPS / carpeta
            if not d.is_dir():
                continue
            for cps in sorted(d.glob("*.cps")):
                asm = tmp / (cps.stem + ".asm")
                if asm.exists():
                    asm.unlink()
                code, out, err = correr(binario, "--mips", str(cps), "-o", str(asm))
                texto = out + err
                exp = esperados(cps)
                got = codigos(texto)
                if code == 0:
                    fallo(f"{cps.relative_to(RAIZ)}: debia fallar", texto)
                    fallos += 1
                    continue
                if asm.exists():
                    fallo(f"{cps.relative_to(RAIZ)}: no debia quedar .asm")
                    fallos += 1
                    continue
                from collections import Counter

                if not exp:
                    fallo(f"{cps.relative_to(RAIZ)}: falta '// expect:'", texto)
                    fallos += 1
                    continue
                if Counter(exp) != Counter(got):
                    fallo(f"{cps.relative_to(RAIZ)}: codigos {got} != {exp}", texto)
                    fallos += 1
                    continue
                ok(f"{cps.relative_to(RAIZ)}  {got}")
                oks += 1

    print("----")
    print(f"{oks} ok, {fallos} fallos, {skips} skip")
    # Criterio doc 01: sin MARS no se finge verde de ejecucion, pero la
    # generacion debe pasar. Exit 1 solo si hay fallos reales.
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main())
