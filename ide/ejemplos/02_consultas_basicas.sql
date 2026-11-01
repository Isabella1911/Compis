-- SELECT sobre una tabla: proyección, alias, expresiones, WHERE y ORDER BY.
SELECT * FROM cursos;

SELECT nombre, edad, edad + 1 AS edad_proximo_anio
FROM estudiantes
WHERE edad >= 18 AND becado = TRUE
ORDER BY edad DESC, nombre ASC;

SELECT nombre, fecha_ingreso
FROM estudiantes
WHERE fecha_ingreso >= '2025-01-01' OR edad IS NULL
ORDER BY fecha_ingreso;

SELECT nombre, costo, costo / creditos AS costo_por_credito
FROM cursos
WHERE activo = TRUE AND costo IS NOT NULL
ORDER BY costo_por_credito DESC;
