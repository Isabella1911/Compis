-- 'abcdef' no cabe en VARCHAR(3): RUN104 y el archivo no cambia.
-- base: vacia
-- archivos: s.csv
CREATE TABLE s (corto VARCHAR(3), largo VARCHAR(20));
INSERT INTO s VALUES ('ab', 'abc'), ('xy', 'abcdef');
UPDATE s SET corto = largo;
