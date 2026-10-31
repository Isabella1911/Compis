-- base: vacia
-- espera: SEM020
CREATE TABLE t (id INT, s VARCHAR(10));
SELECT SUM(COUNT(*)) FROM t;
