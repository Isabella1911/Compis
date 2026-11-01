-- base: vacia
CREATE TABLE e (id INT, nombre VARCHAR(10));
CREATE TABLE i (eid INT, nota INT);
INSERT INTO e VALUES (1, 'Ana'), (2, 'Luis'), (3, 'José');
INSERT INTO i VALUES (1, 90), (1, 80), (3, 70);
CREATE INDEX idx_eid ON i (eid);
SELECT nombre FROM e WHERE NOT EXISTS (SELECT 1 FROM i WHERE i.eid=e.id) ORDER BY id;
SELECT nombre FROM e WHERE EXISTS (SELECT 1 FROM i WHERE i.eid=e.id AND i.nota >= 80) ORDER BY id;
SELECT nombre FROM e WHERE EXISTS (SELECT 1 FROM i a WHERE EXISTS (SELECT 1 FROM i b WHERE b.eid=e.id AND b.eid=a.eid)) ORDER BY id;
SELECT e.nombre, i.nota FROM e JOIN i ON i.eid=e.id WHERE EXISTS (SELECT 1 FROM e x WHERE x.id=i.eid) ORDER BY e.id, i.nota;
SELECT id FROM e WHERE id IN (SELECT eid FROM i WHERE i.eid=e.id) ORDER BY id;
