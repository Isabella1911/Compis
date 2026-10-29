#!/usr/bin/env python3
"""Verifica los casos de la Parte 1 (lexico, sintactico y semantico).

Cada caso es un .sql en tests/sql/casos/<tema>/ con una cabecera:

    -- <descripcion>
    -- espera: OK
    -- espera: SEM002 @ 3:8
    -- espera: LEX002, SYN001
    -- modo: ast            (opcional: corre --emit ast en vez del analisis)

"espera" es la lista exacta de diagnosticos que debe producir el script, en
orden de posicion. Cada uno es un codigo y, opcionalmente, "@ linea:columna".
OK significa "sin ningun diagnostico".

Corre sqlint con --json --no-exec, asi que no necesita ninguna base de datos
ni la ejecucion de las Partes 2 y 3. El runner general de la bateria
(tests/sql/run_tests.py, Parte 2) compara la salida completa; este solo
revisa codigos y posiciones.

Uso:
    python3 tests/sql/verificar_parte1.py [--bin ./sqlint] [--mostrar] [filtro]
"""

import argparse
import json
import pathlib
import re
import subprocess
import sys

CASOS = pathlib.Path(__file__).resolve().parent / "casos"
ESPERA = re.compile(r"^--\s*espera:\s*(.+)$", re.MULTILINE)
MODO = re.compile(r"^--\s*modo:\s*(\S+)", re.MULTILINE)
ITEM = re.compile(r"^([A-Z]{3}\d{3})(?:\s*@\s*(\d+):(\d+))?$")


def leer_esperado(texto, ruta):
    m = ESPERA.search(texto)
    if not m:
        raise ValueError(f"{ruta}: falta la linea '-- espera:'")
    valor = m.group(1).strip()
    if valor.upper() == "OK":
        return []
    esperados = []
    for parte in valor.split(","):
        item = ITEM.match(parte.strip())
        if not item:
            raise ValueError(f"{ruta}: no entiendo '{parte.strip()}' en '-- espera:'")
        codigo, linea, col = item.groups()
        esperados.append((codigo, int(linea) if linea else None, int(col) if col else None))
    return esperados


def correr(binario, ruta, modo):
    args = [binario, "--json", "--no-exec"]
    if modo == "ast":
        args += ["--emit", "ast"]
    proc = subprocess.run(args + [str(ruta)], capture_output=True, text=True, encoding="utf-8")
    try:
        salida = json.loads(proc.stdout)
    except json.JSONDecodeError:
        raise RuntimeError(f"salida que no es JSON (exit {proc.returncode}):\n{proc.stdout}{proc.stderr}")
    return salida["diagnostics"]


def coincide(esperados, obtenidos):
    if len(esperados) != len(obtenidos):
        return False
    for (codigo, linea, col), d in zip(esperados, obtenidos):
        if d["code"] != codigo:
            return False
        if linea is not None and (d["line"], d["col"]) != (linea, col):
            return False
    return True


def formato(d):
    return f"{d['code']} @ {d['line']}:{d['col']}  {d['message']}"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bin", default="./sqlint")
    parser.add_argument("--mostrar", action="store_true", help="imprime los diagnosticos de todos los casos")
    parser.add_argument("filtro", nargs="?", default="", help="solo casos cuya ruta contenga este texto")
    args = parser.parse_args()

    casos = sorted(p for p in CASOS.rglob("*.sql") if args.filtro in str(p))
    fallos = 0
    por_tema = {}
    for ruta in casos:
        texto = ruta.read_text(encoding="utf-8")
        tema = ruta.parent.name
        rel = ruta.relative_to(CASOS)
        try:
            esperados = leer_esperado(texto, rel)
            modo = MODO.search(texto)
            obtenidos = correr(args.bin, ruta, modo.group(1) if modo else "")
            ok = coincide(esperados, obtenidos)
        except (ValueError, RuntimeError) as e:
            ok, obtenidos = False, []
            print(f"ERROR {rel}: {e}")
        total, bien = por_tema.get(tema, (0, 0))
        por_tema[tema] = (total + 1, bien + (1 if ok else 0))
        if not ok:
            fallos += 1
            esperado_txt = ", ".join(
                c + (f" @ {l}:{k}" if l is not None else "") for c, l, k in esperados
            ) or "OK"
            print(f"FALLA {rel}\n  esperado: {esperado_txt}")
            for d in obtenidos:
                print(f"  obtenido: {formato(d)}")
            if not obtenidos:
                print("  obtenido: OK")
        elif args.mostrar:
            print(f"ok    {rel}")
            for d in obtenidos:
                print(f"        {formato(d)}")

    print()
    for tema, (total, bien) in sorted(por_tema.items()):
        print(f"{tema}: {bien}/{total} ok")
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main())
