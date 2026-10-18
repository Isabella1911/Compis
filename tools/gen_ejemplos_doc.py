#!/usr/bin/env python3
"""Genera docs/10_ejemplos_de_traduccion.md a partir de tests/tac/.

Cada ejemplo es un caso de la bateria: el .cps y su .tac esperado, que es
la salida real de `./compiscript --tac`. Asi la documentacion no puede
quedar desactualizada respecto a las pruebas: despues de `make
test-tac-update`, correr

    python3 tools/gen_ejemplos_doc.py
"""

import re
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
TAC = RAIZ / "tests" / "tac"
SALIDA = RAIZ / "docs" / "10_ejemplos_de_traduccion.md"
SPEC = "08_lenguaje_intermedio.md"

# caso -> (titulo, seccion de docs/08 con las reglas de traduccion)
CASOS = [
    ("p1_aritmetica", "Expresiones aritméticas", "101-expresiones-aritméticas"),
    ("p1_declaraciones", "Declaraciones y asignaciones", "102-declaraciones-y-asignaciones"),
    ("p1_strings", "Strings", "103-strings"),
    ("p1_logicos", "Expresiones lógicas y comparaciones (cortocircuito)", "104-expresiones-lógicas-código-con-saltos-y-cortocircuito"),
    ("p1_if_else", "`if` / `else`", "105-if--else"),
    ("p1_bucles", "`while`, `do-while`, `for`, `break` y `continue`", "106-bucles-break-y-continue"),
    ("p1_switch", "`switch`", "107-switch"),
    ("p1_ternario", "Ternario", "108-ternario"),
    ("p1_try_catch", "`try` / `catch`", "109-try--catch"),
    ("p1_temporales", "Temporales con reciclaje", "8-temporales-asignación-y-reciclaje"),
    ("p2_recursion", "Funciones, llamadas, `return` y recursión", "111-funciones-y-return"),
    ("p2_closure", "Funciones anidadas y closures", "112-closures-funciones-anidadas"),
    ("p2_clase", "Clases, `new`, constructor, atributos, métodos y `this`", "113-clases-new-y-this"),
    ("p2_herencia", "Herencia", "114-herencia"),
    ("p2_polimorfismo", "Polimorfismo y despacho dinámico (vtables)", "117-polimorfismo-y-despacho-dinámico"),
    ("p2_lista", "Listas e índices", "115-listas-y-foreach"),
    ("p2_foreach", "`foreach`", "115-listas-y-foreach"),
    ("p2_return_try", "`return` dentro de un `try`", "116-return-dentro-de-un-try"),
    ("p3_programa_completo", "Programa completo", None),
]


def descripcion(cps_texto):
    """Comentarios '//' iniciales del .cps, como texto corrido."""
    lineas = []
    for l in cps_texto.splitlines():
        if not l.startswith("//"):
            break
        lineas.append(l[2:].strip())
    return " ".join(lineas)


def sin_comentarios_iniciales(cps_texto):
    lineas = cps_texto.splitlines()
    while lineas and lineas[0].startswith("//"):
        lineas.pop(0)
    return "\n".join(lineas).strip("\n")


def main():
    partes = [
        "# Ejemplos de traducción",
        "",
        "> **Generado** por `tools/gen_ejemplos_doc.py` a partir de `tests/tac/`."
        " No editar a mano: cambiar el caso de prueba, correr `make test-tac-update`,"
        " revisar el diff y volver a generar este archivo.",
        ">",
        f"> Cada ejemplo es un caso de la batería de pruebas: el programa `.cps` y el"
        f" TAC que produce `./compiscript --tac` (idéntico al `.tac` esperado)."
        f" Las reglas de traducción, el formato de cada instrucción y los supuestos"
        f" están en [`{SPEC}`]({SPEC}); aquí solo se muestran los resultados.",
        "",
        "## Índice",
        "",
    ]
    vistos = set()
    for i, (caso, titulo, _) in enumerate(CASOS, start=1):
        partes.append(f"{i}. [{titulo}](#{i}-{slug(titulo)})")
        vistos.add(caso)
    extra = sorted(p.stem for p in TAC.glob("*.cps") if p.stem not in vistos)
    casos = CASOS + [(c, c, None) for c in extra]
    for i, (caso, titulo, _) in enumerate(casos[len(CASOS):], start=len(CASOS) + 1):
        partes.append(f"{i}. [{titulo}](#{i}-{slug(titulo)})")
    partes.append("")

    for i, (caso, titulo, seccion) in enumerate(casos, start=1):
        cps = TAC / f"{caso}.cps"
        tac = TAC / f"{caso}.tac"
        if not cps.exists() or not tac.exists():
            raise SystemExit(f"falta {cps.name} o {tac.name}")
        texto = cps.read_text(encoding="utf-8")
        partes += [f"## {i}. {titulo}", ""]
        desc = descripcion(texto)
        if desc:
            partes += [desc, ""]
        ref = f"Reglas: [`{SPEC}`]({SPEC}#{seccion}). " if seccion else ""
        partes += [f"{ref}Caso: `tests/tac/{caso}.cps`.", ""]
        partes += ["```cps", sin_comentarios_iniciales(texto), "```", ""]
        partes += ["```", tac.read_text(encoding="utf-8").rstrip("\n"), "```", ""]

    errores = sorted((TAC / "errores").glob("*.cps"))
    if errores:
        n = len(casos) + 1
        partes += [f"## {n}. Programas con errores: no se genera TAC", "",
                   "Si el análisis semántico encuentra cualquier error, el generador no"
                   " corre: `--tac` imprime solo los diagnósticos y el compilador sale"
                   " con código 1. Estos casos están en `tests/tac/errores/`.", ""]
        for cps in errores:
            texto = cps.read_text(encoding="utf-8")
            esperados = re.findall(r"//\s*expect:\s*(\w+)", texto)
            partes += [f"**`{cps.name}`** — esperado: {', '.join(esperados)}", "",
                       "```cps", sin_comentarios_iniciales(texto), "```", ""]

    SALIDA.write_text("\n".join(partes), encoding="utf-8", newline="\n")
    print(f"Escrito {SALIDA.relative_to(RAIZ)} ({len(casos)} ejemplos)")


def slug(titulo):
    s = titulo.lower().replace("`", "")
    s = re.sub(r"[^\w\s-]", "", s)
    # Como GitHub: cada espacio es un guion (sin colapsar), asi "if / else"
    # queda "if--else".
    return s.strip().replace(" ", "-")


if __name__ == "__main__":
    main()
