-- El WHERE tiene que ser una condicion.
-- espera: SEM007 @ 4:27
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
DELETE FROM alumnos WHERE edad;
