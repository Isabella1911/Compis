-- Llave primaria repetida (la 1 ya existe): RUN101 y no se inserta NINGUNA
-- fila de la sentencia. El CSV queda igual que en databases/escuela.
-- archivos: estudiantes.csv
INSERT INTO estudiantes (id, nombre) VALUES (100, 'Nueva'), (1, 'Repetida');
