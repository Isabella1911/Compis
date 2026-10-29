-- Calificar con el alias o con el nombre de la tabla sin alias.
-- espera: OK
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
SELECT a.nombre, a.* FROM alumnos a;
SELECT alumnos.nombre FROM alumnos;
