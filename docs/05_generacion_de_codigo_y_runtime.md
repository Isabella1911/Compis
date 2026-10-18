# Generación de código y runtime (fuera de Proyecto 2)

> **Documento histórico (escrito antes de esta entrega).** Eran notas para
> preparar la generación de código. Lo que se implementó en el Proyecto 2
> está especificado en [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md)
> y descrito en [`09_arquitectura.md`](09_arquitectura.md). Lo de MIPS,
> runtime y garbage collector sigue siendo trabajo de la fase siguiente.


> Documento 4 de esta serie de "lo que falta", pero de una fase distinta:
> nada de esto es parte de la rúbrica de `README_SEMANTIC_ANALYSIS.md`
> (Proyecto 2 termina en análisis semántico). Se deja preparado acá porque
> el equipo ya sabe que va a llegar a generación de código y runtime más
> adelante en el curso, y conviene no tomar decisiones ahora (en la Etapa 2)
> que después haya que deshacer.

## 0. Aviso sobre la nota de clase

Los apuntes de clase (`Apuntes_extensos_Clases_260818.md`, sección 17)
dicen: *"Es una clase de POO: hay que tener en cuenta el garbage collector
[...] Puede ser como se hace en Rust."* Vale la pena aclararlo antes de
que alguien busque "cómo Rust implementa su GC": **Rust no tiene garbage
collector.** Rust maneja memoria con *ownership* y *borrowing*, verificado
enteramente en tiempo de compilación (el "borrow checker") — cuando un
valor sale de scope, el compilador ya insertó el `drop` correspondiente,
sin nada corriendo en runtime. Es un enfoque real y válido, pero
técnicamente es *lo opuesto* a un garbage collector: en vez de una tarea
en tiempo de ejecución que rastrea qué sigue vivo, es un análisis en
tiempo de compilación que prueba cuándo cada valor deja de ser necesario.
Implementarlo a ese nivel es un sistema de tipos aparte (lifetimes),
bastante más grande que un GC clásico.

Dado el alcance de este proyecto, la recomendación de este documento es un
**garbage collector clásico en runtime** (mark-and-sweep), no un sistema
de lifetimes al estilo Rust. Si el equipo prefiere explorar el camino
Rust-like más adelante, es una decisión aparte y bastante más ambiciosa.

## 1. ¿Por qué Compiscript necesita alguno?

La gramática no tiene ninguna palabra reservada para liberar memoria
manualmente (no hay `delete`, `free`, ni nada parecido) — la única forma
de que la memoria se libere es automática. Y Compiscript sí genera
estructuras con tiempo de vida no trivial:

- **Objetos** (`new Clase(...)`): un objeto puede sobrevivir a la función
  que lo creó (si se retorna, o si otro objeto lo guarda en un campo).
- **Arreglos** (`ArrayLiteral`, incluidos los multidimensionales): mismo
  caso.
- **Closures**: una función anidada que captura una variable del entorno
  exterior necesita que esa variable siga viva después de que la función
  contenedora ya retornó (ver la nota de closures en
  [`docs/03_passes_semanticos.md`](03_passes_semanticos.md), sección
  "Funciones y Procedimientos") — esto es exactamente el caso que rompe
  una estrategia ingenua de "todo vive en el stack".

Sin alguna forma de recolección, esas tres cosas se acumulan sin límite o
se liberan demasiado pronto (dangling pointers). Hace falta algo.

## 2. Opciones y por qué se recomienda mark-and-sweep

| Estrategia | Ventaja | Problema para Compiscript |
|---|---|---|
| **Conteo de referencias** | Simple de razonar, libera apenas el contador llega a 0 | **No maneja ciclos.** Una clase puede tener un campo que apunte a otro objeto de una clase que, a su vez, tenga un campo que apunte de vuelta (o directamente un campo que se apunte a sí misma vía `this`) — eso es un ciclo, y con solo conteo de referencias esos objetos nunca llegan a 0 y quedan huérfanos para siempre. Necesitaría un colector de ciclos aparte encima, lo cual complica bastante la implementación "simple" que promete el conteo de referencias. |
| **Mark-and-sweep** | Maneja ciclos sin trabajo extra (si nadie lo alcanza desde una raíz, se recolecta, ciclo o no) | Pausa el programa mientras corre (aceptable para un proyecto de curso; no es un problema real hasta que se necesite tiempo real) |
| **Ownership/borrowing (estilo Rust)** | Cero costo en runtime | Es un sistema de tipos aparte (lifetimes), mucho más grande de implementar; no es "un GC", es evitar necesitar uno |

**Recomendación: mark-and-sweep clásico.** Es el que mejor equilibra
"realmente funciona con las estructuras que tiene el lenguaje" contra
"se puede implementar en el tiempo de un curso".

## 3. Idea de implementación

### 3.1. Qué hace falta en el runtime generado

- Un **heap**: una lista (o pool) de todos los bloques de memoria
  reservados dinámicamente — cada objeto (`new Clase(...)`) y cada arreglo
  reservado en runtime es una entrada ahí. Cada bloque necesita un header
  chico: tamaño, un bit/flag "marcado", y suficiente información para que
  el paso de `mark` sepa **qué campos de ese bloque son a su vez punteros
  a otros bloques del heap** (y cuáles son datos primitivos que no hace
  falta seguir).
- Reservar (`new`) siempre a través de una función del runtime
  (`gc_alloc(tamaño, descriptor_de_tipo)`), nunca `malloc` directo desde
  el código generado — así el GC siempre sabe qué hay reservado.
- Un conjunto de **raíces**: variables locales vivas en la pila de
  llamadas actual, variables globales, y — el caso importante de
  Compiscript — las variables capturadas por cada closure activa.

### 3.2. El algoritmo

1. **Mark**: partiendo de cada raíz, recorrer recursivamente (o con una
   pila explícita, para no volver a depender del stack de C++ del propio
   GC) cada puntero a otro bloque del heap, marcándolo como alcanzable.
2. **Sweep**: recorrer el heap completo; cualquier bloque que no haya
   quedado marcado se libera; los que sí, se desmarcan para el próximo
   ciclo.
3. Correr esto en un punto seguro — la opción más simple para un proyecto
   de curso es "antes de cada `new`, si el heap superó cierto umbral de
   tamaño", sin necesidad de nada más sofisticado (generational GC,
   incremental GC, etc. — todo eso es optimización, no correctitud).

### 3.3. Dónde engancha con lo que ya existe

Nada de esto toca el código de la Etapa 2 (`src/ast/`, `src/semantic/`) —
sería un módulo nuevo, algo como `src/runtime/gc.{h,cpp}`, que forma parte
del **backend/runtime**, no del frontend. Pero sí hay información que la
Etapa 2 ya está calculando y que el `mark` del punto 3.2 va a necesitar
directamente:

- **La forma de cada clase** (qué campos tiene y de qué tipo) — es
  exactamente lo que `ClassSymbol::class_scope` ya contiene una vez que
  `DeclarationCollector` recorre sus miembros. Con el
  [sistema de tipos](02_sistema_de_tipos.md) resuelto, cada campo de la
  clase va a tener su `Type` — y el `mark` necesita saber, para cada
  clase, cuáles de sus campos son `TypeKind::Class` o `TypeKind::Array`
  (punteros a otros bloques del heap que hay que seguir) contra cuáles
  son `Integer`/`String`/`Boolean` (datos planos, no se siguen). Esa
  tabla "descriptor de tipo por clase" se puede generar directamente
  recorriendo la tabla de símbolos ya construida — no hace falta
  recalcular nada desde cero.
- **Qué variables captura cada closure** — es justo la información que el
  documento de passes semánticos ya recomienda guardar en
  `FunctionSymbol` (sección de closures) aunque hoy no dispare ningún
  diagnóstico. Esa lista es exactamente el conjunto de raíces adicionales
  que una closure activa aporta al `mark`.

En otras palabras: el trabajo semántico de la Etapa 2/3 no es "aparte" del
GC — es la mitad del trabajo de diseño que un GC con conocimiento de tipos
necesita, y ya se va a haber hecho para cuando el equipo llegue a esta
fase.
