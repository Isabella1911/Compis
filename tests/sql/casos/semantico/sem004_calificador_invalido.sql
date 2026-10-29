-- Con alias, la tabla solo se puede calificar con el alias.
-- espera: SEM004 @ 4:8
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
SELECT alumnos.nombre FROM alumnos a;
