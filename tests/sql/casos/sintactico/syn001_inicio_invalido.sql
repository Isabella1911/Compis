-- Una sentencia que EMPIEZA con un token invalido no tapa los errores de las siguientes.
-- espera: SYN001 @ 3:1, SYN001 @ 4:45, SYN001 @ 5:8
SELEC nombre FROM estudiantes;
SELECT nombre FROM estudiantes WHERE edad > ;
DELETE estudiantes;
SELECT 'a;b' FROM estudiantes;
