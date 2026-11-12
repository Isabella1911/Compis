-- DROP / DELETE con CASCADE sobre asistencias → estudiantes.
-- Usá "usar copia" y "Reiniciar copia" antes de la demo.
--
-- 1) Ver la tabla nueva y su relación
SELECT * FROM asistencias ORDER BY id;

SELECT a.id, e.nombre, a.fecha, a.presente
FROM asistencias a
JOIN estudiantes e ON e.id = a.estudiante_id
WHERE a.estudiante_id = 1
ORDER BY a.fecha;

-- 2) Sin CASCADE: no se puede borrar la tabla padre
-- DROP TABLE estudiantes;

-- 3) DELETE CASCADE: borra al estudiante 1 y sus filas en asistencias
--    e inscripciones (FK ON DELETE CASCADE)
DELETE FROM estudiantes WHERE id = 1;

SELECT * FROM asistencias WHERE estudiante_id = 1;
SELECT * FROM inscripciones WHERE estudiante_id = 1;
SELECT nombre FROM estudiantes WHERE id = 1;

-- 4) DROP TABLE CASCADE: elimina la tabla padre, limpia hijas y borra
--    estudiantes.csv (en la copia de la IDE)
DROP TABLE estudiantes CASCADE;

SELECT * FROM asistencias ORDER BY id;
SELECT * FROM cursos;
