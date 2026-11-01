-- base: vacia
-- espera: SEM005
CREATE TABLE t (id INT, s VARCHAR(10));
SELECT a.id FROM t a JOIN t a ON a.id=a.id;
