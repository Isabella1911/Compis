-- Valor calculado que cabe en el VARCHAR.
-- base: vacia
-- archivos: s.csv
CREATE TABLE s (corto VARCHAR(3), largo VARCHAR(20));
INSERT INTO s VALUES ('ab', 'abc'), ('xy', 'abcdef');
UPDATE s SET corto = largo WHERE largo = 'abc';
