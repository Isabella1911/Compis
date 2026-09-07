#!/usr/bin/env python3
"""Ejecuta fixtures con timeout y exige el conjunto exacto de codigos de error."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile

DIAGNOSTIC = re.compile(r'^\s*\[(\w+)\] (error|warning) (\d+):(\d+):', re.MULTILINE)


def validate_result(result, expected):
    required_exit = 1 if expected else 0
    if result.returncode != required_exit:
        return f'salida {result.returncode}; se esperaba {required_exit} (crashes no son rechazos validos)'
    if result.stderr.strip():
        return f'stderr inesperado: {result.stderr.strip()}'
    diagnostics = DIAGNOSTIC.findall(result.stdout)
    actual = {code for code, severity, _, _ in diagnostics if severity == 'error'}
    if actual != set(expected):
        return f'codigos esperados {sorted(expected)}, recibidos {sorted(actual)}'
    if any(int(line) < 1 or int(column) < 1 for _, _, line, column in diagnostics):
        return 'diagnostico sin ubicacion valida'
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('--timeout', type=float, default=5)
    args = parser.parse_args()
    binary = args.binary.resolve()
    root = Path(__file__).resolve().parent / 'fixtures'
    expected = json.loads((root / 'expected.json').read_text(encoding='utf-8'))
    invalid = sorted((root / 'invalid').glob('*.cps'))
    if set(expected) != {p.name for p in invalid} or any(not codes for codes in expected.values()):
        parser.error('expected.json debe contener exactamente todos los fixtures invalidos y sus codigos')
    fixtures = sorted((root / 'valid').glob('*.cps')) + invalid
    failures = 0
    with tempfile.TemporaryDirectory(prefix='compis-fixtures-') as workdir:
        for fixture in fixtures:
            codes = expected[fixture.name] if fixture.parent.name == 'invalid' else []
            try:
                result = subprocess.run([str(binary), str(fixture)], cwd=workdir,
                                        capture_output=True, text=True, timeout=args.timeout)
                error = validate_result(result, codes)
                detail = result.stdout if error else ''
            except subprocess.TimeoutExpired:
                error, detail = f'timeout de {args.timeout}s', ''
            except OSError as exc:
                error, detail = str(exc), ''
            if error:
                failures += 1
                print(f'FALLO {fixture.name}: {error}\n{detail}')
            else:
                print(f'OK    {fixture.name}')
    print(f'{len(fixtures) - failures} ok, {failures} fallos')
    return int(failures != 0)


if __name__ == '__main__':
    raise SystemExit(main())
