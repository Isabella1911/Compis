# Compis — Compiscript (Proyecto 2)

Compilador de **Compiscript** (subconjunto de TypeScript) para el curso
*Construcción de Compiladores*, UVG. El código vive en [`compiscript/`](compiscript/README.md) —
ahí está toda la documentación real: arquitectura, cómo compilar y correr,
decisiones de lenguaje, y el estado actual del proyecto.

## Historial

Este repositorio empezó como `Compis`, un compilador didáctico con lexer y
parser (LL(1)/LR(0)/SLR(1)/LALR(1)) hechos a mano, del curso *Diseño de
Lenguajes*. Ese trabajo quedó preservado íntegro en la rama
[`Deprecated`](../../tree/Deprecated) — el enunciado de este proyecto exige
un analizador sintáctico generado con ANTLR, así que la rama `main` avanza
sobre esa base en vez de sobre el motor propio.
