-- INNER JOIN de dos y tres tablas, con alias.
SELECT e.nombre, i.nota
FROM estudiantes e
INNER JOIN inscripciones i ON e.id = i.estudiante_id
WHERE i.nota >= 90
ORDER BY i.nota DESC;

SELECT e.nombre AS estudiante, c.nombre AS curso, i.nota
FROM estudiantes e
JOIN inscripciones i ON e.id = i.estudiante_id
JOIN cursos c ON c.id = i.curso_id
WHERE c.creditos >= 4
ORDER BY c.nombre, e.nombre;
