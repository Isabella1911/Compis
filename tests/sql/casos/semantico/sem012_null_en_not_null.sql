-- NULL explicito en una columna NOT NULL.
-- espera: SEM012 @ 4:45
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
INSERT INTO alumnos (id, nombre) VALUES (1, NULL);
