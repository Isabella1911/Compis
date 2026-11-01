-- Subconsultas: IN, NOT IN, EXISTS, NOT EXISTS (correlacionada) y escalar.
SELECT nombre FROM estudiantes
WHERE id IN (SELECT estudiante_id FROM inscripciones WHERE nota >= 95);

SELECT nombre FROM estudiantes
WHERE id NOT IN (SELECT estudiante_id FROM inscripciones);

SELECT e.nombre FROM estudiantes e
WHERE NOT EXISTS (SELECT * FROM inscripciones i WHERE i.estudiante_id = e.id);

SELECT nombre, edad FROM estudiantes
WHERE edad = (SELECT MAX(edad) FROM estudiantes);
