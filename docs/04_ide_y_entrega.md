# IDE, batería de pruebas y entrega final

> **Documento histórico (entrega anterior: análisis semántico).** Describe
> el plan de IDE y pruebas de esa entrega. El estado actual de la IDE y de la
> batería está en el [README](../README.md) (secciones 4 y 5) y en
> [`09_arquitectura.md`](09_arquitectura.md).


> Documento 3 de 3. Esto no depende de que el [sistema de tipos](02_sistema_de_tipos.md)
> ni los [passes semánticos](03_passes_semanticos.md) estén 100% completos
> — se puede empezar en paralelo, apuntando a lo que ya existe
> (`Compiler::compile()` ya funciona de punta a punta) e ir ampliando a
> medida que el resto del equipo agrega validaciones.

## 1. IDE — 15 puntos

Requisito del PDF: "Desarrollar un IDE que permita a los usuarios escribir
su propio código y compilarlo". Mínimo funcional:

- Editor de texto para `.cps` (cargar archivo, escribir, guardar).
- Botón "Compilar" que invoque el binario `compiscript` (o llame
  directamente a `Compiler::compile()` si el IDE se escribe en C++ — ver
  nota abajo) y muestre el resultado.
- Mostrar diagnósticos (línea, columna, código, mensaje) — idealmente
  resaltando la línea con el error.
- Mostrar el árbol sintáctico (ya hay `printTree()` en texto y `toDot()`
  para Graphviz — `dot -Tpng` genera un PNG que cualquier IDE puede
  embeber).
- Mostrar el estado de la tabla de símbolos (`printScopeTree()`, ya
  existe).

### Referencia de patrón de UI (no de código a copiar)

El repo tenía antes una IDE en Python/tkinter para el proyecto anterior
(`Compis`, LL1/LR0/SLR1/LALR1) — quedó preservada en la rama
[`Deprecated`](../../../tree/Deprecated) (`ide/ide_app.py`, `ide/IDE.md`).
El patrón general sirve como referencia de diseño aunque el compilador que
envuelve ya no exista en esta rama: tres editores (fuente, y en ese caso
dos archivos más que acá no aplican), botón compilar que invoca el
ejecutable como subproceso, y una pestaña por fase del pipeline
(tokens, FIRST/FOLLOW, tablas, resultado, errores). Para Compiscript el
equivalente sería: un editor de `.cps`, botón compilar, y pestañas para
AST / tabla de símbolos / diagnósticos.

**Decisión de equipo pendiente:** ¿la IDE se hace en Python (como la
anterior, invocando `compiscript` como subproceso y parseando su salida)
o en C++ (usando alguna librería de UI, o incluso una interfaz web servida
por el propio binario)? Python+subproceso es el camino de menor esfuerzo
dado que ya existe el patrón de referencia y el binario ya imprime todo lo
necesario por stdout.

## 2. Batería de pruebas por regla semántica — requisito de evaluación

El PDF es explícito: *"deberá escribir una batería de tests para validar
casos exitosos y fallidos en cada una de las reglas mencionadas [...] al
momento de presentar su trabajo, esta batería de tests debe estar presente
y será tomada en cuenta para validar el funcionamiento de su compilador."*

Ya existe el mecanismo (`tests/fixtures/valid/`, `tests/fixtures/invalid/`,
`make test`), pero hoy `make test` solo verifica "aceptó" / "rechazó" en
general — no verifica **qué código** de error salió. Para una batería que
demuestre cada regla por separado, conviene:

- Un archivo `.cps` por regla semántica (o un caso válido + uno inválido
  por regla), nombrado de forma que se entienda qué prueba
  (`tests/fixtures/invalid/sem004_tipo_incompatible_aritmetica.cps`, por
  ejemplo).
- Extender `make test` (o agregar un target nuevo) para que, en los casos
  inválidos, verifique que el código de diagnóstico reportado es
  **exactamente** el esperado (no solo "hubo un error cualquiera") — eso
  evita que un test pase por la razón equivocada.
- Cuantos más de los ejemplos de `Especificaciones.md` (los bloques ` ```cps ` )
  se conviertan en fixtures válidos, mejor: son casos que el profesor ya
  aprobó como Compiscript legítimo.

## 3. Documentación y checklist de entrega

Ya existe y hay que mantener actualizado a medida que se agregan passes:

- `README.md` (raíz) — arquitectura, cómo compilar/correr, decisiones de
  lenguaje. Cada decisión nueva que el equipo tome (las pendientes están
  listadas en la sección 4 de ese archivo, y algunas más se mencionan en
  los documentos 1 y 2 de esta serie) debería agregarse ahí, con la razón.
- `tools/setup.sh` — ya deja el entorno reproducible sin sudo en cualquier
  máquina del equipo.

Lo que falta para la entrega final:

- [ ] **Commits individuales por integrante.** El PDF es explícito: *"se
      validan los commits y contribuciones de cada integrante, no se
      permite compartir commits en conjunto, debe notarse claramente qué
      porción de código implementó cada integrante."* — repartir el
      trabajo de los documentos 1 y 2 de forma que cada quien comitee lo
      suyo directamente (no que una sola persona escriba todo y lo
      atribuya a los tres).
- [ ] IDE funcional (sección 1).
- [ ] Batería de pruebas completa y corriendo al momento de la
      presentación (sección 2).
- [ ] Repositorio de GitHub entregado — ya existe y está en uso
      (`github.com/Isabella1911/Compis`, rama `main`).
