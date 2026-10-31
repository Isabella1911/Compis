-- base: vacia
-- espera: SEM003
CREATE TABLE t (id INT, s VARCHAR(10));
SELECT id FROM t a JOIN t b ON a.id=b.id;
