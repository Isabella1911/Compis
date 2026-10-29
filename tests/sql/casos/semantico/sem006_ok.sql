-- INT con FLOAT, DATE con una cadena que es fecha, y NULL con todo.
-- espera: OK
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
SELECT nombre FROM alumnos WHERE edad > promedio AND ingreso >= '2024-01-01' AND activo = NULL;
