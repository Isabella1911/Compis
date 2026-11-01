-- Los SET se evaluan con la fila original: SET a = b, b = a intercambia.
-- base: vacia
-- archivos: w.csv
CREATE TABLE w (a INT, b INT);
INSERT INTO w VALUES (1, 2), (3, 4), (NULL, 5);
UPDATE w SET a = b, b = a WHERE a < 3;
SELECT * FROM w;
