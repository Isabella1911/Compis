-- base: vacia
CREATE TABLE t (id INT);
INSERT INTO t VALUES (1);
SELECT 8 % 3 AS entero, 8.5 % 2.0 AS flotante FROM t;
SELECT id % 0 FROM t;
