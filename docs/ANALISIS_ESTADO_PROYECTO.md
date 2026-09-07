# Análisis detallado del estado de Compiscript

> Informe histórico del estado anterior a las correcciones de los pases. Para la implementación actual y sus pruebas, consultar [03_passes_semanticos.md](03_passes_semanticos.md). Los hallazgos siguientes se conservan como evidencia de la revisión original.

**Fecha:** 6 de septiembre de 2026.  
**Revisión base:** `61dd444` — `feat: conectando ide viejo con nueva implementacion de codigo`.  
**Alcance:** código, gramática, interfaz, construcción, pruebas y documentación presentes en este checkout.

## 1. Diagnóstico general

El proyecto ya tiene una implementación considerable de un **frontend de compilador con análisis semántico y una IDE de escritorio**. Existe un recorrido completo desde el texto fuente hasta un AST propio, una tabla de símbolos y diagnósticos, con seis pases semánticos conectados. La interfaz ya invoca el compilador y consume sus resultados.

Sin embargo, **no es correcto considerar terminado el análisis semántico**. Hay verificaciones implementadas que cubren los casos básicos, otras que dependen del orden de declaración y varias situaciones inválidas que no producen ningún diagnóstico. También se identificaron problemas de estabilidad: una consulta de constructor sobre herencia circular puede bloquear el análisis; comparar ciertos tipos de función puede abortar el proceso; y la recuperación de redeclaraciones deja punteros sin propietario válido.

La documentación está desfasada en ambos sentidos: al principio del README presenta como pendientes funcionalidades que sí existen, mientras que al final afirma que toda la semántica está terminada. Además, marca la IDE como pendiente aunque ya hay una implementación de 2,488 líneas.

El estado más preciso es: **fundación y arquitectura implementadas; semántica ampliamente desarrollada, pero incompleta y con defectos; IDE integrada, pendiente de robustecimiento y validación visual; pruebas y documentación insuficientes para certificar una entrega cerrada**.

No se asigna un porcentaje global porque no está disponible en este checkout el enunciado oficial completo ni una matriz de requisitos verificable. Los comentarios que mencionan “el PDF” no sustituyen ese documento.

## 2. Cómo se realizó y qué se verificó

Se contrastó el README con la gramática, el pipeline real, los pases semánticos, la estructura del AST, el Makefile, el instalador y la IDE. También se revisaron los 22 fixtures existentes: **6 válidos y 16 inválidos**.

Se usan estas categorías de evidencia:

- **Implementación inspeccionada:** existe código concreto que realiza la operación; no equivale a cobertura exhaustiva.
- **Reproducido sobre AST:** se construyeron nodos directamente en un programa temporal de C++ y se ejecutaron los pases reales. Verifica la semántica sin depender de ANTLR.
- **Verificado por ejecución:** se ejecutó una prueba o comprobación específica y se observó su resultado.
- **Pendiente de validación:** la inspección identifica una limitación o riesgo, pero no se ejecutó el flujo completo correspondiente.

### Resultados de las comprobaciones

| Comprobación | Resultado | Qué demuestra y qué no |
|---|---|---|
| `make test-symbols` | **24 checks, 0 fallos** | La estructura básica de ámbitos y símbolos pasa su prueba aislada. No cubre los pases ni su manejo de errores. |
| `make test` | **No pudo iniciar los fixtures** | Falta `.deps/jre17/bin/java`; la generación del parser falla con error 127. No se atribuye un resultado a los 22 casos. |
| Compilación de pases semánticos en un programa temporal | Correcta | Estos componentes pueden compilarse sin ANTLR. |
| 11 escenarios sobre AST | 9 finalizaron; 1 excedió 2 segundos; 1 abortó | Confirman las limitaciones descritas en la sección 5. |
| Análisis de sintaxis de `ide/ide_app.py` | Correcto | El archivo es sintácticamente válido en el Python disponible. |
| Importación de la IDE sin crear una ventana | Correcta; Tkinter disponible | No certifica el funcionamiento visual. |
| Extracción de diagnósticos, AST y símbolos con salida de muestra | 3 comprobaciones correctas | El protocolo textual básico coincide con el formato actual del CLI. |
| Intento de usar AddressSanitizer | No enlazó: falta `libasan.so.8.0.0` | El defecto de vida útil de símbolos se fundamenta en inspección, no en una ejecución instrumentada. |

Las sondas de C++ se prepararon en `/tmp`, sin modificar la implementación del proyecto. Ejecutaron `DeclarationCollector`, `InheritanceResolver`, `NameResolver`, `TypeChecker` y `ControlFlowChecker`. No ejercitaron el parser ni `ClosureAnalyzer`. Los ejemplos `.cps` de los hallazgos reproducidos son la representación fuente de esos AST; **no se presentan como ejecuciones de punta a punta de archivos `.cps`**.

## 3. Arquitectura y componentes que ya existen

### 3.1 Pipeline real

La secuencia efectiva de [Compiler::compile](../src/compiler/compiler.cpp) es:

```text
Texto fuente
  → Lexer y parser ANTLR
  → AstBuilder
  → AST propio
  → DeclarationCollector
  → InheritanceResolver
  → NameResolver
  → TypeChecker
  → ControlFlowChecker
  → ClosureAnalyzer
  → CompilationResult { ast, diagnostics, symbol_table }
```

El CLI agrega la impresión del AST, la tabla de símbolos y la exportación DOT. La IDE invoca ese CLI como un proceso externo.

Esta separación está bien establecida: los tipos de ANTLR quedan encapsulados en `src/frontend/`; los pases semánticos trabajan con nodos propios. Cada compilación crea su propio recolector de diagnósticos y su tabla de símbolos. No hay un contador global de errores que deba reiniciarse entre compilaciones.

Si falla la fase léxica o sintáctica, `parse()` devuelve un AST nulo y se omite la semántica. Si un pase semántico emite errores, los pases posteriores **sí continúan**; esto permite acumular diagnósticos, pero exige que todos toleren estructuras inválidas. Actualmente esa condición no se cumple en todos los casos.

### 3.2 Inventario por área

| Área | Archivos principales | Estado real |
|---|---|---|
| Definición del lenguaje | `grammar/Compiscript.g4` | Gramática presente, con reglas léxicas y sintácticas. |
| Conversión a AST | `src/frontend/ast_builder.*`, `parser_driver.*` | Implementada e integrada; ejecución completa no verificada en esta sesión. |
| Modelo del AST | `src/ast/nodes.h` | Amplio y preparado para anotaciones semánticas. |
| Visualización del AST | `src/ast/printer.*` | Texto y DOT implementados. |
| Diagnósticos | `src/diagnostics/*` | Recolector, severidades, posiciones y ordenamiento implementados. |
| Símbolos y ámbitos | `src/semantic/scope.*`, `symbol.h`, `symbol_table.h` | Estructura básica verificada; recuperación de redeclaraciones defectuosa en el collector. |
| Declaraciones y nombres | `declaration_collector.*`, `name_resolver.*` | Recorridos integrados; faltan restricciones y decisiones de visibilidad. |
| Tipos | `type.*`, `type_checker.*` | Operaciones y validaciones extensas; todavía con omisiones y errores. |
| Herencia | `inheritance_resolver.*` | Enlace y detección de ciclos; tratamiento posterior de ciclos incompleto. |
| Flujo de control | `control_flow_checker.*` | Validación de contexto y código muerto local; no analiza todos los caminos. |
| Capturas | `closure_analyzer.*` | Recolección básica de símbolos externos; no implementación ejecutable de closures. |
| CLI | `src/main.cpp` | Lectura de un archivo, análisis y exportación. |
| IDE | `ide/ide_app.py` | Interfaz e integración presentes; no es una tarea por empezar. |
| Construcción | `Makefile`, `tools/setup.sh` | Flujo definido; dependencias ausentes en este entorno y portabilidad limitada. |
| Pruebas | `tests/` | Base pequeña; falta verificar diagnósticos exactos y casos límite. |
| Backend/runtime | Sin módulos correspondientes | No implementados en este checkout. |

## 4. Qué está hecho, con detalle

### 4.1 Gramática y frontend

La gramática contempla declaraciones `let`, `var` y `const`; funciones y clases; herencia con `:`; asignaciones; llamadas; acceso a miembros e índices; creación con `new`; arreglos; operadores; ternario; bloques; `print`; condicionales; cuatro formas de bucle; `switch`; `try/catch`; y sentencias de transferencia de control.

`AstBuilder` tiene conversiones para estas construcciones. Plega cadenas binarias hacia la izquierda, preserva la asignación recursiva hacia la derecha y construye sucesivamente los sufijos de expresiones como `obj.metodo()[0].campo`. También asigna línea y columna, usando columnas desde 1.

El frontend instala un listener propio tanto en el lexer como en el parser. Ambos tipos de errores se convierten actualmente en `SYN001`: existe integración de errores léxicos, pero no una clasificación léxica separada.

Hay métodos del visitor que devuelven `std::any()` porque el nodo padre procesa directamente esa regla, por ejemplo parámetros y sufijos. Esos métodos no deben contarse automáticamente como funcionalidades faltantes: la implementación explica y usa ese recorrido alternativo.

**Límites del lenguaje definido:** no hay `float`, `throw`, lambdas ni sintaxis general de tipos de función. `void` existe como tipo interno, pero no como alternativa reservada de `baseType`; una anotación `: void` se trataría como un identificador de tipo. Los bloques de `if` y bucles requieren llaves. Estas ausencias no son por sí mismas errores del compilador: corresponden al alcance de la gramática disponible.

### 4.2 AST y representación visual

[nodes.h](../src/ast/nodes.h) define nodos propios para sentencias, expresiones y anotaciones de tipo. Cada nodo dispone de posición y campos `symbol`, `scope` y `resolved_type`. La representación permite separar la construcción sintáctica de la resolución semántica.

El printer comparte una descripción de nodos para generar texto y DOT. Ya se puede inspeccionar la estructura sintáctica sin depender de la representación interna de ANTLR.

La existencia de `resolved_type` no significa que **todos** los nodos terminen tipados. Hay nodos sin valor que naturalmente no necesitan un tipo, y también símbolos o expresiones cuyo tipo queda desconocido por las limitaciones del checker. Las vistas actuales tampoco muestran sistemáticamente los tipos inferidos.

### 4.3 Tabla de símbolos y declaraciones

`Scope` implementa declaración local, detección de conflicto, búsqueda local y búsqueda ascendente. Conserva el símbolo original ante una redeclaración y permite shadowing entre ámbitos distintos. Los scopes hijos tienen propietario mediante `unique_ptr`; la tabla posee el scope global.

`DeclarationCollector` registra variables, constantes, parámetros, funciones y clases. Crea ámbitos para bloques, funciones, clases, `for`, `foreach` y `catch`. El cuerpo inmediato de una función comparte ámbito con sus parámetros. Los símbolos de función y clase conservan sus scopes asociados y sus metadatos.

Se registra la mutabilidad de constantes y variables. **Registrar este atributo está hecho; hacer cumplir esa mutabilidad no está hecho.**

La prueba de 24 comprobaciones cubre declaración, conflicto, conservación del original, shadowing, resolución multinivel, ausencia de nombres, metadatos de funciones/clases y el indicador de constante. Es una base útil, pero no prueba la interacción entre AST y símbolos.

### 4.4 Resolución de nombres

`NameResolver` recorre expresiones y sentencias, conecta identificadores con símbolos y emite `SEM001` para nombres inexistentes. Resuelve el destino de asignaciones simples y el nombre usado por `new`.

Los accesos `obj.campo` se resuelven posteriormente en `TypeChecker`, porque necesitan el tipo del objeto. Esto es una división de responsabilidades implementada, no una ausencia actual de resolución de miembros.

La búsqueda se realiza sobre una tabla que ya contiene todas las declaraciones. Esto permite encontrar nombres posteriores, pero no establece por sí mismo que su uso anticipado sea válido ni garantiza que sus tipos ya estén disponibles.

### 4.5 Sistema de tipos y operaciones

Existen representaciones para `integer`, `boolean`, `string`, `null`, `void`, error, arreglos, clases y funciones. Las anotaciones se resuelven contra primitivas o símbolos de clase; un nombre inválido produce `SEM013`.

El checker implementa:

- Inferencia de variables y constantes a partir del inicializador.
- Compatibilidad entre anotación e inicialización, y entre destino y valor asignado.
- Operaciones numéricas, lógicas, relacionales y de igualdad.
- `integer + integer` y `string + string`; la mezcla `string + integer` se rechaza.
- Condiciones booleanas de `if`, `while`, `do-while`, `for` y ternario.
- Compatibilidad entre ramas del ternario y entre sujeto y casos de `switch`.
- Homogeneidad de elementos de arreglos, índices enteros y rechazo de indexación sobre valores no arreglo.
- Comparación del valor de cada `return` encontrado con el retorno esperado.
- Cantidad y tipos de argumentos cuando la firma del invocable ya está resuelta.

`Type::equals()` considera compatible el tipo de error con cualquier otro para reducir diagnósticos en cascada. También permite `null` con clases, arreglos y funciones. Es una estrategia razonable de recuperación, pero actualmente se usa “error” para situaciones que nunca generaron un diagnóstico, con lo que puede ocultar programas inválidos.

### 4.6 Clases y herencia

`InheritanceResolver` enlaza clases base después de recolectar todas las clases, por lo que puede resolver una base declarada después. Reporta bases inválidas con `SEM013` y ciclos con `SEM014`.

`TypeChecker` busca atributos y métodos en la clase y después en sus bases, reporta miembros ausentes con `SEM010`, atribuye un tipo a `this` y emite `SEM015` cuando no encuentra una clase contenedora. También busca un método llamado `constructor` y valida sus argumentos si dispone de su firma. Una clase sin constructor rechaza argumentos adicionales.

Esto constituye soporte real de objetos a nivel estático. No incluye creación de objetos en memoria, despacho de métodos ni ejecución de constructores.

### 4.7 Control de flujo

`ControlFlowChecker` verifica `break` y `continue` fuera de bucles, `return` fuera de funciones y sentencias posteriores a un terminador directo en una misma lista de instrucciones. Una función anidada reinicia el contexto de bucle, evitando que su `break` use un bucle externo.

Se emite un solo `SEM012` por lista revisada. La implementación lo clasifica como **error**, aunque el recolector también soporte warnings.

La decisión actual es que `switch` no habilita por sí solo `break` o `continue`. El código conserva el contexto de un bucle externo si lo hay. Esta regla debe contrastarse con el enunciado antes de cambiarla; no corresponde asumir automáticamente las reglas de JavaScript o C.

### 4.8 Capturas de funciones

`ClosureAnalyzer` encuentra usos de símbolos fuera del ámbito de una función y los registra, sin duplicados, en `FunctionSymbol::captured`. Recorre lecturas y asignaciones y analiza las funciones anidadas con su propio contexto. El printer expone la lista como `[captura: ...]`.

Está hecha la **recolección básica de información de captura**. No hay entornos de ejecución, representación de celdas capturadas ni gestión de su vida útil. Además, el criterio actual puede incluir símbolos globales o funciones, porque no filtra únicamente variables de una función exterior.

### 4.9 CLI y diagnósticos

El ejecutable recibe exactamente una ruta, lee el archivo, llama a `Compiler::compile()`, imprime diagnósticos con código/severidad/posición, muestra AST y símbolos y escribe `output/ast.dot`. Devuelve 0 cuando no hay diagnósticos de error y 1 ante errores controlados de análisis, uso o lectura.

El recolector ordena los diagnósticos por posición y soporta longitud y warnings, aunque el CLI no serializa todos esos datos y no se encontraron reglas que emitan warnings actualmente.

La herramienta **analiza programas**: `print(...)` genera y valida un nodo, pero no imprime el valor del programa como lo haría un intérprete.

### 4.10 IDE: funcionalidades ya implementadas

La IDE usa Python/Tkinter y ya contiene:

- Un editor de fuente `.cps` con números de línea, resaltado, indicación de modificaciones, línea actual, correspondencia de delimitadores y autoindentación.
- Apertura, guardado, guardado como, búsqueda y atajos.
- Panel lateral y navegación entre vistas.
- Construcción mediante `make` y análisis mediante el ejecutable real.
- Guardado del archivo antes de iniciar el análisis.
- Ejecución del build y del analizador en hilos, con indicador de actividad y bloqueo de acciones simultáneas dentro de esa instancia.
- Consola, vista de errores y tabla de diagnósticos con navegación a línea y columna.
- Vista textual del AST y de símbolos, extraída de la salida del CLI.
- Renderizado DOT a PNG con Graphviz y visualización dentro de un canvas.
- Apertura externa de artefactos y tratamiento de rutas para Windows/WSL.
- Arrastrar y soltar mediante la dependencia opcional `tkinterdnd2`.
- Persistencia de geometría, ruta y selección de vistas en `.ide_state.json`.

Aunque usa un notebook, `_construir_editor()` crea actualmente **un solo editor de fuente**. No debe describirse como edición simultánea de múltiples archivos.

## 5. Qué está a medias o presenta defectos

### 5.1 Bloqueo al consultar clases con herencia circular — prioridad crítica

**Evidencia:** reproducido sobre AST. Una instancia de `A` con `A : B` y `B : A` no terminó dentro del límite de 2 segundos.

```cps
class A : B {}
class B : A {}
let a = new A();
```

`InheritanceResolver::detectCycle()` reporta el ciclo, pero deja los enlaces `base_class` intactos. El pipeline sigue con `TypeChecker`. Su `lookupMember()` recorre las bases sin registrar clases visitadas; al buscar un constructor ausente, vuelve indefinidamente a las mismas clases.

**Lo hecho:** detectar y reportar el ciclo. **Lo incompleto:** permitir que los pases posteriores finalicen con ese grafo inválido. Debe cortarse o marcarse la herencia inválida, o proteger las búsquedas contra ciclos. El fixture circular existente solo declara clases y no cubre esta consulta posterior.

### 5.2 Punteros colgantes después de una redeclaración — prioridad crítica

**Evidencia:** inspección de `Scope::declare()` y `DeclarationCollector::collectStatement()`; no verificado con AddressSanitizer por la dependencia faltante.

Cuando existe el nombre, `Scope::declare()` conserva el símbolo anterior y no almacena el nuevo. El collector reporta `SEM002`, pero igualmente asigna `n->symbol = sym.get()`. Al salir de la rama, el nuevo `shared_ptr` pierde su último propietario y el AST queda apuntando a memoria liberada.

El mismo patrón afecta declaraciones de funciones y clases; sus scopes también pueden conservar un `owner` inválido. Los pases siguientes usan esos punteros.

**Lo hecho:** detectar el conflicto. **Lo incompleto:** mantener un estado seguro después del conflicto. El resultado puede ser comportamiento indefinido, no solamente un diagnóstico adicional. Hay que preservar la vida del símbolo rechazado, usar una representación explícita de declaración inválida o adaptar los pases para omitirla de forma segura.

### 5.3 Parámetros sin tipo y comparación de funciones — prioridad crítica/alta

**Evidencia:** parámetros sin tipo aceptados sin diagnósticos; comparación de tipos de función abortó el proceso temporal con código `-6` y una aserción al desreferenciar un `shared_ptr` nulo.

```cps
function f(x) {}
f = f;
```

La gramática permite omitir el tipo de un parámetro. El checker almacena `nullptr` en la firma y nunca emite `SEM011`, aunque el código está reservado. `Type::equals()` compara parámetros mediante `param_types[i]->equals(...)` sin verificar si están presentes.

La asignación anterior no tiene por qué ser legal como decisión de lenguaje; el hallazgo es que el analizador debe **rechazarla o procesarla sin abortar**. Hace falta definir la política de parámetros no anotados y garantizar que toda firma pueda compararse de forma segura.

### 5.4 Validación dependiente del orden de declaración — prioridad alta

**Evidencia:** reproducido sobre AST; la primera versión produjo 0 diagnósticos y la segunda produjo `SEM008`.

```cps
// La firma todavía no está resuelta al revisar la llamada.
f("s");
function f(x: integer) {}
```

```cps
// La firma ya está resuelta: sí detecta el argumento incorrecto.
function f(x: integer) {}
f("s");
```

El collector conoce ambos nombres, pero el checker llena sus tipos durante un único recorrido en orden. Un identificador todavía sin tipo se convierte en `Error`, y la llamada no valida argumentos. El acceso a miembros y los constructores también dependen de que sus firmas ya se hayan procesado.

**Falta:** separar la preparación de firmas/tipos declarados de la revisión de cuerpos e inicializadores, o introducir una resolución diferida consistente. Conocer el nombre de una declaración posterior no basta para validar su uso.

### 5.5 Constantes y destinos de asignación — prioridad alta

**Evidencia:** la reasignación de una constante produjo 0 diagnósticos sobre AST.

```cps
const x = 1;
x = 2;
```

El símbolo tiene `is_mutable = false`, pero ninguna rama de asignación consulta ese campo. Tampoco existe una validación general que exija un destino asignable: las ramas revisan compatibilidad de tipos y no distinguen sistemáticamente variables, constantes, funciones o resultados temporales.

**Falta:** aplicar mutabilidad y comprobar la categoría del destino, tanto en sentencias como en asignaciones usadas como expresión y propiedades. Debe decidirse por separado si una constante de arreglo impide cambiar la referencia, los elementos o ambos; esa política no se debe inferir solo del nombre `const`.

### 5.6 Invocar valores no invocables y usar `new` sobre variables — prioridad alta

**Evidencia:** ambos escenarios produjeron 0 diagnósticos sobre AST.

```cps
let x = 1;
x();
```

```cps
let x = 1;
let instancia = new x();
```

En la llamada, si el tipo no es `Function`, el checker devuelve `Error` sin emitir un diagnóstico. En `new`, el name resolver verifica que el nombre exista, pero no que sea una clase; si el `dynamic_cast<ClassSymbol*>` falla, también se devuelve un tipo de error silenciosamente.

**Falta:** diferenciar un error ya informado de un uso inválido nuevo y emitir el diagnóstico correspondiente.

### 5.7 `foreach` no exige un arreglo — prioridad alta

**Evidencia:** reproducido sobre AST con 0 diagnósticos.

```cps
foreach (x in 1) {}
```

Si el iterable es un arreglo, se infiere correctamente el tipo del elemento. En cualquier otro caso se asigna `Error` a la variable de iteración sin reportar el problema.

**Lo hecho:** inferir el elemento del arreglo. **Falta:** validar que el operando sea iterable y definir exactamente qué tipos se admiten.

### 5.8 El parámetro del `catch` no termina tipado — prioridad alta

**Evidencia:** reproducido sobre AST con 0 diagnósticos para una resta que debería ser incompatible con la decisión documentada de tratar el error como `string`.

```cps
try {} catch (e) {
  print(e - 1);
}
```

El collector asigna a `e` una anotación `string`, pero no su `resolved_type`. La rama `TryCatchStatement` del checker solo recorre los bloques y no completa el tipo del símbolo. Cuando se usa `e`, se trata como tipo de error, suprimiendo incompatibilidades.

**Falta:** materializar la decisión de tipo en el símbolo del catch antes de revisar su cuerpo.

### 5.9 Retornos y código inalcanzable: análisis local — prioridad media/alta

**Evidencia:** una función con retorno `integer` y cuerpo vacío produjo 0 diagnósticos sobre AST. Las restantes limitaciones se identificaron por lectura del recorrido.

```cps
function f(): integer {}
```

El checker compara los `return` que encuentra; no comprueba que una función con retorno no vacío devuelva un valor en todos los caminos. `ControlFlowChecker` solo reconoce como terminador una instrucción directa `return`, `break` o `continue`.

Por ello, no propaga la terminación de un `if/else` cuyas dos ramas retornan, ni la de un bloque anidado. Tampoco hay un análisis de asignación definida antes de usar variables.

**Falta:** si el alcance exige estas reglas, representar resultados de flujo como “puede continuar”, “retorna” o “interrumpe” y combinarlos entre bloques y ramas. No hay actualmente un grafo de flujo de control.

### 5.10 Visibilidad y scopes: decisiones por completar — prioridad media

**Evidencia:** inspección del collector y del resolver.

- El `switch` no crea un scope propio: todos sus casos usan directamente el ámbito que lo contiene. Una declaración dentro de un caso puede quedar visible después del switch. El comentario habla del “scope del switch”, pero no se crea uno.
- Las variables se recolectan antes de resolver usos; no se controla uso antes de declaración ni autoinicialización. Debe definirse la política del lenguaje.
- `let` y `var` se conservan como palabras distintas en el AST, pero se recolectan con el mismo comportamiento de ámbito y mutabilidad.
- `this` se admite al encontrar cualquier scope de clase ancestro. No se exige explícitamente estar en un método, a pesar del texto de `SEM015`; esto incluye contextos como inicializadores de miembros y funciones anidadas dentro de métodos.

Estos puntos requieren reglas explícitas. No todos implican automáticamente que deba copiarse la semántica de TypeScript.

### 5.11 Herencia y tipos: soporte nominal limitado — prioridad media

`Type::equals()` considera iguales dos tipos de clase solo cuando apuntan al mismo `ClassSymbol`. No recorre las bases para admitir una instancia derivada donde se espera una base.

Tampoco se encontró validación de compatibilidad entre firmas de métodos sobrescritos. La búsqueda toma el primer miembro con ese nombre y no verifica que preserve el contrato del miembro heredado.

**Lo hecho:** resolver herencia y buscar miembros. **Lo pendiente de decidir/implementar:** subtipado, reglas de sobrescritura y restricciones especiales de constructores. Herencia de miembros y compatibilidad entre tipos son problemas distintos.

### 5.12 Closures: metadatos básicos, no soporte completo — prioridad media

La recolección actual no captura explícitamente `this`, no propaga necesidades de entorno a través de funciones intermedias y puede incluir nombres globales o funciones como capturas. Además, `walkStatement()` omite clases anidadas dentro de una función: los métodos de esas clases no se analizan por esa ruta.

Antes de usar `captured` para generación de código, hay que definir qué símbolos requieren almacenamiento en un entorno y cómo se transmite entre niveles. No se verificaron estos casos mediante pruebas ejecutables en esta revisión.

### 5.13 IDE: integración terminada en lo básico, robustez incompleta

**Evidencia:** inspección de código y comprobaciones de helpers sin ventana.

| Limitación | Impacto |
|---|---|
| Consume salida humana mediante regex y encabezados | Cambiar el formato del CLI puede romper las vistas. No existe contrato JSON. |
| Build y análisis usan `subprocess.run()` sin timeout ni cancelación | Un analizador bloqueado puede dejar la interfaz permanentemente en estado de proceso activo. |
| Graphviz se ejecuta sin hilo en `renderizar_ast()` | Un renderizado costoso puede bloquear la interfaz. |
| Se reutilizan `output/ast.dot` y `output/ast.png` | Los artefactos no quedan asociados de forma inequívoca a la última ejecución. |
| Un error sintáctico retorna antes de sobrescribir el DOT | La IDE puede mostrar o renderizar el AST de una ejecución anterior. |
| `_on_cerrar()` guarda estado de sesión y destruye la ventana, sin revisar modificaciones sin guardar | Hay riesgo de perder texto editado al cerrar. Guardar rutas de sesión no guarda el contenido del editor. |
| Un solo editor fuente | La estructura de pestañas no representa todavía un workspace de varios archivos abiertos. |
| Resaltado incluye `void` | Puede sugerir una anotación que la gramática no define como primitiva. |
| Dependencias y arranque de IDE no documentados de forma completa | Tkinter, Graphviz y la opción de drag-and-drop necesitan instrucciones claras. |

No se abrió la interfaz ni se validaron visualmente tamaños, atajos, drag-and-drop o compatibilidad entre plataformas. La revisión confirma implementación e integración textual, no una certificación de experiencia de usuario.

### 5.14 Construcción, instalador y salida — prioridad media

El Makefile genera código ANTLR y compila C++17 con `-Wall`. La generación depende del archivo de gramática y las pruebas de símbolos no requieren el frontend. El instalador prepara dependencias locales sin sudo.

Quedan estos puntos:

- **Dependencias ausentes:** este checkout no trae `.deps/`, parser generado ni ejecutable, lo cual es coherente con `.gitignore`, pero impide validar inmediatamente el sistema completo.
- **Headers fuera de las dependencias de build:** los objetivos binarios enumeran `.cpp`, pero no `.h` ni archivos de dependencias automáticas. Cambiar únicamente un header puede dejar un binario desactualizado al ejecutar `make`.
- **Recompilación monolítica:** un cambio de fuente reconstruye todos los `.cpp` en una sola invocación; no hay compilación incremental por objetos.
- **Portabilidad parcial:** el Makefile considera `.exe` y la IDE contiene adaptaciones de Windows/WSL, pero `setup.sh` descarga JRE y CMake para Linux x86-64 y usa `nproc`. No es un instalador universal para macOS, Windows nativo o ARM.
- **Prerrequisitos externos:** el script presupone `curl`, Python, herramientas de extracción, compilador y utilidades de construcción; no instala todo desde cero.
- **Manejo de exportación:** el CLI no comprueba explícitamente si se abrió/escribió correctamente el DOT y no captura errores de creación del directorio de salida.
- **Interfaz de un archivo:** no hay opciones de stdin, formato de salida estructurado, ruta de artefactos o análisis de un proyecto completo.

## 6. Cobertura semántica y pruebas existentes

### 6.1 Matriz de códigos

“Fixture dedicado” significa que existe un archivo dirigido a ese caso; **no significa que su código esperado se compruebe automáticamente**.

| Código | Comportamiento implementado | Fixture dedicado | Observación |
|---|---|---|---|
| `SYN001` | Error del lexer/parser | Sí, falta de llave | Se comparte código entre léxico y sintáctico. |
| `SEM001` | Nombre inexistente | Sí | No comprueba categoría de símbolo en `new`. |
| `SEM002` | Redeclaración | Sí | El manejo del símbolo rechazado es inseguro. |
| `SEM003` | Asignación/inicialización incompatible | Sí | No comprueba mutabilidad; omite tipos desconocidos. |
| `SEM004` | Operadores, arreglos, índices, ternario y casos de switch | Sí, aritmética | Un caso no cubre todas estas ramas. |
| `SEM005` | Condición no booleana | Sí, `if` | Faltan casos negativos dedicados para las demás condiciones. |
| `SEM006` | `break`/`continue` fuera de bucle | Sí, `break` | Faltan combinaciones con funciones y switches. |
| `SEM007` | `return` fuera de función | Sí | Valida contexto, no cobertura de retornos. |
| `SEM008` | Argumentos de funciones, métodos y constructores | Sí, método y constructor | Depende de que la firma ya esté tipada. |
| `SEM009` | Tipo de un `return` encontrado | No | Tampoco se detecta la falta total de retorno. |
| `SEM010` | Miembro ausente o acceso sobre valor no objeto | Sí, miembro inexistente | Falta cubrir cadenas, herencia y orden de miembros. |
| `SEM011` | Parámetro sin anotación | No | **Solo declarado; no se emite.** |
| `SEM012` | Código muerto después de terminador directo | Sí | No propaga terminación entre estructuras. |
| `SEM013` | Tipo o clase base inválidos | Sí, dos fixtures | Está implementado para anotaciones y bases. |
| `SEM014` | Herencia circular | Sí | El fixture no consulta miembros ni instancia clases del ciclo. |
| `SEM015` | `this` sin clase contenedora | Sí | El criterio implementado es más amplio que “dentro de método”. |

Los seis fixtures válidos son `hello.cps`, `completo.cps`, `tipos.cps`, `herencia.cps`, `clases_y_objetos.cps` y `control_flow_y_closures.cps`. Aportan ejemplos de uso y recorridos integrados, pero no demuestran exhaustividad.

### 6.2 Debilidad del runner actual

`make test` únicamente clasifica por código de salida: 0 para válidos y cualquier valor distinto de 0 para inválidos. Descarta tanto stdout como stderr y no valida código de diagnóstico, severidad, ubicación o cantidad.

Esto tiene consecuencias concretas:

1. Un fixture pensado para `SEM003` puede pasar aunque solo produzca un error sintáctico.
2. Un proceso que aborta o falla por memoria puede contarse como un inválido correctamente rechazado.
3. Un caso que entra en un ciclo puede detener la suite completa porque no hay timeout.
4. La prueba de símbolos no se ejecuta como dependencia de `make test`.

### 6.3 Qué falta para una batería convincente

- Casos positivos y negativos por cada regla, con códigos y posiciones esperados.
- Pruebas de referencias hacia adelante y permutación del orden de funciones, clases y miembros.
- Regresiones de los bloqueos, abortos y redeclaraciones.
- Parámetros no anotados, constantes, no invocables, `new` inválido, `foreach` no iterable y tipo del catch.
- Retornos completos e incompletos, código muerto tras estructuras y restricciones de ámbito.
- Pruebas del AST para precedencia, asociatividad y sufijos encadenados.
- Pruebas de herencia con consultas posteriores a ciclos y bases inválidas.
- Capturas multinivel, clases dentro de funciones y tratamiento de `this`.
- Compilaciones repetidas en el mismo proceso usando `Compiler::compile()`.
- Pruebas de la integración IDE/CLI y manejo de artefactos obsoletos.
- Ejecución automatizada en integración continua; no se encontró una configuración de CI en el checkout.

## 7. Documentación que debe actualizarse

El [README](../README.md) tiene estas inconsistencias verificables:

| Afirmación o referencia | Estado observado |
|---|---|
| El pipeline termina después de resolución de nombres | El pipeline real incluye herencia, tipos, flujo y capturas. |
| Tipos, miembros, `this` y argumentos siguen sin implementar | Hay código e integración para todas esas áreas, con limitaciones. |
| `resolved_type` permanece siempre nulo | El checker lo llena en numerosos símbolos y expresiones. |
| `string + integer` está por decidir | El checker ya lo rechaza explícitamente. |
| Todo el análisis semántico está completo | Las omisiones y fallos reproducidos contradicen esa conclusión. |
| La IDE está pendiente | `ide/ide_app.py` implementa e integra la interfaz. |
| Documentos `docs/02_sistema_de_tipos.md` a `docs/05_generacion_de_codigo_y_runtime.md` | Los cuatro archivos referenciados no están presentes en el checkout. |

También hay comentarios desactualizados en `codes.h`, que presentan códigos semánticos como aún no emitidos, y en el checker, que habla de miembros pendientes de una fase ya incorporada.

Falta una guía actual de la IDE: arranque con `python3 ide/ide_app.py`, dependencias, diferencia entre construir el compilador y analizar el fuente, uso de Graphviz y límites de plataforma. `.ide_state.json` contiene estado local de la interfaz y no está excluido por el `.gitignore` actual; conviene decidir si debe versionarse.

El README menciona una rama histórica `Deprecated`. Este informe evalúa el checkout actual; no certifica el contenido ni la integridad de esa otra rama.

## 8. Qué no está implementado y cómo clasificarlo

No se encontraron módulos de representación intermedia, generación de código, optimización, máquina virtual, intérprete, runtime de objetos, manejo ejecutable de excepciones ni garbage collector.

Esto no significa que el frontend esté vacío: son etapas posteriores a la infraestructura ya desarrollada. El propio README sitúa generación de código y runtime fuera de Proyecto 2. Sin el enunciado completo, se deben tratar como **trabajo futuro según el alcance documentado**, no como requisitos incumplidos de esta entrega.

De igual forma, `try/catch`, `new`, closures y `print` tienen representación o análisis estático, pero no comportamiento ejecutable del programa fuente.

## 9. Orden de trabajo recomendado para cerrar lo existente

### Prioridad 0: garantizar que todo análisis termine de forma controlada

1. Corregir la vida útil de símbolos rechazados por redeclaración.
2. Proteger las búsquedas de miembros y constructores frente a herencia circular.
3. Evitar parámetros nulos en comparaciones de tipos de función.
4. Incorporar regresiones que exijan un diagnóstico y terminación normal, con timeout; un aborto no debe contarse como éxito.

**Criterio de cierre:** los casos inválidos anteriores producen diagnósticos y el proceso termina sin bloqueo ni fallo de memoria.

### Prioridad 1: completar las verificaciones semánticas básicas

1. Preparar firmas antes de revisar usos y cuerpos.
2. Definir y aplicar la política de parámetros sin tipo y variables sin tipo/inicializador.
3. Verificar mutabilidad y destinos asignables.
4. Reportar llamadas a no funciones y `new` sobre no clases.
5. Validar el iterable de `foreach` y completar el tipo del catch.
6. Definir alcance de retornos, scopes de switch, uso anticipado, `this`, subtipado y sobrescritura según el enunciado.

**Criterio de cierre:** la matriz de requisitos distingue reglas implementadas, decisiones del lenguaje y exclusiones; los resultados no cambian accidentalmente por el orden de las declaraciones.

### Prioridad 2: fortalecer pruebas, build e IDE

1. Asegurar un entorno de ANTLR reproducible y ejecutar los 22 fixtures.
2. Verificar códigos, posiciones y severidad, además de la salida del proceso.
3. Incluir headers en las dependencias de construcción.
4. Invalidar artefactos de ejecuciones anteriores al analizar un archivo nuevo.
5. Agregar timeout/cancelación y confirmación o guardado seguro al cerrar con cambios.
6. Verificar visualmente el flujo abrir → editar → guardar → analizar → navegar al diagnóstico → renderizar AST.

**Criterio de cierre:** una máquina documentada puede construir y ejecutar las pruebas; la IDE muestra resultados del análisis actual y se recupera de fallos del proceso externo.

### Prioridad 3: documentación y evidencia de entrega

Actualizar el README alrededor del pipeline actual, restaurar o sustituir enlaces rotos y añadir instrucciones de IDE. La afirmación de “terminado” debe apoyarse en una matriz del enunciado y resultados reproducibles, no solo en la existencia de archivos.

## 10. Balance final por estado

**Ya desarrollado:** gramática, integración de ANTLR, AST propio, diagnósticos, tablas de símbolos, recolección de declaraciones, resolución de nombres, infraestructura de tipos, numerosas reglas semánticas, enlace de herencia, control de flujo local, metadatos de captura, CLI, exportación y una IDE integrada.

**A medias:** recuperación tras errores, consistencia de tipos y firmas, restricciones de constantes/invocación/iteración, parámetros y catch, reglas completas de flujo y objetos, semántica avanzada de capturas, robustez de la IDE, cobertura automática y documentación.

**Fuera de la implementación actual:** ejecución del programa, backend y runtime. Su necesidad para una entrega depende del alcance oficial.

El siguiente avance debe concentrarse en **estabilidad, cierre de huecos semánticos y pruebas que comprueben el error esperado**. La base ya permite hacerlo sin reconstruir el proyecto desde cero.
