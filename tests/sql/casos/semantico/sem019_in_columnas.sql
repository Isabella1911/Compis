-- base: vacia
-- espera: SEM019
CREATE TABLE t (id INT, s VARCHAR(10));
SELECT id FROM t WHERE id IN (SELECT id, s FROM t);
