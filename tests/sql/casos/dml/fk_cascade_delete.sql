-- ON DELETE CASCADE: al borrar un estudiante se borran sus inscripciones
-- del CSV. DROP TABLE CASCADE hace lo mismo y elimina la tabla padre.
-- base: vacia

CREATE TABLE padres (
    id INT PRIMARY KEY,
    nombre VARCHAR(20) NOT NULL
);

CREATE TABLE hijos (
    id INT PRIMARY KEY,
    padre_id INT NOT NULL,
    nombre VARCHAR(20) NOT NULL,
    FOREIGN KEY (padre_id) REFERENCES padres (id) ON DELETE CASCADE
);

INSERT INTO padres VALUES (1, 'A'), (2, 'B');
INSERT INTO hijos VALUES (10, 1, 'a1'), (11, 1, 'a2'), (20, 2, 'b1');

DELETE FROM padres WHERE id = 1;
SELECT * FROM hijos ORDER BY id;

DROP TABLE padres CASCADE;
SELECT * FROM hijos ORDER BY id;
