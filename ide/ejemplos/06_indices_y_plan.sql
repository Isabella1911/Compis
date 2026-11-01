-- Índices: se crean, se usan en el WHERE (ver la pestaña Plan: IndexScan)
-- y se mantienen al modificar la tabla. Al final se eliminan para que el
-- ejemplo se pueda repetir.
CREATE INDEX idx_edad ON estudiantes (edad);

SELECT nombre, edad FROM estudiantes WHERE edad >= 21 ORDER BY edad;
SELECT nombre FROM estudiantes WHERE edad = 20;

-- Sin índice sobre nombre: recorrido completo (SeqScan).
SELECT edad FROM estudiantes WHERE nombre = 'Ana García';

-- El índice se actualiza con el UPDATE: la consulta siguiente lo usa
-- y encuentra las filas con la edad nueva.
UPDATE estudiantes SET edad = edad + 100 WHERE edad = 17;
SELECT nombre, edad FROM estudiantes WHERE edad >= 100;
UPDATE estudiantes SET edad = edad - 100 WHERE edad >= 100;

DROP INDEX idx_edad;
