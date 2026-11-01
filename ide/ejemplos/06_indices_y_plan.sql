-- Índices. La base de ejemplo ya trae idx_estudiantes_edad sobre
-- estudiantes(edad): las consultas por edad lo usan (pestaña Plan:
-- IndexScan) y el índice se mantiene al modificar la tabla. Al final se
-- crea y se elimina otro índice, así el ejemplo se puede repetir.
SELECT nombre, edad FROM estudiantes WHERE edad >= 21 ORDER BY edad;
SELECT nombre FROM estudiantes WHERE edad = 20;

-- Sin índice sobre nombre: recorrido completo (SeqScan).
SELECT edad FROM estudiantes WHERE nombre = 'Ana García';

-- El índice se actualiza con el UPDATE: la consulta siguiente lo usa
-- y encuentra las filas con la edad nueva.
UPDATE estudiantes SET edad = edad + 100 WHERE edad = 17;
SELECT nombre, edad FROM estudiantes WHERE edad >= 100;
UPDATE estudiantes SET edad = edad - 100 WHERE edad >= 100;

-- CREATE INDEX sobre otra columna: la misma consulta pasa de SeqScan a
-- IndexScan.
SELECT nombre FROM cursos WHERE costo > 900;
CREATE INDEX idx_cursos_costo ON cursos (costo);
SELECT nombre FROM cursos WHERE costo > 900;
DROP INDEX idx_cursos_costo;
