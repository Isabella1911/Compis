-- base: vacia
-- espera: SEM020
CREATE TABLE t (id INT, s VARCHAR(10));
SELECT id FROM t WHERE COUNT(*) > 1;
