-- DROP TABLE y CREATE TABLE de nuevo con el mismo nombre: la tabla nueva
-- arranca vacia y con su propio esquema.
-- base: vacia
-- archivos: metadata.txt, t.csv
CREATE TABLE t (a INT, b VARCHAR(5));
INSERT INTO t VALUES (1, 'x'), (2, 'y');
DROP TABLE t;
CREATE TABLE t (c BOOLEAN);
INSERT INTO t VALUES (TRUE);
SELECT * FROM t;
