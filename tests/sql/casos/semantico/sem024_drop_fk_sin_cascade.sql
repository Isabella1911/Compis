-- DROP TABLE sin CASCADE falla si otra tabla tiene FOREIGN KEY hacia ella.
-- espera: SEM024
CREATE TABLE padres (id INT PRIMARY KEY);
CREATE TABLE hijos (
    id INT PRIMARY KEY,
    padre_id INT NOT NULL,
    FOREIGN KEY (padre_id) REFERENCES padres (id) ON DELETE CASCADE
);
DROP TABLE padres;
