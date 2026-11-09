#!/usr/bin/env python3
"""Prueba del clasificador de tests/mips/mars.py contra MARS real.

Los programas de aqui estan escritos a mano A PROPOSITO: no prueban el
compilador sino al runner. Cada uno provoca una forma distinta de terminar
y el runner tiene que reconocerla, en especial las que MARS reporta con
codigo de salida 0 aunque el programa no haya terminado bien (limite de
pasos, caerse al final del codigo). Si MARS rechaza un .asm, el resultado
tiene que traer el error concreto (N16).

Sin MARS las pruebas se omiten (skip) con el motivo; con
COMPISCRIPT_ESTRICTO=1 la falta de MARS es un fallo.

Uso: python3 tests/mips/test_mars_runner.py
"""

from __future__ import annotations

import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import mars  # noqa: E402

CFG, MOTIVO = mars.detectar_mars()

PROGRAMAS = {
    "exit_normal": ".text\nmain: li $v0, 1\n li $a0, 42\n syscall\n li $v0, 10\n syscall\n",
    "exit_con_codigo": ".text\nmain: li $v0, 1\n li $a0, 7\n syscall\n li $a0, 1\n li $v0, 17\n syscall\n",
    "error_ensamblado": ".text\nmain: li $v0, 1\n addx $t0, $t1\n syscall\n",
    "excepcion": ".text\nmain: li $t0, 3\n lw $t1, 0($t0)\n li $v0, 10\n syscall\n",
    "ciclo_infinito": ".text\nmain: j main\n",
    "sin_exit": ".text\nmain: li $v0, 1\n li $a0, 5\n syscall\n",
}


def correr(nombre, **kw):
    with tempfile.TemporaryDirectory(prefix="mars-runner-") as tmp:
        asm = Path(tmp) / f"{nombre}.asm"
        asm.write_text(PROGRAMAS[nombre], encoding="utf-8")
        return mars.ejecutar(CFG, asm, **kw)


@unittest.skipIf(CFG is None and os.environ.get("COMPISCRIPT_ESTRICTO") != "1",
                 f"MARS no disponible: {MOTIVO}")
class ClasificadorMars(unittest.TestCase):
    def setUp(self):
        if CFG is None:
            self.fail(f"modo estricto sin MARS: {MOTIVO}")

    def test_exit_normal(self):
        r = correr("exit_normal")
        self.assertEqual(r.terminacion, "syscall")
        self.assertEqual(r.codigo, 0)
        self.assertEqual(r.stdout, "42")
        self.assertEqual(r.errores, [])

    def test_exit_con_codigo(self):
        r = correr("exit_con_codigo")
        self.assertEqual(r.terminacion, "syscall")
        self.assertEqual(r.codigo, 1)
        self.assertEqual(r.stdout, "7")

    def test_error_de_ensamblado_trae_el_error(self):
        r = correr("error_ensamblado")
        self.assertEqual(r.terminacion, "error_ensamblado")
        self.assertFalse(r.termino_bien)
        self.assertTrue(any("addx" in e for e in r.errores), r.errores)

    def test_excepcion_de_runtime(self):
        r = correr("excepcion")
        self.assertEqual(r.terminacion, "excepcion")
        self.assertTrue(any("Runtime exception" in e for e in r.errores), r.errores)

    def test_ciclo_infinito_no_es_exito(self):
        # MARS sale con 0 al agotar los pasos: el codigo solo no alcanza.
        r = correr("ciclo_infinito", max_pasos=10000)
        self.assertEqual(r.codigo, 0)
        self.assertEqual(r.terminacion, "limite_pasos")
        self.assertFalse(r.termino_bien)

    def test_caerse_del_codigo_no_es_exito(self):
        r = correr("sin_exit")
        self.assertEqual(r.codigo, 0)
        self.assertEqual(r.stdout, "5")
        self.assertEqual(r.terminacion, "sin_exit")
        self.assertFalse(r.termino_bien)

    def test_timeout(self):
        # El limite de pasos de MARS es un int: 2.000 millones no se alcanza
        # en 3 s, asi que lo que corta es el timeout del proceso.
        r = correr("ciclo_infinito", max_pasos=2_000_000_000, timeout_s=3)
        self.assertEqual(r.terminacion, "timeout")
        self.assertFalse(r.termino_bien)


class DeteccionDelJar(unittest.TestCase):
    def test_jar_corrupto_se_rechaza(self):
        with tempfile.TemporaryDirectory(prefix="mars-jar-") as tmp:
            falso = Path(tmp) / "Mars.jar"
            falso.write_text("no soy un jar", encoding="utf-8")
            viejo = os.environ.get("MARS_JAR")
            os.environ["MARS_JAR"] = str(falso)
            try:
                cfg, motivo = mars.detectar_mars()
            finally:
                if viejo is None:
                    del os.environ["MARS_JAR"]
                else:
                    os.environ["MARS_JAR"] = viejo
        self.assertIsNone(cfg)
        self.assertIn("no es un .jar valido", motivo)


if __name__ == "__main__":
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
    import resultados_unittest  # noqa: E402
    sys.exit(resultados_unittest.correr("mars_runner", sys.modules[__name__]))
