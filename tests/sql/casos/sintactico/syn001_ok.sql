-- Ejemplos del enunciado y la gramatica completa (incluida la de la entrega 2).
-- espera: OK
-- modo: ast
SELECT nombre
FROM estudiantes
WHERE edad >= 18;

SELECT nombre
FROM estudiantes
WHERE id IN (SELECT estudiante_id
             FROM inscripciones
             WHERE curso_id = 3);

SELECT e.nombre, COUNT(*) AS total
FROM estudiantes e INNER JOIN inscripciones i ON i.estudiante_id = e.id
WHERE NOT EXISTS (SELECT 1 FROM cursos c WHERE c.id = i.curso_id)
GROUP BY e.nombre
HAVING COUNT(*) > (SELECT AVG(nota) FROM inscripciones)
ORDER BY total DESC;

CREATE INDEX idx_edad ON estudiantes (edad);
DROP INDEX idx_edad;
