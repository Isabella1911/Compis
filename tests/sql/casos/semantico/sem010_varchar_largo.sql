-- Texto mas largo que VARCHAR(10).
-- espera: SEM010 @ 4:45
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
INSERT INTO alumnos (id, nombre) VALUES (1, 'Maximiliano Perez');
