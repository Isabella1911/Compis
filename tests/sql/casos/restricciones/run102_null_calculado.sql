-- b + 1 da NULL en la primera fila y a es NOT NULL: RUN102 en ejecucion
-- (el semantico solo ve el NULL literal). No se actualiza ninguna fila.
-- base: vacia
-- archivos: p.csv
CREATE TABLE p (a INT NOT NULL, b INT);
INSERT INTO p VALUES (1, NULL), (2, 5);
UPDATE p SET a = b + 1;
