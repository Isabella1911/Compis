-- base: vacia
CREATE TABLE t (id INT);
INSERT INTO t VALUES (1);
SELECT 8 / 2 AS correcto, 8.0 / 2 AS flotante, NULL / 0 AS nulo FROM t;
SELECT id / 0 FROM t;
INSERT INTO t VALUES (2);
