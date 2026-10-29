-- La tabla existe, aunque se escriba con otras mayusculas.
-- espera: OK
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
SELECT nombre FROM ALUMNOS;
DROP TABLE Alumnos;
