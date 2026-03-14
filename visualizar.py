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
    """Renderiza un .dot a imagen usando pydot."""
    import pydot
    out_path = dot_path.replace(".dot", f".{formato}")
    try:
        graphs = pydot.graph_from_dot_file(dot_path, encoding="utf-8")
        if not graphs:
            return None, "No se pudo parsear el archivo .dot"
        graph = graphs[0]
        if formato == "png":
            graph.write_png(out_path)
        elif formato == "pdf":
            graph.write_pdf(out_path)
        elif formato == "svg":
            graph.write_svg(out_path)
        else:
            graph.write_png(out_path)
 
        if os.path.exists(out_path):
            size = os.path.getsize(out_path)
            return out_path, size
        else:
            return None, "Archivo de salida no generado"
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
        if os.path.isdir("output"):
            directorio = "output"
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