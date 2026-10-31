-- base: vacia
CREATE TABLE t (g INT, n INT, f FLOAT, s VARCHAR(10));
INSERT INTO t VALUES (1, 10, 1.5, 'b'), (1, 20, 2.5, 'a'), (2, NULL, NULL, NULL), (NULL, 5, 3.0, 'z'), (NULL, NULL, NULL, NULL);
SELECT COUNT(*), COUNT(n), SUM(n), AVG(n), MIN(n), MAX(n), SUM(f), MIN(s), MAX(s) FROM t;
SELECT g, COUNT(*), COUNT(n), SUM(n), AVG(n), MIN(n), MAX(n) FROM t GROUP BY g ORDER BY g;
SELECT g, SUM(n) / COUNT(n) AS promedio FROM t GROUP BY g HAVING COUNT(n) > 0 ORDER BY COUNT(*) DESC, g;
SELECT g FROM t GROUP BY g HAVING SUM(n) >= 10 ORDER BY SUM(n) DESC;
SELECT g + 1 AS clave, COUNT(*) FROM t GROUP BY g + 1 ORDER BY clave;
