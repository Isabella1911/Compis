#!/usr/bin/env python3
"""
visualizar.py — Genera imágenes de archivos .dot producidos por YalexParser.
 
Uso:
    python3 visualizar.py [directorio] [--formato png|pdf|svg] [--incluir-afn]
 
Si no se especifica directorio, busca en ./output/
 
Requisitos:
    pip install graphviz
"""
 
import os
import sys
import glob
import shutil
 
def check_deps():
    """Verifica dependencias de Python y ejecutable dot."""
    try:
        import graphviz  # noqa: F401
    except ImportError:
        print("Error: graphviz (paquete de Python) no esta instalado.")
        print("Instalar con: pip install graphviz")
        return False
    if shutil.which("dot") is None:
        print("Error: no se encontró 'dot' en PATH.")
        print("Instala Graphviz y agrega su carpeta bin al PATH.")
        return False
    return True
 
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
 
def categorizar_dots(archivos, incluir_afn=False):
    """Agrupa los archivos .dot por categoría."""
    cats = {
        "ASTs individuales": [],
        "AST combinado": [],
        "AFD": [],
        "AFD minimizado": [],
    }
    afn_omitidos = []
    for f in sorted(archivos):
        nombre = os.path.basename(f)
        if nombre.startswith("ast_regla_"):
            cats["ASTs individuales"].append(f)
        elif nombre.startswith("ast_combinado"):
            cats["AST combinado"].append(f)
        elif nombre.startswith("afn_"):
            if incluir_afn:
                cats.setdefault("AFN", []).append(f)
            else:
                afn_omitidos.append(f)
        elif nombre.startswith("afd_min"):
            cats["AFD minimizado"].append(f)
        elif nombre.startswith("afd_"):
            cats["AFD"].append(f)
        else:
            cats.setdefault("Otros", []).append(f)
    return cats, afn_omitidos
 
def main():
    # Parsear argumentos
    directorio = None
    formato = "png"
    incluir_afn = False
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--formato" and i + 1 < len(args):
            formato = args[i + 1]
            i += 2
        elif args[i] == "--incluir-afn":
            incluir_afn = True
            i += 1
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
 
    cats, afn_omitidos = categorizar_dots(archivos, incluir_afn=incluir_afn)
    if afn_omitidos:
        print(f"Omitiendo AFN ({len(afn_omitidos)}) por defecto. Usa --incluir-afn para renderizarlos.")
        print("")
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
