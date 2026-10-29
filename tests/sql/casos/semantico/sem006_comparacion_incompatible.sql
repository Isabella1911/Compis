-- INT comparado con VARCHAR.
-- espera: SEM006 @ 4:39
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
SELECT nombre FROM alumnos WHERE edad = 'veinte';
