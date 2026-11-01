-- base: vacia
CREATE TABLE t (id INT, a INT, b INT);
INSERT INTO t VALUES (1, 1, 2), (2, NULL, 1), (3, 1, NULL), (4, 1, 2), (5, 2, 1);
SELECT id, a AS clave FROM t ORDER BY clave ASC, b DESC;
SELECT id FROM t ORDER BY a DESC, b ASC;
