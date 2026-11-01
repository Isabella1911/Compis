#!/usr/bin/env python3
"""Oráculos independientes y garantías de integración de la Parte 3.

No regenera expectativas: comprueba filas calculadas a mano, códigos exactos,
planes, persistencia, ausencia de escrituras con --no-exec y acceso por índice.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CASES = Path(__file__).resolve().parent / 'casos'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--bin', default='./sqlint')
    args = parser.parse_args()
    binary = str(Path(args.bin).resolve())
    checks = 0

    def check(condition, message):
        nonlocal checks
        checks += 1
        if not condition:
            raise AssertionError(message)

    def run(sql, db, options=()):
        proc = subprocess.run([binary, '--json', '--db', str(db), *options, '-'],
                              input=sql, text=True, capture_output=True)
        check(proc.returncode in (0, 1), f'El proceso falló: {proc.returncode}: {proc.stderr}')
        data = json.loads(proc.stdout)
        check(proc.returncode == bool(data['diagnostics']), 'Código de salida y diagnósticos difieren')
        return data

    def case(name, expected_code=None):
        with tempfile.TemporaryDirectory(prefix='sqlint_exec_') as tmp:
            data = run((CASES / (name + '.sql')).read_text(), Path(tmp) / 'db')
        check([d['code'] for d in data['diagnostics']] == ([expected_code] if expected_code else []),
              f'{name}: {data["diagnostics"]}')
        return data, [s for s in data['statements'] if s['kind'] == 'SELECT']

    _, selections = case('select/proyeccion')
    check(selections[0]['rows'] == [['Ana']], 'SELECT edad >= 18')
    check(selections[1]['rows'] == [[1, 'Ana', 18, 3.5, True, '2026-01-01', 19],
                                   [2, 'Luis', None, None, False, None, None],
                                   [3, 'José', 17, 4, None, '2026-02-28', 18]], 'Tipos, * y aritmética NULL')
    check(selections[2]['rows'][0][-3:] == [3, 3.5, 1], 'División entera, promoción y módulo')
    check(selections[3]['rows'] == [['Ana'], ['José']], 'Conversión DATE')
    check(selections[4]['rows'][0][1] is None, 'NULL JSON')
    check(selections[1]['columns'][-1] == {'name': 'siguiente', 'type': 'INT'}, 'Alias y tipo')

    _, s = case('select/orden_null')
    check(s[0]['rows'] == [[3, 1], [1, 1], [4, 1], [5, 2], [2, None]], 'Orden ASC/DESC, NULL y estabilidad')
    check(s[1]['rows'] == [[2], [5], [1], [4], [3]], 'Ordenar por columnas no proyectadas')

    _, s = case('select/logica_null')
    conjunction = [True, False, None, False, False, False, None, False, None]
    disjunction = [True, True, True, True, False, None, True, None, None]
    check([r[2] for r in s[0]['rows']] == conjunction, 'Tabla de verdad AND completa')
    check([r[3] for r in s[0]['rows']] == disjunction, 'Tabla de verdad OR completa')
    check(s[1]['rows'] == [[False, False, True], [True, False, True], [None, True, False]], 'NOT e IS NULL')
    check(s[2]['rows'] == [[1, True, False, None, None], [2, None, None, None, None],
                           [3, None, None, None, None]], 'IN/NOT IN y aritmética/comparaciones NULL')
    check(s[3]['rows'] == [[1]] and s[4]['rows'] == [], 'WHERE solo acepta TRUE')

    for name in ('run202_division', 'run202_modulo'):
        data, s = case('select/' + name, 'RUN202')
        check(data['diagnostics'][0]['line'] == 5 and data['diagnostics'][0]['col'] == 11, 'Ubicación RUN202')
        check(s[-1]['status'] == 'error' and len(data['statements']) == 4, 'Parar al primer RUN202')
    check(s[0]['rows'] == [[2, .5]], 'Módulo entero y FLOAT')

    _, s = case('join/tres_tablas')
    check(s[0]['rows'] == [[1, 'Ana', 80], [1, 'Ana', 90], [2, 'Luis', 80]], 'JOIN de tres tablas y t.*')
    check(s[1]['rows'] == [[2, 'Luis', 2, 20]] and s[2]['rows'] == [], 'JOIN de dos tablas y vacío')

    _, s = case('agregados/funciones')
    check(s[0]['rows'] == [[5, 3, 35, 35 / 3, 5, 20, 7, 'a', 'z']], 'Todos los agregados')
    check(s[1]['rows'] == [[1, 2, 2, 30, 15, 10, 20], [2, 1, 0, None, None, None, None],
                           [None, 2, 1, 5, 5, 5, 5]], 'Grupos con NULL')
    check(s[2]['rows'] == [[1, 15], [None, 5]], 'Aritmética sobre agregados y ORDER BY COUNT')
    check(s[3]['rows'] == [[1]], 'HAVING con agregado fuera de SELECT')
    check(s[4]['rows'] == [[2, 2], [3, 1], [None, 2]], 'GROUP BY expresión')
    check(s[0]['columns'][2]['type'] == 'INT' and s[0]['columns'][3]['type'] == 'FLOAT', 'Tipos SUM y AVG')

    _, s = case('agregados/vacios')
    check(s[0]['rows'] == [[0, 0, None, None, None, None]], 'Grupo global vacío')
    check(s[1]['rows'] == [] and s[2]['rows'] == [[0]], 'GROUP BY vacío y HAVING')
    check(s[3]['rows'] == [[2, 0, None, None, None, None]] and s[4]['rows'] == [[None, 2]], 'Solo NULL')

    _, s = case('subconsultas/escalar')
    check(s[0]['rows'] == [[1, 10, None], [2, 10, None], [3, 10, None]], 'Escalar de 0 y 1 fila')
    check(s[1]['rows'] == [[1, 10], [2, 20], [3, 20]], 'Escalar correlacionada')
    check(s[2]['rows'] == [[11]], 'La caché no atraviesa sentencias')
    data, s = case('subconsultas/run201_varias_filas', 'RUN201')
    check(data['diagnostics'][0]['line'] == 5 and data['diagnostics'][0]['col'] == 8, 'Ubicación RUN201')
    check(len(data['statements']) == 4 and s[-1]['status'] == 'error', 'Detener script con RUN201')

    _, s = case('subconsultas/exists_proyeccion')
    check([x['rows'] for x in s] == [[[True]], [[True]], [[True]]], 'EXISTS ignora valores proyectados y ORDER BY')

    _, s = case('subconsultas/in_null')
    check(s[0]['rows'] == [[1, True, False], [2, None, None], [None, None, None]], 'IN subconsulta con NULL')
    check(s[1]['rows'] == [[1, False, True], [2, False, True], [None, False, True]], 'IN conjunto vacío')
    check(s[2]['rows'] == [], 'NOT IN con NULL no selecciona filas')

    _, s = case('subconsultas/correlacionadas')
    check(s[0]['rows'] == [['Luis']] and s[1]['rows'] == [['Ana']], 'EXISTS/NOT EXISTS correlacionado')
    check(s[2]['rows'] == [['Ana'], ['José']], 'Correlación depth=2')
    check(s[3]['rows'] == [['Ana', 80], ['Ana', 90], ['José', 70]], 'Subconsulta depende de fuente JOIN')
    check(s[4]['rows'] == [[1], [3]], 'IN correlacionado')
    check('correlacionada' in s[0]['plan'] and 'IndexScan idx_eid' in s[0]['plan'], 'Plan anidado con índice')

    _, s = case('plan/indices')
    check(s[0]['rows'] == s[1]['rows'] == s[-1]['rows'] == [[2, 20]], 'Mismo resultado con y sin índice')
    check(s[0]['plan'] == s[-1]['plan'] == 'SeqScan en t' and s[1]['plan'].startswith('IndexScan idx_n'), 'Elección de plan')
    check([x['rows'] for x in s[2:8]] == [[[3]], [[2]], [[1]], [[1], [2]], [[3]], []], 'Operadores indexables y NULL')
    check(all('SeqScan en t' == x['plan'] for x in s[8:11]), 'IS NULL, OR y columna en expresión usan scan')
    check(s[11]['rows'] == [[3]] and 'Subconsulta' in s[11]['plan'], 'Constante escalar como extremo de índice')
    _, s = case('plan/join_indice')
    check(s[0]['rows'] == [[2]] and 'IndexScan idx_a' in s[0]['plan'], 'Empujar WHERE antes de JOIN')
    data, s = case('plan/dml_indice')
    check([x['rows'] for x in s] == [[[2, 21], [3, 31]], [[3, 31]], [[4]]], 'Mantenimiento de índices tras DML')
    check(all('IndexScan idx_n' in x['plan'] for x in data['statements'] if x['kind'] in ('UPDATE', 'DELETE')), 'DML usa matchingRows')

    with tempfile.TemporaryDirectory(prefix='sqlint_invariants_') as tmp:
        db = Path(tmp) / 'db'
        sql = 'CREATE TABLE t (id INT); CREATE INDEX idx ON t(id); INSERT INTO t VALUES (1); DROP INDEX idx; SELECT * FROM t;'
        data = run(sql, db, ('--no-exec',))
        check(data['diagnostics'] == [] and data['statements'] == [] and not db.exists(), '--no-exec simula DDL sin escribir')
        data = run('CREATE TABLE t (id INT); SELECT FROM;', db)
        check(data['diagnostics'] and not db.exists(), 'Un error sintáctico evita toda ejecución')
        data = run('CREATE TABLE t (id INT); INSERT INTO t VALUES (1); SELECT ausente FROM t; DROP TABLE t;', db)
        check([d['code'] for d in data['diagnostics']] == ['SEM002'], 'Error semántico exacto')
        check((db / 't.csv').read_text() == 'id\n1\n' and len(data['statements']) == 2, 'Persistir anteriores y detener siguientes')
        run('INSERT INTO t VALUES (9); CREATE INDEX idx ON t(id);', db)
        # Mantener offsets: el índice de 1 debe evitar leer la fila corrupta de 9.
        p = db / 't.csv'
        p.write_text(p.read_text().replace('\n9\n', '\nx\n'))
        data = run('SELECT id FROM t WHERE id=1;', db)
        check(data['diagnostics'] == [] and data['statements'][0]['rows'] == [[1]], 'IndexScan lee solo offsets candidatos')
        data = run('SELECT id FROM t WHERE id+0=1;', db)
        check([d['code'] for d in data['diagnostics']] == ['RUN103'], 'SeqScan detecta la fila corrupta')

    # Paridad sistemática para ambos lados, rangos, FLOAT/INT y residuales.
    with tempfile.TemporaryDirectory(prefix='sqlint_index_parity_') as tmp:
        db = Path(tmp) / 'db'
        run('CREATE TABLE t (id INT, n INT); INSERT INTO t VALUES (1,10),(2,20),(3,30),(4,NULL);', db)
        predicates = [f'{left} {op} {right}' for op in ('=', '<', '<=', '>', '>=')
                      for left, right in (('n', '20.5'), ('20.5', 'n'), ('n', '20'), ('20', 'n'))]
        queries = ''.join('SELECT id FROM t WHERE ' + p + ' AND id<>1 ORDER BY id;' for p in predicates)
        before = run(queries, db)
        run('CREATE INDEX idx ON t(n);', db)
        after = run(queries, db)
        check(before['diagnostics'] == after['diagnostics'] == [], 'Paridad sin errores')
        check([s['rows'] for s in before['statements']] == [s['rows'] for s in after['statements']], 'Paridad en 20 predicados')
        check(all('IndexScan' in s['plan'] for s in after['statements']), 'Todos los predicados usan índice')
        proc = subprocess.run([binary, '--db', str(db), '-'], input='SELECT 3.5 AS valor FROM t WHERE id=1;',
                              text=True, capture_output=True)
        check(proc.returncode == 0 and '3.5 ' in proc.stdout and '3.500000' not in proc.stdout, 'FLOAT de consola sin ceros sobrantes')
        proc = subprocess.run([binary, '--json', '--db', str(db), '--emit', 'catalog'], text=True, capture_output=True)
        check(proc.returncode == 0 and json.loads(proc.stdout)['catalog'], '--emit catalog')
    print(f'ejecución: {checks}/{checks} verificaciones ok')


if __name__ == '__main__':
    main()
