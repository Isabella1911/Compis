-- UPDATE y DELETE con WHERE sobre la base de ejemplo.
-- archivos: estudiantes.csv
UPDATE estudiantes SET edad = edad + 1 WHERE edad < 18;
DELETE FROM estudiantes WHERE edad IS NULL;
SELECT id, nombre, edad FROM estudiantes WHERE edad <= 18 ORDER BY id;
