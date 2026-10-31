-- base: vacia
CREATE TABLE t (id INT, n INT);
INSERT INTO t VALUES (1, 10), (2, 20), (3, NULL);
SELECT id, (SELECT n FROM t x WHERE x.id=1) AS una, (SELECT n FROM t x WHERE x.id=99) AS ninguna FROM t ORDER BY id;
SELECT id, (SELECT MAX(x.n) FROM t x WHERE x.id <= t.id) AS maximo FROM t ORDER BY id;
UPDATE t SET n = n + 1 WHERE id=1;
SELECT (SELECT n FROM t x WHERE x.id=1) AS nuevo FROM t WHERE id=1;
