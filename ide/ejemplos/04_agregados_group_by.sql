-- COUNT, SUM, AVG, MIN, MAX con GROUP BY y HAVING.
SELECT COUNT(*) AS estudiantes, AVG(edad) AS edad_promedio,
       MIN(edad) AS menor, MAX(edad) AS mayor
FROM estudiantes;

SELECT c.nombre, COUNT(*) AS inscritos, AVG(i.nota) AS promedio
FROM cursos c
JOIN inscripciones i ON c.id = i.curso_id
GROUP BY c.nombre
HAVING COUNT(*) >= 2
ORDER BY inscritos DESC;

SELECT becado, COUNT(*) AS total, SUM(edad) AS suma_edades
FROM estudiantes
GROUP BY becado;
