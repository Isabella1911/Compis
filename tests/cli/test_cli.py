#!/usr/bin/env python3
"""Casos limite de la CLI publica `compiscript` (--tac / --mips / -o).

Cada prueba corre en su propio directorio temporal. Las que ejecutan el
.asm usan MARS (tests/mips/mars.py) y se omiten si no esta.

Uso: python3 tests/cli/test_cli.py [--bin ./compiscript]
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(RAIZ / "tests" / "mips"))
import mars  # noqa: E402

BIN = str(RAIZ / "compiscript")
CFG, MOTIVO = mars.detectar_mars()
CLASE = RAIZ / "tests" / "mips" / "e2e" / "B_clase_ejecutada.cps"
SALIDA_CLASE = ["Ana", "120", "1", "Luis", "60", "2", "121"]


def cli(*args, timeout=120):
    p = subprocess.run([BIN, *map(str, args)], cwd=RAIZ, capture_output=True, text=True,
                       encoding="utf-8", errors="replace", timeout=timeout,
                       stdin=subprocess.DEVNULL)
    return p.returncode, p.stdout, p.stderr


class Cli(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory(prefix="compiscript-cli-")
        self.tmp = Path(self._tmp.name)

    def tearDown(self):
        self._tmp.cleanup()

    def ejecutar(self, asm):
        if CFG is None:
            self.skipTest(f"MARS no disponible: {MOTIVO}")
        r = mars.ejecutar(CFG, asm)
        self.assertTrue(r.termino_bien, r.errores)
        return r

    # --- archivos de entrada ---

    def test_fuente_inexistente_y_asm_viejo(self):
        asm = self.tmp / "viejo.asm"
        asm.write_text("# de una compilacion anterior\n", encoding="utf-8")
        codigo, _, err = cli("--mips", self.tmp / "no_existe.cps", "-o", asm)
        self.assertEqual(codigo, 1)
        self.assertIn("No existe el archivo", err)
        self.assertFalse(asm.exists(), "quedo el .asm de un intento anterior")

    def test_directorio_como_fuente(self):
        codigo, _, err = cli("--mips", self.tmp, "-o", self.tmp / "x.asm")
        self.assertEqual(codigo, 1)
        self.assertIn("no es un archivo", err)
        self.assertFalse((self.tmp / "x.asm").exists())

    def test_archivo_vacio(self):
        # Un programa vacio es valido: main solo termina.
        fuente = self.tmp / "vacio.cps"
        fuente.write_text("", encoding="utf-8")
        asm = self.tmp / "vacio.asm"
        codigo, _, err = cli("--mips", fuente, "-o", asm)
        self.assertEqual(codigo, 0, err)
        r = self.ejecutar(asm)
        self.assertEqual(r.stdout, "")
        self.assertEqual(r.codigo, 0)

    def test_rutas_con_espacios_y_tildes(self):
        carpeta = self.tmp / "carpeta con espacios" / "ñandú"
        carpeta.mkdir(parents=True)
        fuente = carpeta / "clase áé.cps"
        fuente.write_text(CLASE.read_text(encoding="utf-8"), encoding="utf-8")
        asm = carpeta / "salida ñ.asm"
        codigo, _, err = cli("--mips", fuente, "-o", asm)
        self.assertEqual(codigo, 0, err)
        r = self.ejecutar(asm)
        self.assertEqual(r.stdout.rstrip("\n").split("\n"), SALIDA_CLASE)

    # --- salida ---

    def test_directorio_de_salida_inexistente(self):
        destino = self.tmp / "no" / "existe" / "x.asm"
        codigo, _, err = cli("--mips", CLASE, "-o", destino)
        self.assertEqual(codigo, 1)
        self.assertIn("no se pudo escribir", err)
        self.assertFalse(destino.exists())

    @unittest.skipIf(hasattr(os, "geteuid") and os.geteuid() == 0, "root ignora permisos")
    @unittest.skipIf(os.name == "nt", "permisos POSIX")
    def test_directorio_sin_permiso_de_escritura(self):
        carpeta = self.tmp / "solo_lectura"
        carpeta.mkdir()
        carpeta.chmod(0o500)
        try:
            # En /mnt/c (NTFS) chmod no quita la escritura: solo se prueba
            # donde el sistema de archivos respeta el permiso.
            prueba = carpeta / "sonda"
            try:
                prueba.write_text("x")
                prueba.unlink()
                self.skipTest("este sistema de archivos no respeta chmod")
            except PermissionError:
                pass
            codigo, _, err = cli("--mips", CLASE, "-o", carpeta / "x.asm")
            self.assertEqual(codigo, 1)
            self.assertIn("no se pudo escribir", err)
        finally:
            carpeta.chmod(0o700)

    def test_mips_sin_o_escribe_en_stdout(self):
        codigo, out, err = cli("--mips", CLASE)
        self.assertEqual(codigo, 0, err)
        self.assertIn(".text", out)
        self.assertIn("main:", out)
        self.assertEqual(err, "")

    # --- opciones ---

    def test_flags_incompatibles(self):
        for args, mensaje in [(("--tac", "--mips", CLASE), "no se pueden usar juntos"),
                              (("-o", "x.asm", CLASE), "-o solo aplica con --mips"),
                              (("--nada", CLASE), "Opcion desconocida"),
                              (("--mips",), "falta el archivo"),
                              (("--mips", CLASE, "-o"), "-o requiere una ruta"),
                              ((CLASE, CLASE), "Sobra el argumento")]:
            with self.subTest(args=args):
                codigo, _, err = cli(*args)
                self.assertEqual(codigo, 2)
                self.assertIn(mensaje, err)

    def test_tac_sigue_igual(self):
        codigo, out, _ = cli("--tac", RAIZ / "tests" / "tac" / "p1_aritmetica.cps")
        self.assertEqual(codigo, 0)
        esperado = (RAIZ / "tests" / "tac" / "p1_aritmetica.tac").read_text(encoding="utf-8")
        self.assertEqual(out.strip(), esperado.strip())

    # --- tamano y repeticion ---

    def test_entrada_grande(self):
        # 400 funciones y una expresion que las llama a todas.
        n = 400
        lineas = [f"function f{i}(x: integer): integer {{\n  return x + {i};\n}}" for i in range(n)]
        suma = " + ".join(f"f{i}(1)" for i in range(n))
        lineas.append(f"print({suma});")
        fuente = self.tmp / "grande.cps"
        fuente.write_text("\n".join(lineas) + "\n", encoding="utf-8")
        asm = self.tmp / "grande.asm"
        inicio = time.monotonic()
        codigo, _, err = cli("--mips", fuente, "-o", asm, timeout=300)
        self.assertEqual(codigo, 0, err[-500:])
        self.assertLess(time.monotonic() - inicio, 120, "compilar tardo demasiado")
        r = self.ejecutar(asm)
        self.assertEqual(r.stdout.strip(), str(n + sum(range(n))))

    def test_compilar_dos_veces_da_el_mismo_asm(self):
        a, b = self.tmp / "a.asm", self.tmp / "b.asm"
        self.assertEqual(cli("--mips", CLASE, "-o", a)[0], 0)
        self.assertEqual(cli("--mips", CLASE, "-o", b)[0], 0)
        self.assertEqual(a.read_bytes(), b.read_bytes())

    def test_fallo_tras_un_exito_en_la_misma_ruta(self):
        asm = self.tmp / "programa.asm"
        self.assertEqual(cli("--mips", CLASE, "-o", asm)[0], 0)
        self.assertTrue(asm.exists())
        malo = self.tmp / "malo.cps"
        malo.write_text("let x: integer = ;\n", encoding="utf-8")
        codigo, _, err = cli("--mips", malo, "-o", asm)
        self.assertEqual(codigo, 1)
        self.assertIn("SYN001", err)
        self.assertFalse(asm.exists(), "el .asm del intento exitoso anterior sigue ahi")


if __name__ == "__main__":
    ap = argparse.ArgumentParser(add_help=False)
    ap.add_argument("--bin", default=BIN)
    args, resto = ap.parse_known_args()
    BIN = str(Path(args.bin).resolve())
    unittest.main(argv=[sys.argv[0], "-v", *resto])
