#!/usr/bin/env python3
"""Chequeo de requisitos contra evidencia de ejecuciones reales.

No corre pruebas ni calcula una nota: lee los resultados que dejaron los
runners en tests/results/*.json (de la ultima corrida) y el manifiesto
tests/mips/casos_rubrica.json, que dice que pruebas demuestran cada caso
del plan (E01-E18, N01-N16, prueba estrella A-E). Para cada caso y cada
apartado de la rubrica responde:

  PASS     todas sus pruebas pasaron en la ultima corrida
  FAIL     alguna fallo
  BLOCKED  alguna no se ejecuto (SKIP, sin resultado, o resultado anterior
           al ultimo build del compilador): no hay evidencia, no se aprueba

La nota la pone el docente: aqui no se asignan puntos. Si alguno de E09,
E10, E12, E13 o de la prueba estrella A-E no pasa, se informa que la prueba
estrella falla. Resultado en tests/results/rubrica.json.

Uso (normalmente via `make validar-entrega`):
  python3 tests/validar_rubrica.py
"""

from __future__ import annotations

import json
import sys
from datetime import datetime, timezone
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
RESULTADOS = RAIZ / "tests" / "results"
MANIFIESTO = RAIZ / "tests" / "mips" / "casos_rubrica.json"

# suite del manifiesto -> (archivo de resultados, campo que identifica el caso)
FUENTES = {
    "e2e": ("mips_e2e.json", "fuente"),
    "integracion": ("mips_integracion.json", "fuente"),
    "unitarias": ("mips_unitarias.json", "test_id"),
    "runner": ("mars_runner.json", "test_id"),
    "ide": ("ide.json", "test_id"),
    "cli": ("cli.json", "test_id"),
}

# Binarios cuyos resultados tienen que ser posteriores al ultimo build.
BINARIOS = {"e2e": "compiscript", "integracion": "compiscript", "cli": "compiscript",
            "ide": "compiscript", "unitarias": "mips_backend_test"}

# Apartados del enunciado y los casos que los evidencian. "suite:<nombre>"
# exige que TODOS los casos de esa suite hayan pasado.
RUBRICA = [
    ("Llamadas y retornos", 20, [
        "E05", "E06", "E13", "E14", "E16",
        # cada ejecucion en MARS verifica al final de main que $sp quedo
        # balanceado: toda la suite e2e en verde es esa evidencia
        "suite:e2e", "suite:unitarias"]),
    ("Asignacion de registros o pila", 20, [
        "E07", "suite:unitarias"]),
    ("Codigo MIPS funcional y bateria de pruebas", 60, [
        "E01", "E02", "E03", "E04", "E05", "E06", "E07", "E08", "E09", "E10", "E11", "E12",
        "E13", "E14", "E15", "E16", "E17", "E18",
        "N01", "N02", "N03", "N04", "N05", "N06", "N07", "N08", "N09", "N10", "N11", "N12",
        "N13", "N14", "N15", "N16",
        "A", "B", "C", "D", "E", "variantes",
        "suite:integracion", "suite:e2e", "suite:runner", "suite:cli", "suite:ide"]),
]
ESTRELLA = ["E09", "E10", "E12", "E13", "A", "B", "C", "D", "E"]


def cargar():
    datos, motivos = {}, {}
    for suite, (archivo, clave) in FUENTES.items():
        ruta = RESULTADOS / archivo
        if not ruta.is_file():
            motivos[suite] = f"no hay {archivo}: falta correr esa suite"
            continue
        contenido = json.loads(ruta.read_text(encoding="utf-8"))
        binario = BINARIOS.get(suite)
        if binario and (RAIZ / binario).is_file():
            if ruta.stat().st_mtime < (RAIZ / binario).stat().st_mtime:
                motivos[suite] = f"{archivo} es anterior al ultimo build de {binario}"
                continue
        datos[suite] = {c[clave]: c for c in contenido.get("casos", [])}
    return datos, motivos


def evaluar_prueba(ref, datos, motivos):
    suite, _, ident = ref.partition(":")
    if suite in motivos:
        return "BLOCKED", motivos[suite]
    caso = datos.get(suite, {}).get(ident)
    if caso is None:
        return "BLOCKED", f"sin resultado para {ref}"
    estado = caso.get("estado")
    if estado == "PASS":
        return "PASS", ""
    if estado == "SKIP":
        return "BLOCKED", f"{ref} no se ejecuto: {caso.get('motivo', '')}"
    return "FAIL", f"{ref}: {caso.get('motivo', '')}"


def combinar(estados):
    if any(e == "FAIL" for e, _ in estados):
        return "FAIL"
    if any(e == "BLOCKED" for e, _ in estados):
        return "BLOCKED"
    return "PASS"


def evaluar_suite(suite, datos, motivos):
    if suite in motivos:
        return "BLOCKED", motivos[suite]
    casos = datos.get(suite, {})
    if not casos:
        return "BLOCKED", f"la suite {suite} no tiene casos"
    malos = [c for c in casos.values() if c.get("estado") != "PASS"]
    if not malos:
        return "PASS", f"{len(casos)} casos"
    estado = "FAIL" if any(c.get("estado") == "FAIL" for c in malos) else "BLOCKED"
    return estado, f"{len(malos)} de {len(casos)} sin pasar"


def main():
    manifiesto = json.loads(MANIFIESTO.read_text(encoding="utf-8"))
    datos, motivos = cargar()

    casos = {}
    for ident, info in manifiesto.items():
        if ident.startswith("_"):
            continue
        resultados = [evaluar_prueba(r, datos, motivos) for r in info["pruebas"]]
        estado = combinar(resultados)
        motivo = "; ".join(dict.fromkeys(m for e, m in resultados if e != "PASS"))
        casos[ident] = {"titulo": info["titulo"], "estado": estado,
                        "pruebas": len(resultados), "motivo": motivo}

    print(f"{'Caso':<10} {'Estado':<8} Detalle")
    for ident, c in casos.items():
        detalle = c["motivo"] or f"{c['pruebas']} prueba(s) en verde"
        print(f"{ident:<10} {c['estado']:<8} {c['titulo']} — {detalle}")

    print("\n== Apartados del enunciado (sin nota: la asigna el docente)")
    apartados = []
    for nombre, puntos, requisitos in RUBRICA:
        evaluados = []
        for req in requisitos:
            if req.startswith("suite:"):
                evaluados.append((req, *evaluar_suite(req[6:], datos, motivos)))
            else:
                c = casos.get(req, {"estado": "BLOCKED", "motivo": f"{req} no esta en el manifiesto"})
                evaluados.append((req, c["estado"], c["motivo"]))
        estado = combinar([(e, m) for _, e, m in evaluados])
        pendientes = [f"{r}={e}" for r, e, _ in evaluados if e != "PASS"]
        apartados.append({"apartado": nombre, "puntos_del_enunciado": puntos, "estado": estado,
                          "requisitos": [{"requisito": r, "estado": e, "motivo": m}
                                         for r, e, m in evaluados]})
        print(f"{nombre} ({puntos} pts del enunciado): {estado}"
              + (f" — pendientes: {', '.join(pendientes)}" if pendientes else
                 f" — {len(evaluados)} requisitos con evidencia"))

    estrella = combinar([(casos[k]["estado"], "") for k in ESTRELLA if k in casos])
    print(f"\nPrueba estrella ({', '.join(ESTRELLA)}): {estrella}")
    if estrella != "PASS":
        print("  El proyecto NO puede declararse terminado mientras la prueba estrella no pase.")

    RESULTADOS.mkdir(parents=True, exist_ok=True)
    (RESULTADOS / "rubrica.json").write_text(json.dumps({
        "generado": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "nota": "sin puntaje: el validador solo informa evidencia; la nota la asigna el docente",
        "prueba_estrella": estrella, "apartados": apartados, "casos": casos,
        "bloqueos": motivos}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    todo_ok = estrella == "PASS" and all(a["estado"] == "PASS" for a in apartados)
    return 0 if todo_ok else 1


if __name__ == "__main__":
    sys.exit(main())
