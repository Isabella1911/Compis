-- El INSERT no le da valor a una columna NOT NULL (la PRIMARY KEY tambien es NOT NULL).
-- espera: SEM012 @ 4:13
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
INSERT INTO alumnos (nombre) VALUES ('Ana');
