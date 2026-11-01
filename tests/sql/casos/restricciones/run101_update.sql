-- UPDATE que deja dos filas con la misma llave: RUN101 y el archivo no cambia.
-- archivos: estudiantes.csv
UPDATE estudiantes SET id = 1 WHERE id = 2;
