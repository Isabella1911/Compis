-- base: vacia
CREATE TABLE t (id INT, nombre VARCHAR(20), edad INT, f FLOAT, b BOOLEAN, d DATE);
INSERT INTO t VALUES (1, 'Ana', 18, 3.5, TRUE, '2026-01-01'), (2, 'Luis', NULL, NULL, FALSE, NULL), (3, 'José', 17, 4.0, NULL, '2026-02-28');
SELECT nombre FROM t WHERE edad >= 18;
SELECT *, edad + 1 AS siguiente FROM t ORDER BY id;
SELECT t.*, 7 / 2 AS entera, 7 / 2.0 AS real, 7 % 2 AS modulo FROM t WHERE id = 1;
SELECT nombre AS persona FROM t WHERE d >= '2026-01-01' ORDER BY edad DESC;
SELECT nombre, NULL, 'comilla '' y \"' AS texto FROM t WHERE id=1;
