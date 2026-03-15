#!/usr/bin/env python3
"""
visualizar.py — Genera imágenes PNG de todos los archivos .dot
producidos por el generador de analizadores léxicos (YalexParser).
 
Uso:
    python3 visualizar.py [directorio] [--formato png|pdf|svg]
 
Si no se especifica directorio, busca en ./output/
 
Requisitos:
    pip install pydot pillow
"""
 
import os
import sys
import glob
 
def check_deps():
    """Verifica que pydot esté instalado."""
    try:
        import pydot
        return True
    except ImportError:
        print("Error: pydot no esta instalado.")
        print("Instalar con: pip install pydot")
        print("")
        print("pydot incluye su propio renderizador, NO necesitas")
        print("instalar Graphviz por separado.")
        return False
 
def render_dot(dot_path, formato="png"):
    import graphviz
    with open(dot_path, "r", encoding="utf-8", errors="replace") as f:
        source = f.read()
    out_base = dot_path.replace(".dot", "")
    try:
        src = graphviz.Source(source)
        src.render(out_base, format=formato, cleanup=True)
        out_path = f"{out_base}.{formato}"
        if os.path.exists(out_path):
            return out_path, os.path.getsize(out_path)
        return None, "Archivo no generado"
    except Exception as e:
        return None, str(e)
 
def categorizar_dots(archivos):
    """Agrupa los archivos .dot por categoría."""
    cats = {
        "ASTs individuales": [],
        "AST combinado": [],
        "AFN": [],
        "AFD": [],
        "AFD minimizado": [],
    }
    for f in sorted(archivos):
        nombre = os.path.basename(f)
        if nombre.startswith("ast_regla_"):
            cats["ASTs individuales"].append(f)
        elif nombre.startswith("ast_combinado"):
            cats["AST combinado"].append(f)
        elif nombre.startswith("afn_"):
            cats["AFN"].append(f)
        elif nombre.startswith("afd_min"):
            cats["AFD minimizado"].append(f)
        elif nombre.startswith("afd_"):
            cats["AFD"].append(f)
        else:
            cats.setdefault("Otros", []).append(f)
    return cats
 
def main():
    # Parsear argumentos
    directorio = None
    formato = "png"
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--formato" and i + 1 < len(args):
            formato = args[i + 1]
            i += 2
        elif not args[i].startswith("--"):
            directorio = args[i]
            i += 1
        else:
            i += 1
 
    # Buscar directorio por defecto
    if directorio is None:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        if os.path.isdir(os.path.join(script_dir, "output")):
            directorio = os.path.join(script_dir, "output")
        else:
            directorio = "."
 
    if not check_deps():
        return 1
 
    # Buscar .dot
    patron = os.path.join(directorio, "*.dot")
    archivos = glob.glob(patron)
 
    if not archivos:
        print(f"No se encontraron archivos .dot en '{directorio}/'")
        print(f"Primero ejecuta: .\\yalex_parser archivo.yal")
        return 1
 
    print(f"=== Visualizador de Automatas y Arboles ===")
    print(f"")
    print(f"Directorio: {os.path.abspath(directorio)}")
    print(f"Archivos .dot: {len(archivos)}")
    print(f"Formato: {formato}")
    print(f"")
 
    cats = categorizar_dots(archivos)
    ok = 0
    fail = 0
 
    for cat, files in cats.items():
        if not files:
            continue
        print(f"--- {cat} ({len(files)}) ---")
        for f in files:
            nombre = os.path.basename(f)
            out, info = render_dot(f, formato)
            if out:
                kb = info / 1024
                print(f"  OK  {nombre} -> {os.path.basename(out)} ({kb:.1f} KB)")
                ok += 1
            else:
                print(f"  ERR {nombre} -- {info}")
                fail += 1
        print()
 
    print(f"Listo: {ok} generados, {fail} errores")
    if ok > 0:
        print(f"Imagenes en: {os.path.abspath(directorio)}/")
    return 0 if fail == 0 else 1
 
if __name__ == "__main__":
    sys.exit(main())