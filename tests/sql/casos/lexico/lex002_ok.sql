-- Comilla escapada con '' y cadena en varias lineas.
-- espera: OK
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
INSERT INTO alumnos (id, nombre) VALUES (1, 'O''Neil');
INSERT INTO alumnos (id, nombre) VALUES (2, 'dos
lineas');
