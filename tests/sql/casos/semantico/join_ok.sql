-- JOIN ya se analiza y ejecuta.
-- espera: OK
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
SELECT a.nombre FROM alumnos a JOIN alumnos b ON a.id = b.id;
