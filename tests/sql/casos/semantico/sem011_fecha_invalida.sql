-- 2026 no es bisiesto.
-- espera: SEM011 @ 4:61
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
INSERT INTO alumnos (id, nombre, ingreso) VALUES (1, 'Ana', '2026-02-29');
