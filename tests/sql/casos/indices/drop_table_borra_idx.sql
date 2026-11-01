-- DROP TABLE borra tambien los archivos de sus indices: los dos .idx salen
-- como null.
-- archivos: metadata.txt, indices/idx_edad.idx, indices/idx_nombre.idx
CREATE INDEX idx_edad ON estudiantes (edad);
CREATE INDEX idx_nombre ON estudiantes (nombre);
DROP TABLE inscripciones;
DROP TABLE estudiantes;
