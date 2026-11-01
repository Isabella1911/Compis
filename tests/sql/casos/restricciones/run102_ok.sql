-- NOT NULL con un valor calculado que no es NULL.
-- base: vacia
-- archivos: p.csv
CREATE TABLE p (a INT NOT NULL, b INT);
INSERT INTO p VALUES (1, NULL), (2, 5);
UPDATE p SET a = b + 1 WHERE b IS NOT NULL;
