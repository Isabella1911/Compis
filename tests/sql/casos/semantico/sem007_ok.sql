-- Aritmetica entre numeros y logica entre booleanos.
-- espera: OK
CREATE TABLE alumnos (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, promedio FLOAT, activo BOOLEAN, ingreso DATE);
SELECT -edad * 2 + promedio / 3 FROM alumnos WHERE NOT activo OR edad % 2 = 0;
