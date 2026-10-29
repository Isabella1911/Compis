-- Las columnas que aceptan NULL se pueden omitir.
-- espera: OK
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
INSERT INTO alumnos (id, nombre) VALUES (1, 'Ana');
UPDATE alumnos SET edad = NULL;
