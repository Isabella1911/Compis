"""Corre una prueba unittest y guarda su resultado por caso en
tests/results/<nombre>.json (mismo formato que los demas runners), para que
tests/validar_rubrica.py pueda leerlo. La salida en consola es la normal de
unittest (verbosa).
"""

from __future__ import annotations

import json
import sys
import unittest
from datetime import datetime, timezone
from pathlib import Path

RESULTADOS = Path(__file__).resolve().parent / "results"


class _Registro(unittest.TextTestResult):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.casos = []

    def _agregar(self, test, estado, motivo=""):
        self.casos.append({"test_id": test.id().split(".")[-1], "estado": estado,
                           "motivo": motivo})

    def addSuccess(self, test):
        super().addSuccess(test)
        self._agregar(test, "PASS")

    def addFailure(self, test, err):
        super().addFailure(test, err)
        self._agregar(test, "FAIL", self._exc_info_to_string(err, test).splitlines()[-1])

    def addError(self, test, err):
        super().addError(test, err)
        self._agregar(test, "FAIL", self._exc_info_to_string(err, test).splitlines()[-1])

    def addSkip(self, test, reason):
        super().addSkip(test, reason)
        self._agregar(test, "SKIP", reason)


def correr(nombre: str, modulo) -> int:
    """Corre los TestCase de `modulo` y devuelve el codigo de salida."""
    suite = unittest.defaultTestLoader.loadTestsFromModule(modulo)
    runner = unittest.TextTestRunner(verbosity=2, resultclass=_Registro)
    resultado = runner.run(suite)
    casos = resultado.casos
    resumen = {"suite": nombre, "ejecutados": len(casos),
               "pass": sum(c["estado"] == "PASS" for c in casos),
               "fail": sum(c["estado"] == "FAIL" for c in casos),
               "skip": sum(c["estado"] == "SKIP" for c in casos)}
    RESULTADOS.mkdir(parents=True, exist_ok=True)
    (RESULTADOS / f"{nombre}.json").write_text(json.dumps({
        "generado": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "resumen": resumen, "casos": casos}, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8")
    return 0 if resultado.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit("Este modulo se usa desde las pruebas, no se ejecuta solo.")
