-- 2024 si es bisiesto; tambien se acepta DATE '...'.
-- espera: OK
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
INSERT INTO alumnos (id, nombre, ingreso) VALUES (1, 'Ana', '2024-02-29'), (2, 'Luis', DATE '2025-12-31');
