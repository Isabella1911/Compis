-- La base del caso (run103_csv_corrupto.db/) tiene un INT invalido en la
-- linea 3 del CSV: RUN103.
DELETE FROM t WHERE a = 1;
