-- Exactamente 10 caracteres; los acentos cuentan como uno.
-- espera: OK
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
INSERT INTO alumnos (id, nombre) VALUES (1, 'Ángel Muñó');
