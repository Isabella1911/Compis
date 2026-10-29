-- JOIN se soporta hasta la entrega 2.
-- espera: SEM023 @ 4:32
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
SELECT a.nombre FROM alumnos a JOIN alumnos b ON a.id = b.id;
