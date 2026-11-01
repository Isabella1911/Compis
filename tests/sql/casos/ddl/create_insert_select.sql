-- CREATE -> INSERT -> SELECT sobre una base vacia. Todos los tipos, NULL,
-- cadena vacia, coma y comillas dentro de un VARCHAR.
-- base: vacia
-- archivos: metadata.txt, t.csv
CREATE TABLE t (id INT PRIMARY KEY, nombre VARCHAR(20) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
INSERT INTO t VALUES (1, 'Ana', 20, 8, TRUE, '2024-02-29'),
                     (2, 'Pérez, Luis', NULL, 7.25, FALSE, NULL),
                     (3, '', 19, NULL, NULL, DATE '2025-01-13'),
                     (4, 'dijo "hola"', 17, 0.1, TRUE, NULL);
SELECT * FROM t ORDER BY id;
