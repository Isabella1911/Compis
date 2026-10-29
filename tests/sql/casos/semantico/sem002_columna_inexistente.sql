-- Columna que no esta en la tabla.
-- espera: SEM002 @ 4:16
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
SELECT nombre, nota FROM alumnos;
