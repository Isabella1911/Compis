-- La fila tiene menos valores que columnas.
-- espera: SEM008 @ 4:28
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
INSERT INTO alumnos VALUES (1, 'Ana', 20);
