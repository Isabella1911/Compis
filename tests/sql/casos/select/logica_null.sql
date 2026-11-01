-- base: vacia
CREATE TABLE t (id INT, v BOOLEAN);
INSERT INTO t VALUES (1, TRUE), (2, FALSE), (3, NULL);
SELECT a.v AS a, b.v AS b, a.v AND b.v AS conjuncion, a.v OR b.v AS disyuncion FROM t a JOIN t b ON TRUE ORDER BY a.id, b.id;
SELECT NOT v AS negacion, v IS NULL AS nulo, v IS NOT NULL AS presente FROM t ORDER BY id;
SELECT id, id IN (1, NULL) AS dentro, id NOT IN (1, NULL) AS fuera, NULL + id AS suma, NULL = id AS igual FROM t ORDER BY id;
SELECT id FROM t WHERE v;
SELECT id FROM t WHERE id NOT IN (1, NULL);
