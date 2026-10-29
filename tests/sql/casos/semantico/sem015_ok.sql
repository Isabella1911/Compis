-- Cada columna una sola vez.
-- espera: OK
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
UPDATE alumnos SET edad = 1, promedio = 2.5 WHERE id = 1;
