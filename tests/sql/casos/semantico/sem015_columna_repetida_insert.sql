-- La misma columna dos veces en el INSERT.
-- espera: SEM015 @ 4:34
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
INSERT INTO alumnos (id, nombre, id) VALUES (1, 'Ana', 2);
