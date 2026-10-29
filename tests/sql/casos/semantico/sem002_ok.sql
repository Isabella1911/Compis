-- Columnas existentes, sin distinguir mayusculas.
-- espera: OK
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
SELECT Nombre, EDAD FROM alumnos WHERE id = 1;
