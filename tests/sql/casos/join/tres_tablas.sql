-- base: vacia
CREATE TABLE a (id INT, nombre VARCHAR(10));
CREATE TABLE b (aid INT, cid INT);
CREATE TABLE c (id INT, valor INT);
INSERT INTO a VALUES (1, 'Ana'), (2, 'Luis');
INSERT INTO b VALUES (1, 10), (1, 20), (2, 20);
INSERT INTO c VALUES (10, 90), (20, 80);
SELECT a.*, c.valor FROM a JOIN b ON a.id=b.aid JOIN c ON b.cid=c.id WHERE c.valor >= 80 ORDER BY a.id, c.valor;
SELECT * FROM a JOIN b ON a.id=b.aid WHERE a.id=2;
SELECT a.* FROM a JOIN b ON FALSE;
