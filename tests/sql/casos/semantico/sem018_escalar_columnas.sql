-- base: vacia
-- espera: SEM018
CREATE TABLE t (id INT, s VARCHAR(10));
SELECT (SELECT id, s FROM t) FROM t;
