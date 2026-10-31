-- base: vacia
-- espera: SEM021 @ 4:28, SEM021 @ 4:49
CREATE TABLE t (id INT, s VARCHAR(10));
SELECT COUNT(*), (SELECT x.id FROM t WHERE id=x.id) FROM t x;
