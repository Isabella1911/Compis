-- Texto en una columna INT.
-- espera: SEM009 @ 4:58
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
INSERT INTO alumnos (id, nombre, edad) VALUES (1, 'Ana', 'veinte');
