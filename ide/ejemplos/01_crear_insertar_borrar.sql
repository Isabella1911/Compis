-- DDL y DML: crear una tabla, llenarla, modificarla y borrarla.
-- Deja la base como estaba (la tabla se elimina al final).
CREATE TABLE profesores (
    id INT PRIMARY KEY,
    nombre VARCHAR(40) NOT NULL,
    salario FLOAT,
    ingreso DATE,
    activo BOOLEAN
);

INSERT INTO profesores VALUES
    (1, 'Laura Méndez', 15000.50, '2020-02-01', TRUE),
    (2, 'Óscar Ruiz', 12000, '2023-07-15', TRUE),
    (3, 'Marta Díaz', NULL, NULL, FALSE);

INSERT INTO profesores (id, nombre) VALUES (4, 'Sin datos');

SELECT * FROM profesores ORDER BY id;

UPDATE profesores SET salario = salario * 1.10 WHERE activo = TRUE;
DELETE FROM profesores WHERE salario IS NULL;

SELECT nombre, salario FROM profesores ORDER BY salario DESC;

DROP TABLE profesores;
