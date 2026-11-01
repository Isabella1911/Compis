-- Despues de INSERT, UPDATE y DELETE, el SELECT por indice (con plan
-- IndexScan) da lo mismo que el SELECT despues de borrar el indice
-- (SeqScan). Es la prueba mas importante de los indices. El indice es el
-- que trae databases/escuela, asi que ademas prueba que un indice cargado
-- desde disco se mantiene igual que uno recien creado.
-- archivos: indices/idx_estudiantes_edad.idx
INSERT INTO estudiantes (id, nombre, edad) VALUES (100, 'Nueva', 18), (101, 'Otra', 30);
UPDATE estudiantes SET edad = edad + 1 WHERE edad < 18;
DELETE FROM estudiantes WHERE edad = 30 OR id = 1;
SELECT id, edad FROM estudiantes WHERE edad >= 18 ORDER BY id;
DROP INDEX idx_estudiantes_edad;
SELECT id, edad FROM estudiantes WHERE edad >= 18 ORDER BY id;
