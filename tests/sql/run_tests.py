#!/usr/bin/env python3
"""Runner de la bateria de pruebas (docs/sql/parte2, seccion 5.6).

Recorre tests/sql/casos/**/*.sql. Para cada caso:

  1. copia la base a un directorio temporal:
       - <caso>.db/ si existe junto al .sql (una base propia del caso);
       - vacia si la cabecera dice "-- base: vacia";
       - databases/escuela en cualquier otro caso;
  2. corre  sqlint --json --db <tmp> <caso>.sql
  3. normaliza la salida: claves ordenadas y la ruta temporal cambiada por
     <db>, para que el resultado no dependa de la maquina;
  4. agrega, si la cabecera lo pide, el estado final de archivos de la base:
       -- archivos: estudiantes.csv, indices/idx_edad.idx
     (un archivo que no existe sale como null: asi se prueba que DROP TABLE
     borra el .idx). Sirve para verificar que una sentencia que falla no
     cambio nada;
  5. compara con <caso>.expected.

Un caso sin .expected no cuenta como fallo: sale como "pendiente" en el
resumen. --update escribe (o reescribe) los .expected; revisa el diff antes
de hacer commit.

Uso:
    python3 tests/sql/run_tests.py [--bin ./sqlint] [--update] [filtro]
"""

import argparse
import difflib
import json
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

RAIZ = pathlib.Path(__file__).resolve().parents[2]
CASOS = pathlib.Path(__file__).resolve().parent / "casos"
ESCUELA = RAIZ / "databases" / "escuela"
BASE = re.compile(r"^--\s*base:\s*(\S+)", re.MULTILINE)
ARCHIVOS = re.compile(r"^--\s*archivos:\s*(.+)$", re.MULTILINE)


def preparar_base(caso, texto, tmp):
    db = tmp / "db"
    propia = caso.with_suffix(".db")
    base = BASE.search(texto)
    if propia.is_dir():
        shutil.copytree(propia, db)
    elif base and base.group(1) == "vacia":
        db.mkdir()
    else:
        # Solo los archivos de la base, no crear.sql.
        shutil.copytree(ESCUELA, db, ignore=shutil.ignore_patterns("*.sql"))
    return db


def leer_archivos(texto, db):
    m = ARCHIVOS.search(texto)
    if not m:
        return None
    estado = {}
    for nombre in (n.strip() for n in m.group(1).split(",")):
        if not nombre:
            continue
        ruta = db / nombre
        estado[nombre] = ruta.read_text(encoding="utf-8") if ruta.is_file() else None
    return estado


def reemplazar_ruta(valor, ruta):
    if isinstance(valor, str):
        return valor.replace(ruta, "<db>")
    if isinstance(valor, list):
        return [reemplazar_ruta(v, ruta) for v in valor]
    if isinstance(valor, dict):
        return {k: reemplazar_ruta(v, ruta) for k, v in valor.items()}
    return valor


def correr(binario, caso):
    texto = caso.read_text(encoding="utf-8")
    with tempfile.TemporaryDirectory(prefix="sqlint_caso_") as nombre_tmp:
        tmp = pathlib.Path(nombre_tmp)
        db = preparar_base(caso, texto, tmp)
        proc = subprocess.run(
            [binario, "--json", "--db", str(db), str(caso)],
            capture_output=True, text=True, encoding="utf-8",
        )
        try:
            salida = json.loads(proc.stdout)
        except json.JSONDecodeError:
            salida = {"salida_no_json": proc.stdout, "stderr": proc.stderr}
        salida["exit"] = proc.returncode
        estado = leer_archivos(texto, db)
        if estado is not None:
            salida["archivos"] = estado
        salida = reemplazar_ruta(salida, str(db))
    return json.dumps(salida, sort_keys=True, indent=2, ensure_ascii=False) + "\n"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bin", default="./sqlint")
    parser.add_argument("--update", action="store_true", help="regenera los .expected")
    parser.add_argument("filtro", nargs="?", default="", help="solo casos cuya ruta contenga este texto")
    args = parser.parse_args()
    binario = str(pathlib.Path(args.bin).resolve())

    casos = sorted(p for p in CASOS.rglob("*.sql") if args.filtro in str(p))
    por_tema = {}
    fallos = 0
    for caso in casos:
        rel = caso.relative_to(CASOS)
        tema = rel.parts[0]
        esperado_ruta = caso.with_suffix(".expected")
        obtenido = correr(binario, caso)
        total, bien, pendientes = por_tema.get(tema, (0, 0, 0))

        if args.update:
            if not esperado_ruta.exists() or esperado_ruta.read_text(encoding="utf-8") != obtenido:
                esperado_ruta.write_text(obtenido, encoding="utf-8", newline="\n")
                print(f"escrito {esperado_ruta.relative_to(CASOS)}")
            por_tema[tema] = (total + 1, bien + 1, pendientes)
            continue

        if not esperado_ruta.exists():
            por_tema[tema] = (total + 1, bien, pendientes + 1)
            continue
        esperado = esperado_ruta.read_text(encoding="utf-8")
        if esperado == obtenido:
            por_tema[tema] = (total + 1, bien + 1, pendientes)
            continue

        fallos += 1
        por_tema[tema] = (total + 1, bien, pendientes)
        print(f"FALLA {rel}")
        diff = difflib.unified_diff(
            esperado.splitlines(keepends=True), obtenido.splitlines(keepends=True),
            fromfile=f"{rel.with_suffix('.expected')}", tofile="obtenido",
        )
        sys.stdout.writelines("  " + linea for linea in diff)
        print()

    for tema, (total, bien, pendientes) in sorted(por_tema.items()):
        extra = f" ({pendientes} sin .expected)" if pendientes else ""
        print(f"{tema}: {bien}/{total - pendientes} ok{extra}")
    if not casos:
        print("No hay casos" + (f" que contengan '{args.filtro}'" if args.filtro else ""))
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main())
