-- base: vacia
-- espera: SEM016
CREATE TABLE t (id INT, s VARCHAR(10));
CREATE INDEX idx ON t(id); CREATE INDEX otro ON t(id);
