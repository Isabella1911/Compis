-- Suma entre numero y texto.
-- espera: SEM007 @ 4:13
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
SELECT edad + nombre FROM alumnos;
