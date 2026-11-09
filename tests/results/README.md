# Resultados de ejecuciones reales

Los archivos `.json` los escriben los runners en cada corrida. La versión
versionada es la de la última `make validar-entrega` (modo estricto: MARS
real y display para la IDE). Ninguno se edita a mano.

| Archivo | Lo escribe | Contenido |
|---|---|---|
| `mips_integracion.json` | `tests/mips/run_mips_tests.py` | CLI: cada `.cps` válido genera `.asm`; cada inválido da sus códigos y ningún `.asm`. |
| `mips_e2e.json` | `tests/mips/run_mips_tests.py` | Cada programa válido ejecutado en MARS: salida esperada y obtenida, código de salida, forma de terminar. |
| `mips_unitarias.json` | `tests/mips/unit/run_unit.py` | Verificaciones C++ del backend y sus programas TAC ejecutados en MARS. |
| `mars_runner.json` | `tests/mips/test_mars_runner.py` | El clasificador de MARS ante cada forma de terminar (incluido un `.asm` que MARS rechaza). |
| `cli.json` | `tests/cli/test_cli.py` | Casos límite de la CLI. |
| `ide.json` | `tests/ide/test_ide.py` | La IDE manejada con Tkinter real. |
| `rubrica.json` | `tests/validar_rubrica.py` | Cada caso del plan (E01–E18, N01–N16, A–E) y cada apartado del enunciado con `PASS`/`FAIL`/`BLOCKED` y su evidencia. Sin puntaje: la nota la pone el docente. |

Los `.txt` (`baseline_*.txt`, `after_mips_make_test.txt`, `mips_tests.txt`)
son el registro histórico de la auditoría inicial del backend, de antes de
estas pruebas (cuando MARS no estaba instalado y varias construcciones daban
`CG004`). Se conservan como evidencia de ese momento; el estado actual es el
de los `.json`.
