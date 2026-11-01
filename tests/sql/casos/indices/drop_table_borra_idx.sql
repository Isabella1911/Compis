-- DROP TABLE borra tambien los archivos de sus indices: el que trae
-- databases/escuela y uno creado aqui. Los dos .idx salen como null.
-- archivos: metadata.txt, indices/idx_estudiantes_edad.idx, indices/idx_nombre.idx
CREATE INDEX idx_nombre ON estudiantes (nombre);
DROP TABLE inscripciones;
DROP TABLE estudiantes;
