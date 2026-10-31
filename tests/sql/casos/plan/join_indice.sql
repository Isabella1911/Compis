-- base: vacia
CREATE TABLE a (id INT);
CREATE TABLE b (aid INT);
INSERT INTO a VALUES (1), (2), (3);
INSERT INTO b VALUES (1), (2), (3);
CREATE INDEX idx_a ON a (id);
SELECT a.id FROM a JOIN b ON a.id=b.aid WHERE a.id>=2 AND b.aid<3 ORDER BY a.id;
