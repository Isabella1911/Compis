# IDE, batería de pruebas y entrega final

Estado de cumplimiento del documento de entrega (IDE + tests + checklist)
respecto a este repositorio.

## 1. IDE — cubierto

| Requisito | Estado |
|-----------|--------|
| Editor `.cps` (abrir / editar / guardar) | Sí — `ide/ide_app.py` |
| Compilar e invocar `compiscript` | Sí — Compilar (`make`) + Ejecutar análisis |
| Diagnósticos (línea, columna, código, mensaje) | Sí — tabla + pestaña Errores |
| Resaltar línea con error | Sí — fondo en editor + gutter |
| AST texto (`printTree`) y DOT/PNG | Sí |
| Tabla de símbolos (`printScopeTree`) | Sí |

Detalle de uso: [ide/IDE.md](../ide/IDE.md).

Decisión de equipo: Python + subproceso (mismo patrón que la IDE de la
rama `Deprecated`), no UI en C++.

## 2. Batería de pruebas — cubierto

- Fixtures en `tests/fixtures/valid/` y `tests/fixtures/invalid/`.
- Nombres por regla (`sem004_tipo_aritmetica.cps`, …).
- `tests/fixtures/expected.json` exige el **conjunto exacto** de códigos.
- `make test` corre unitarios + runner + fixtures (timeout 5 s, sin stderr
  inesperado, posiciones válidas).

Verificado en este checkout: `140 ok, 0 fallos` con `./compiscript`.

## 3. Documentación y checklist

| Ítem | Estado |
|------|--------|
| `README.md` arquitectura / build / decisiones | Actualizado |
| `tools/setup.sh` reproducible sin sudo | Presente |
| IDE funcional | Presente |
| Batería corriendo al presentar | Presente (`make test`) |
| Repo GitHub (`Isabella1911/Compis`, `main`) | En uso |
| Commits individuales por integrante | Proceso de equipo (no automatizable aquí) |

Los pases semánticos y el sistema de tipos se documentan en
[03_passes_semanticos.md](03_passes_semanticos.md). El informe histórico
[ANALISIS_ESTADO_PROYECTO.md](ANALISIS_ESTADO_PROYECTO.md) describe un
estado anterior; no sustituye al README actual.
