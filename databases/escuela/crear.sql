-- Base de ejemplo "escuela" (docs/sql/parte2, seccion 5.5).
--
-- Construye desde cero lo que hay en databases/escuela:
--   ./sqlint --db /tmp/escuela databases/escuela/crear.sql
--
-- Pensada para las pruebas: estudiantes menores y mayores de 18, nombres con
-- coma y con comillas, algunos NULL, estudiantes sin inscripciones (6, 14,
-- 18, 22 y 24, para NOT EXISTS) y cursos con varias inscripciones (para
-- GROUP BY). Usa todos los tipos: INT, FLOAT, VARCHAR, BOOLEAN y DATE.
-- make test-mutator verifica que este script produce exactamente los
-- archivos de databases/escuela.
--
-- Sin indices: CREATE INDEX es de la entrega 2 (hoy el semantico responde
-- SEM023). Cuando se habilite, agregar al final:
--   CREATE INDEX idx_estudiantes_edad ON estudiantes (edad);

CREATE TABLE estudiantes (
    id INT PRIMARY KEY,
    nombre VARCHAR(50) NOT NULL,
    edad INT,
    fecha_ingreso DATE,
    becado BOOLEAN
);

CREATE TABLE cursos (
    id INT PRIMARY KEY,
    nombre VARCHAR(60) NOT NULL,
    creditos INT NOT NULL,
    activo BOOLEAN NOT NULL,
    costo FLOAT
);

CREATE TABLE inscripciones (
    estudiante_id INT NOT NULL,
    curso_id INT NOT NULL,
    nota FLOAT,
    fecha DATE
);

INSERT INTO estudiantes (id, nombre, edad, fecha_ingreso, becado) VALUES
    (1, 'Ana García', 20, DATE '2024-01-15', TRUE),
    (2, 'Pérez, Luis', 17, DATE '2025-07-01', FALSE),
    (3, 'María José Ruiz', 19, DATE '2024-01-15', TRUE),
    (4, 'Carlos O''Neil', 22, DATE '2022-07-04', FALSE),
    (5, 'Sofía "Sofi" Méndez', 18, DATE '2025-01-13', FALSE),
    (6, 'Diego Hernández', 16, DATE '2026-01-12', NULL),
    (7, 'Valeria López', 21, DATE '2023-01-16', TRUE),
    (8, 'José Martínez', NULL, DATE '2024-07-08', FALSE),
    (9, 'Lucía Castillo', 17, DATE '2025-07-01', FALSE),
    (10, 'Andrés Morales', 23, DATE '2021-07-05', TRUE),
    (11, 'Fernanda Rodas', 18, NULL, FALSE),
    (12, 'Pablo Quiñónez', 20, DATE '2024-01-15', FALSE),
    (13, 'Gabriela Ortiz', 19, DATE '2024-07-08', TRUE),
    (14, 'Ricardo Paz', 25, DATE '2020-01-13', FALSE),
    (15, 'Camila Reyes', 17, DATE '2026-01-12', FALSE),
    (16, 'Mateo Aguilar', 18, DATE '2025-01-13', NULL),
    (17, 'Isabel Fuentes', 22, DATE '2023-07-03', TRUE),
    (18, 'Javier Solís', 16, DATE '2026-01-12', FALSE),
    (19, 'Daniela Cruz', 20, DATE '2024-02-29', FALSE),
    (20, 'Tomás Barrios', 21, DATE '2023-01-16', FALSE),
    (21, 'Elena Vásquez', 19, DATE '2024-07-08', TRUE),
    (22, 'Hugo Estrada', 24, DATE '2021-01-11', FALSE),
    (23, 'Natalia Gómez', 18, DATE '2025-07-01', FALSE),
    (24, 'Sebastián Toledo', NULL, NULL, NULL),
    (25, 'Renata Lima', 20, DATE '2024-01-15', TRUE);

INSERT INTO cursos (id, nombre, creditos, activo, costo) VALUES
    (1, 'Bases de Datos', 4, TRUE, 850.0),
    (2, 'Compiladores', 5, TRUE, 950.5),
    (3, 'Cálculo I', 4, TRUE, 700.0),
    (4, 'Física, Mecánica', 4, TRUE, 725.25),
    (5, 'Programación Orientada a Objetos', 3, TRUE, 800.0),
    (6, 'Estadística', 3, TRUE, 650.0),
    (7, 'Redes', 4, FALSE, 900.0),
    (8, 'Ética Profesional', 2, TRUE, NULL),
    (9, 'Inteligencia Artificial', 5, TRUE, 1100.0),
    (10, 'Sistemas Operativos', 4, TRUE, 875.0),
    (11, 'Álgebra Lineal', 4, TRUE, 700.0),
    (12, 'Historia del Arte', 2, FALSE, 500.0),
    (13, 'Diseño Web', 3, TRUE, 780.0),
    (14, 'Seguridad Informática', 4, TRUE, 990.0),
    (15, 'Inglés Técnico', 2, TRUE, 450.0),
    (16, 'Ecuaciones Diferenciales', 4, TRUE, 720.0),
    (17, 'Teoría de la Computación', 4, TRUE, 860.0),
    (18, 'Gestión de Proyectos', 3, FALSE, NULL),
    (19, 'Computación Gráfica', 4, TRUE, 920.0),
    (20, 'Taller de Investigación', 2, TRUE, 300.0);

INSERT INTO inscripciones (estudiante_id, curso_id, nota, fecha) VALUES
    (1, 1, 85.5, DATE '2024-01-20'),
    (1, 2, 92.0, DATE '2024-01-20'),
    (1, 3, 78.25, DATE '2024-01-20'),
    (2, 1, 60.0, DATE '2025-07-05'),
    (2, 5, NULL, DATE '2025-07-05'),
    (3, 1, 95.0, DATE '2024-01-20'),
    (3, 2, 88.0, DATE '2024-01-20'),
    (3, 9, 91.5, DATE '2024-07-10'),
    (4, 2, 70.0, DATE '2022-07-10'),
    (4, 7, 65.5, DATE '2022-07-10'),
    (5, 1, 99.0, DATE '2025-01-15'),
    (5, 6, 82.0, DATE '2025-01-15'),
    (7, 2, 77.0, DATE '2023-01-20'),
    (7, 10, 84.0, DATE '2023-01-20'),
    (7, 11, 90.0, DATE '2023-07-10'),
    (8, 1, NULL, DATE '2024-07-10'),
    (8, 3, 55.0, DATE '2024-07-10'),
    (9, 5, 73.5, DATE '2025-07-05'),
    (10, 9, 96.0, DATE '2021-07-10'),
    (10, 14, 89.0, DATE '2022-01-15'),
    (10, 2, 81.0, DATE '2021-07-10'),
    (11, 4, 68.0, DATE '2025-01-15'),
    (12, 1, 74.0, DATE '2024-01-20'),
    (12, 13, 87.0, DATE '2024-01-20'),
    (13, 2, 93.5, DATE '2024-07-10'),
    (13, 17, 88.5, DATE '2024-07-10'),
    (15, 15, 80.0, DATE '2026-01-15'),
    (16, 6, 71.0, DATE '2025-01-15'),
    (16, 1, 66.0, DATE '2025-01-15'),
    (17, 9, 94.0, DATE '2023-07-10'),
    (17, 19, 85.0, DATE '2023-07-10'),
    (19, 3, 79.0, DATE '2024-03-01'),
    (19, 16, 72.5, DATE '2024-03-01'),
    (20, 10, 58.0, DATE '2023-01-20'),
    (21, 1, 89.5, DATE '2024-07-10'),
    (21, 2, 90.0, DATE '2024-07-10'),
    (23, 5, 76.0, DATE '2025-07-05'),
    (25, 1, 83.0, DATE '2024-01-20'),
    (25, 20, 100.0, DATE '2024-01-20');
