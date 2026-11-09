#!/usr/bin/env python3
"""Corre las pruebas unitarias del backend MIPS y ejecuta en MARS el
ensamblador de sus casos ejecutables.

1. Corre el binario de tests/mips/unit/test_mips_backend.cpp con
   --emitir <tmp>: hace sus verificaciones (casos de error sin .asm,
   registros reservados, jalr, offsets de mas de 16 bits...) y escribe
   <caso>.asm, <caso>.out (salida esperada) y <caso>.exit.
2. Ejecuta cada <caso>.asm en MARS (tests/mips/mars.py) y exige la salida
   exacta, el codigo de salida y una terminacion por syscall.

Sin MARS el paso 2 queda en SKIP; con COMPISCRIPT_ESTRICTO=1 es un error.
Resultados en tests/results/mips_unitarias.json.

Uso: python3 tests/mips/unit/run_unit.py --bin ./mips_backend_test
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import mars  # noqa: E402

RAIZ = Path(__file__).resolve().parent.parent.parent.parent
RESULTADOS = RAIZ / "tests" / "results"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    args = ap.parse_args()
    estricto = os.environ.get("COMPISCRIPT_ESTRICTO") == "1"

    casos = []
    with tempfile.TemporaryDirectory(prefix="mips-unit-") as tmp:
        p = subprocess.run([args.bin, "--emitir", tmp], capture_output=True, text=True,
                           encoding="utf-8", errors="replace", timeout=120)
        print(p.stdout.rstrip())
        unit_ok = p.returncode == 0
        casos.append({"test_id": "verificaciones_cpp", "etapa": "generacion",
                      "estado": "PASS" if unit_ok else "FAIL",
                      "motivo": p.stdout.strip().splitlines()[-1] if p.stdout.strip() else p.stderr})

        cfg, motivo = mars.detectar_mars()
        if cfg is None and estricto:
            print(f"BLOQUEADO: la validacion estricta necesita MARS.\n  {motivo}")
            return 2
        for asm in sorted(Path(tmp).glob("*.asm")):
            esperado = asm.with_suffix(".out").read_text(encoding="utf-8").rstrip("\n").split("\n")
            exit_esp = int(asm.with_suffix(".exit").read_text(encoding="utf-8").strip())
            r = {"test_id": asm.stem, "etapa": "ejecucion", "stdout_esperado": esperado,
                 "exit_esperado": exit_esp}
            if cfg is None:
                r.update(estado="SKIP", motivo=motivo)
            else:
                m = mars.ejecutar(cfg, asm)
                obtenido = m.stdout.rstrip("\n").split("\n") if m.stdout else []
                r.update(stdout_obtenido=obtenido, exit_obtenido=m.codigo,
                         terminacion=m.terminacion, errores=m.errores,
                         duracion_s=round(m.duracion_s, 3))
                if not m.termino_bien:
                    r.update(estado="FAIL", motivo=f"no termino bien en MARS ({m.terminacion})")
                elif obtenido != esperado:
                    r.update(estado="FAIL", motivo="la salida no es la esperada")
                elif m.codigo != exit_esp:
                    r.update(estado="FAIL", motivo=f"codigo {m.codigo}, se esperaba {exit_esp}")
                else:
                    r.update(estado="PASS", motivo=f"salida exacta, exit {m.codigo}")
            casos.append(r)
            linea = f"{r['estado']:<6}  unit/{asm.stem}: {r['motivo']}"
            print(linea)
            if r["estado"] == "FAIL":
                print(f"          esperado: {esperado}\n          obtenido: {r.get('stdout_obtenido')}"
                      f"\n          {r.get('errores')}")

    n = {k: sum(1 for c in casos if c["estado"] == k) for k in ("PASS", "FAIL", "SKIP")}
    resumen = {"suite": "unitarias", "ejecutados": len(casos), "pass": n["PASS"],
               "fail": n["FAIL"], "skip": n["SKIP"]}
    RESULTADOS.mkdir(parents=True, exist_ok=True)
    (RESULTADOS / "mips_unitarias.json").write_text(json.dumps({
        "generado": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "mars": {"version": cfg.version} if cfg else None,
        "resumen": resumen, "casos": casos}, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8")
    print(f"---- unitarias: {resumen['ejecutados']} ejecutados, {n['PASS']} PASS, "
          f"{n['FAIL']} FAIL, {n['SKIP']} SKIP -> tests/results/mips_unitarias.json")
    if estricto and n["SKIP"]:
        return 1
    return 1 if n["FAIL"] else 0


if __name__ == "__main__":
    sys.exit(main())
