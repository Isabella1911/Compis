-- Errores semánticos: el script se ejecuta sentencia por sentencia y se
-- detiene en el primer error. Probá cada sentencia con "Ejecutar
-- selección" (Ctrl+Enter) para ver cada código.
SELECT nombre FROM estudiantes WHERE edad > 20;
SELECT edad2 FROM estudiantes;
SELECT nombre FROM alumnos;
SELECT nombre FROM estudiantes e JOIN cursos c ON e.id = c.id;
SELECT nombre FROM estudiantes WHERE nombre > 5;
INSERT INTO estudiantes VALUES (99, 'Muy largo', 20);
INSERT INTO estudiantes VALUES (99, NULL, 20, '2025-02-30', TRUE);
SELECT nombre, COUNT(*) FROM estudiantes;
