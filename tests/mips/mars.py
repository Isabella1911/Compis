"""Ejecucion de programas MIPS en MARS 4.5 desde las pruebas (y la IDE).

Opciones de linea de comandos de MARS 4.5 que se usan, verificadas con la
version instalada (programas de prueba para cada caso):

    nc        sin el aviso de copyright
    me        los mensajes del simulador van a stderr: stdout queda solo
              con lo que imprime el programa
    ae98      un error de ensamblado termina MARS con codigo 98
    se99      una excepcion de runtime (direccion invalida, etc.) con 99
    <n>       limite de instrucciones simuladas; al alcanzarlo MARS sale
              con codigo 0 y escribe en stderr "maximum step limit ... reached"
    v0        al terminar escribe el valor de $v0 en stderr

Clasificacion de una ejecucion (`ResultadoMars.terminacion`):

    "syscall"          termino con syscall 10 (exit) o 17 (exit2): $v0 final
                       es 10 o 17; el codigo de salida es el del programa
    "error_ensamblado" codigo 98
    "excepcion"        codigo 99
    "limite_pasos"     se agoto el limite de instrucciones (ciclo infinito)
    "timeout"          el proceso supero el limite de tiempo
    "sin_exit"         llego al final del codigo sin hacer exit
    "no_ejecutado"     no se pudo lanzar Java

Solo "syscall" es una terminacion valida: un codigo de salida 0 no basta
(MARS tambien sale con 0 al agotar los pasos o al caerse del codigo).

Sin dependencias fuera de la biblioteca estandar.
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import tempfile
import time
import zipfile
from dataclasses import dataclass, field
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent.parent

CODIGO_ENSAMBLADO = 98
CODIGO_EXCEPCION = 99
MAX_PASOS_DEFECTO = int(os.environ.get("MARS_MAX_PASOS", "5000000"))
TIMEOUT_DEFECTO = float(os.environ.get("MARS_TIMEOUT", "30"))

_PATRON_V0 = re.compile(r"^\$v0\s+0x([0-9a-fA-F]{8})\s*$", re.MULTILINE)
_PATRON_LIMITE = re.compile(r"maximum step limit \d+ reached")


@dataclass
class ConfigMars:
    java: str
    jar: str
    version: str


@dataclass
class ResultadoMars:
    terminacion: str
    codigo: int | None
    stdout: str
    stderr: str
    comando: list[str]
    duracion_s: float
    errores: list[str] = field(default_factory=list)

    @property
    def termino_bien(self) -> bool:
        return self.terminacion == "syscall"


def _candidatos_jar() -> list[tuple[str, str]]:
    """(ruta, de donde salio), en orden de preferencia. Son las ubicaciones
    documentadas en el README; no se inventa ninguna otra."""
    out = []
    env = os.environ.get("MARS_JAR", "").strip()
    if env:
        out.append((env, "MARS_JAR"))
    cfg = RAIZ / "tests" / "mips" / "mars.path"
    if cfg.is_file():
        texto = cfg.read_text(encoding="utf-8").strip()
        if texto:
            out.append((texto, "tests/mips/mars.path"))
    out.append((str(RAIZ / ".deps" / "Mars4_5.jar"), ".deps/Mars4_5.jar"))
    return out


def _buscar_java() -> str | None:
    java = shutil.which("java")
    if java:
        return java
    local = RAIZ / ".deps" / "jre17" / "bin" / "java"
    return str(local) if local.exists() else None


def _jar_valido(jar: str) -> str | None:
    """None si el .jar es un MARS legible; si no, el motivo."""
    try:
        with zipfile.ZipFile(jar) as z:
            nombres = set(z.namelist())
            if "Mars.class" not in nombres or "mars/MarsLaunch.class" not in nombres:
                return f"{jar} no parece MARS (le falta Mars.class / mars/MarsLaunch.class)"
            malo = z.testzip()
            if malo is not None:
                return f"{jar} esta corrupto (entrada danada: {malo})"
    except (OSError, zipfile.BadZipFile) as exc:
        return f"{jar} no es un .jar valido: {exc}"
    return None


_PROGRAMA_PRUEBA = ".text\nmain: li $v0, 1\n      li $a0, 42\n      syscall\n      li $v0, 10\n      syscall\n"


def detectar_mars() -> tuple[ConfigMars | None, str]:
    """Busca Java y MARS y los prueba ensamblando y ejecutando un programa
    minimo. Devuelve (config, "") o (None, motivo accionable)."""
    java = _buscar_java()
    if java is None:
        return None, ("No se encontro Java: instalalo o corre `bash tools/setup.sh`, "
                      "que deja uno en .deps/jre17.")
    revisados = []
    for jar, origen in _candidatos_jar():
        if not Path(jar).is_file():
            revisados.append(f"{origen}: {jar} no existe")
            continue
        motivo = _jar_valido(jar)
        if motivo:
            return None, f"{origen}: {motivo}"
        cfg = ConfigMars(java=java, jar=jar, version="?")
        with tempfile.TemporaryDirectory(prefix="mars-prueba-") as tmp:
            asm = Path(tmp) / "prueba.asm"
            asm.write_text(_PROGRAMA_PRUEBA, encoding="utf-8")
            r = ejecutar(cfg, asm, timeout_s=60)
        if not r.termino_bien or r.stdout != "42" or r.codigo != 0:
            return None, (f"{origen}: MARS no ejecuto bien el programa de prueba "
                          f"(terminacion={r.terminacion}, codigo={r.codigo}, "
                          f"stdout={r.stdout!r}, stderr={r.stderr.strip()[:200]!r})")
        cfg.version = _version(cfg)
        return cfg, ""
    return None, ("No se encontro MARS. Bajalo de "
                  "https://github.com/dpetersanderson/MARS/releases (Mars4_5.jar) y "
                  "ponelo en .deps/Mars4_5.jar, o defini MARS_JAR, o escribi su ruta en "
                  "tests/mips/mars.path. Revisado: " + "; ".join(revisados))


def _version(cfg: ConfigMars) -> str:
    try:
        p = subprocess.run([cfg.java, "-jar", cfg.jar, "h"], capture_output=True,
                           text=True, errors="replace", timeout=60,
                           stdin=subprocess.DEVNULL)
    except (OSError, subprocess.TimeoutExpired):
        return "?"
    for linea in (p.stdout + p.stderr).splitlines():
        if linea.startswith("MARS "):
            return linea.split("  ")[0].strip()
    return "?"


def ejecutar(cfg: ConfigMars, asm: Path, timeout_s: float = TIMEOUT_DEFECTO,
             max_pasos: int = MAX_PASOS_DEFECTO) -> ResultadoMars:
    """Ensambla y ejecuta `asm` en MARS y clasifica como termino."""
    comando = [cfg.java, "-jar", cfg.jar, "nc", "me", f"ae{CODIGO_ENSAMBLADO}",
               f"se{CODIGO_EXCEPCION}", str(max_pasos), "v0", str(asm)]
    inicio = time.monotonic()
    try:
        p = subprocess.run(comando, cwd=str(Path(asm).parent), capture_output=True,
                           text=True, encoding="utf-8", errors="replace",
                           stdin=subprocess.DEVNULL, timeout=timeout_s)
    except subprocess.TimeoutExpired as exc:
        salida = exc.stdout or ""
        if isinstance(salida, bytes):
            salida = salida.decode("utf-8", errors="replace")
        return ResultadoMars("timeout", None, salida.replace("\r\n", "\n"), "", comando,
                             time.monotonic() - inicio,
                             [f"MARS no termino en {timeout_s:g} s"])
    except OSError as exc:
        return ResultadoMars("no_ejecutado", None, "", str(exc), comando,
                             time.monotonic() - inicio, [f"No se pudo lanzar Java: {exc}"])
    duracion = time.monotonic() - inicio
    stdout = p.stdout.replace("\r\n", "\n")
    stderr = p.stderr.replace("\r\n", "\n")
    # Lo que MARS agrega a stderr y se puede reconocer sin ambiguedad: la
    # linea de $v0 que pedimos. El resto de stderr se conserva tal cual.
    m = _PATRON_V0.search(stderr)
    v0 = int(m.group(1), 16) if m else None
    mensajes = _PATRON_V0.sub("", stderr).strip()

    if p.returncode == CODIGO_ENSAMBLADO:
        terminacion = "error_ensamblado"
    elif p.returncode == CODIGO_EXCEPCION:
        terminacion = "excepcion"
    elif _PATRON_LIMITE.search(stderr):
        terminacion = "limite_pasos"
    elif v0 in (10, 17):
        terminacion = "syscall"
    else:
        terminacion = "sin_exit"
    errores = []
    if terminacion != "syscall":
        errores.append(mensajes or f"terminacion: {terminacion}")
    return ResultadoMars(terminacion, p.returncode, stdout, mensajes, comando, duracion,
                         errores)
