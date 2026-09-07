# Pases semánticos de Compiscript

Esta guía describe la implementación actual y las decisiones aplicadas al completar los pases. El frontend continúa usando la gramática ANTLR existente; no se implementan ejecución, IR ni runtime.

## Pipeline

```text
ANTLR → AstBuilder → DeclarationCollector → InheritanceResolver
      → NameResolver → TypeChecker (preparación + comprobación)
      → ControlFlowChecker → ClosureAnalyzer
```

`Compiler::compile()` mantiene la fachada y devuelve AST, diagnósticos ordenados y tabla de símbolos. Un error léxico/sintáctico impide construir el AST; los errores semánticos permiten continuar los pases para reportar problemas independientes.

El Visitor de ANTLR construye el AST propio. Los pases semánticos recorren ese AST: no duplican la gramática ni requieren tipos de ANTLR. Este reparto debe explicarse en la entrega, pues el PDF describe un Listener/Visitor aplicando las reglas semánticas.

## 1. Declaraciones y recuperación

`DeclarationCollector` crea los entornos y registra variables, constantes, parámetros, funciones y clases. `Scope::declare()` conserva el símbolo original cuando encuentra un duplicado y el collector emite `SEM002`.

El símbolo rechazado se conserva en `Scope::rejected_symbols_`, marcado con `is_rejected`. No participa en búsquedas de nombres ni sustituye al original. De esta manera los enlaces no propietarios del AST y `Scope::owner` siguen siendo válidos incluso para funciones y clases duplicadas. Sus cuerpos se pueden revisar con sus propios metadatos sin modificar los de la primera declaración.

`Symbol::declaring_scope` identifica el entorno donde se introdujo el símbolo. `Symbol::declaration` enlaza variables y constantes con su declaración para completar inferencia bajo demanda. Son referencias no propietarias: los ámbitos poseen los símbolos y el resultado conserva el AST.

Los parámetros duplicados también se conservan por separado. La firma mantiene las posiciones escritas y el cuerpo resuelve el primer parámetro declarado con ese nombre.

## 2. Herencia segura

`InheritanceResolver` enlaza las bases después de recolectar las clases y mantiene `SEM013` para bases inexistentes/no clases y `SEM014` para herencia circular.

La estrategia de recuperación elegida es **proteger las búsquedas**: `TypeChecker::lookupMember()` registra las clases visitadas y deja de recorrer al repetir una. La búsqueda de constructor usa la misma función. Los enlaces del ciclo permanecen como información del programa inválido, pero ningún recorrido actual de búsqueda de miembros puede quedarse girando en ese ciclo.

Un programa con herencia circular siempre es inválido aunque pueda encontrarse algún miembro antes de repetir una clase. Reportar un ciclo no se considera permiso para aceptar el programa.

## 3. Preparación de tipos y firmas

`TypeChecker::run()` tiene dos fases:

1. `prepareScope()` visita toda la tabla, incluidos símbolos rechazados y scopes hijos. Resuelve anotaciones, tipos de clase, parámetros y firmas de funciones/métodos/constructores.
2. `checkStatement()` y `checkExpression()` revisan inicializadores, cuerpos, operadores y usos con las firmas ya preparadas.

Se conserva la representación existente: `FunctionSymbol::params` y `return_type` contienen las anotaciones fuente; `FunctionSymbol::resolved_type` contiene un `TypeKind::Function` con `param_types` y `return_type` resueltos. No se añaden campos paralelos que puedan quedar inconsistentes.

Las anotaciones de parámetros y retorno se resuelven en el entorno de declaración de la función, no entre sus locales. Un local que oculte el nombre de una clase no cambia retroactivamente la firma.

### Inferencia

Las variables/constantes sin anotación se infieren de su inicializador. Si un uso necesita el tipo de una declaración aún no comprobada, `symbolType()` revisa esa declaración bajo demanda. El conjunto `checked_` evita revisar dos veces una declaración y permite detectar dependencias de inferencia circulares.

- `let x = 1;` infiere `integer`.
- `let x: integer;` conserva el tipo declarado sin exigir inicializador.
- `let x;` emite `SEM019`: no hay información para inferir.
- `let x = y; let y = x;` emite `SEM019` y se recupera con `Error`.
- Un resultado `void` no se puede almacenar como dato ni imprimir; los elementos de arreglos tampoco pueden ser `void`.

Esto no implementa un análisis de asignación definida ni una zona temporal muerta. Los nombres se siguen resolviendo sobre todas las declaraciones del ámbito, como en la arquitectura previa.

### Parámetros y retorno

Se aplica la política de **parámetros con anotación explícita** registrada en `SEM011`. Un parámetro sin anotación produce ese diagnóstico y obtiene tipo `Error`. Las firmas construidas no conservan parámetros nulos; `Type::equals()` además tolera estructuras incompletas sin desreferenciar punteros nulos.

Una función sin anotación de retorno tiene retorno interno `Void`. La gramática no incorpora la palabra reservada `void` como tipo primitivo: los procedimientos omiten la anotación.

## 4. Asignación y mutabilidad

Se validan destino, mutabilidad y compatibilidad de tipos en asignaciones como sentencia o expresión, incluyendo propiedades e índices.

- Se puede asignar a variables, parámetros mutables, atributos mutables y elementos de arreglos.
- No se puede asignar al nombre de una función o clase, una constante, un método ni un resultado temporal.
- `SEM003` cubre tanto incompatibilidad de tipos como destino no asignable/inmutable. La descripción del mensaje distingue la causa.
- La gramática ya exige inicializar `const`; su omisión produce `SYN001`.

**Política de const conservada:** la inmutabilidad es superficial. No se puede cambiar el vínculo de una constante ni el valor de un atributo declarado `const`; sí se pueden modificar elementos de un arreglo referenciado por una constante y atributos mutables de un objeto referenciado por ella. No se añade inmutabilidad profunda, que el enunciado no especifica.

```cps
const arreglo = [1, 2];
arreglo[0] = 5;  // válido
// arreglo = [3];  → SEM003
```

## 5. Llamadas, miembros y constructores

Las llamadas verifican que el valor tenga tipo función (`SEM016` si no lo es), y después cantidad y tipos posicionales de argumentos (`SEM008`). La validación no depende de que la función o método aparezca antes o después del uso.

Los accesos a miembros buscan primero en la clase y después en sus bases; un miembro ausente o un receptor no objeto produce `SEM010`.

`new` exige un símbolo de clase: un nombre inexistente produce `SEM001`; un símbolo existente que no es clase produce `SEM017`. Se busca el miembro `constructor`, incluso heredado, y se revisa su firma. Si un miembro con ese nombre no es un método, se emite `SEM017`. Sin constructor, solo se acepta la lista vacía de argumentos.

No se agregaron reglas de subtipado ni de sobrescritura: el PDF proporcionado no exige compatibilidad derivada→base ni una política de override. Dos tipos de clase siguen comparándose por identidad nominal. Esas extensiones requieren una decisión explícita; no se importan las reglas de TypeScript.

## 6. Arreglos, foreach y catch

Los arreglos verifican homogeneidad y sus índices deben ser `integer`. Indexar un valor que no es arreglo produce `SEM004`.

`[]` usa `EmptyElement` como marcador contextual, **no `Error`**. Se puede asignar a un arreglo anotado o combinar con elementos que determinen su tipo, por ejemplo `let a: integer[] = [];` o `[[], [1]]`. Leer/escribir un elemento de un arreglo vacío sin tipo determinado produce `SEM019`; debe anotarse el tipo. No se implementa inferencia a partir de futuras mutaciones. Las combinaciones compatibles con `null` prefieren el tipo de referencia conocido.

`foreach` solo admite arreglos. Un operando de otro tipo produce `SEM018` y su variable recibe `Error` para evitar cascadas. Para un arreglo válido, la variable recibe el tipo de elemento; usar un elemento aún indeterminado requiere información de tipo y produce `SEM019`.

El símbolo de `catch` tiene anotación `string` desde el collector y tipo resuelto `String` desde la preparación de tipos. Por tanto `e - 1` produce `SEM004`, mientras que `e + "texto"` es válido.

## 7. Flujo de control

El checker mantiene `SEM005` para condiciones no booleanas, `SEM006` para `break`/`continue` fuera de bucles y `SEM007` para `return` fuera de funciones. Una función anidada reinicia su contexto de bucle.

`ControlFlowChecker` combina conjuntos de salidas: continuación normal, retorno, break y continue. La combinación permite representar ramas con resultados distintos sin confundir una salida parcial con terminación total.

- Una lista solo conecta la siguiente instrucción a los caminos que continúan.
- `if/else` une las salidas de ambas ramas; sin else conserva un camino de continuación.
- Los bloques propagan sus salidas.
- Los bucles consumen sus break/continue; se consideran la ejecución inicial obligatoria de do-while y condiciones booleanas literales. Un foreach puede estar vacío.
- El switch considera entrada en cada caso y caída al siguiente; sin default mantiene un camino que no coincide con ningún caso.
- Try/catch combina conservadoramente las salidas de ambos cuerpos.
- Declarar una función o clase no ejecuta su cuerpo.

`SEM012` se emite en la primera instrucción inalcanzable de cada lista, incluso después de un bloque o un if/else terminante. Los cuerpos inalcanzables todavía se revisan para errores independientes.

`SEM009` verifica el tipo de cada retorno encontrado. Se añade `SEM020` cuando una función con retorno anotado puede alcanzar su final sin devolver valor. Esta es la política de cobertura estructural adoptada al completar el apartado de retornos del documento de trabajo; el PDF no la enumera como regla separada. Una función que diverge en un bucle literalmente infinito no alcanza su final. No se realiza evaluación general de constantes ni demostración de terminación.

### Decisiones de switch y discrepancias del PDF

Se conserva la regla documentada: switch no cuenta como bucle para break/continue. Si existe un bucle exterior, esas sentencias se refieren a él.

El documento de trabajo pide mantener la compatibilidad switch/case y la implementación previa usa sujetos de cualquier tipo comparable. Se conserva esa política. **El PDF dice que el switch debe ser booleano**, lo que no coincide con los ejemplos actuales: requiere aclaración para la entrega. Asimismo el PDF menciona `float`, pero la gramática oficial presente solo ofrece `integer`, `boolean`, `string` y nombres de clase. No se modifica la gramática ni se añade float en esta tarea.

## 8. This y closures

Se conserva la política de **this léxico en ámbito de clase**, consistente con el apartado de clases del PDF: métodos, constructor, inicializadores de atributos y funciones anidadas en esos métodos pueden usarlo. Fuera de una clase se emite `SEM015`. Una clase anidada introduce su propio receptor.

`ClosureAnalyzer` reutiliza su recorrido y registra:

- Variables, constantes y parámetros de entornos exteriores que necesitan retenerse.
- Propagación a funciones intermedias: si c usa x de a a través de b, tanto c como b conservan x; a no captura su propio local.
- Exclusión de símbolos globales directos, nombres de funciones y clases.
- Exclusión de variables propias, incluyendo las declaradas en bloques del cuerpo.
- Los atributos/métodos de clase se acceden mediante el receptor, no como variables locales capturadas.
- Receptor léxico en `FunctionSymbol::captured_this`, separado de `captured`. Un método recibe this directamente; las funciones anidadas que lo necesitan lo capturan y propagan.
- Métodos de clases declaradas dentro de funciones.

Las listas no tienen duplicados. El printer muestra `[captura: ...]` y `[captura this: Clase]`. No hay celdas de almacenamiento, objetos de entorno ni ejecución de closures; se conserva únicamente la información estática.

## 9. Diagnósticos

| Código | Regla |
|---|---|
| SYN001 | Error léxico/sintáctico |
| SEM001 | Nombre no declarado |
| SEM002 | Redeclaración en un mismo entorno |
| SEM003 | Tipo incompatible o destino de asignación no mutable/asignable |
| SEM004 | Operación, comparación, lista, índice o case incompatible |
| SEM005 | Condición no booleana |
| SEM006 | Break/continue fuera de bucle |
| SEM007 | Return fuera de función |
| SEM008 | Número o tipos de argumentos incorrectos |
| SEM009 | Tipo de retorno incompatible |
| SEM010 | Acceso a miembro inválido |
| SEM011 | Parámetro sin anotación |
| SEM012 | Código inalcanzable |
| SEM013 | Anotación de tipo/base inválida |
| SEM014 | Herencia circular |
| SEM015 | This fuera del ámbito de clase |
| SEM016 | Valor no invocable |
| SEM017 | New sobre no clase o constructor no método |
| SEM018 | Foreach sobre no arreglo |
| SEM019 | Inferencia indeterminada/circular o uso de void como dato |
| SEM020 | Función que puede terminar sin devolver el valor declarado |

`Error` permite continuar después de un diagnóstico. Los usos inexistentes no provocan una cascada de errores de llamada, miembro e índice. El marcador de arreglos vacíos permite distinguir falta de contexto de un error previamente informado.

## 10. Verificación

```bash
bash tools/setup.sh    # primera vez; dependencias locales Linux x86-64
make test             # build, símbolos, metadatos, runner y fixtures
make test-symbols     # no requiere ANTLR
make test-semantic    # no requiere ANTLR; AST y metadatos
```

`tests/run_fixtures.py` ejecuta cada archivo con timeout de 5 segundos y artefactos en un directorio temporal. `tests/fixtures/expected.json` enumera el conjunto exacto de códigos esperado para cada inválido; el runner falla si falta algún fixture en el manifiesto o sobra una entrada.

Un inválido debe terminar con código **1**, emitir exactamente los códigos previstos y posiciones positivas. Un válido debe terminar con código **0**, sin errores. Un crash, stderr inesperado, timeout o diagnóstico de otra regla no cuenta como éxito. Se comparan conjuntos de códigos; no se exige una cantidad fija de repeticiones de un mismo código ni mensajes literales.

Las regresiones incluyen ciclos con instancia/consulta de miembro, símbolos duplicados usados después, parámetros sin tipo y autoasignación, firmas antes/después del uso, constantes, temporales, no invocables, new inválido, foreach, catch, ramas terminantes y capturas multinivel. Las pruebas C++ inspeccionan propiedad de símbolos, firmas y capturas; las pruebas Python comprueban que el runner rechace resultados engañosos.

Validación realizada con `make test`: **140 fixtures (30 válidos y 110 inválidos), 24 comprobaciones de símbolos, 40 comprobaciones semánticas y 9 pruebas del runner; todos pasan**, sin crashes ni timeouts. La compilación final no emitió warnings. No se ejecutó una comprobación instrumentada con sanitizers porque sus bibliotecas no están disponibles en este entorno.

El Makefile también depende de los headers para evitar ejecutar binarios obsoletos después de cambiar interfaces. Los headers externos de ANTLR se marcan `-isystem`: se mantienen los warnings de código propio sin presentar avisos de headers de terceros como defectos propios.

## Fuera de alcance

Generación de código, IR, VM/intérprete, garbage collector, memoria de objetos, ejecución de closures y excepciones. También quedan fuera cambios de política no pedidos: subtipado, contratos de override, diferencias nuevas entre let/var, zona temporal muerta y análisis de asignación definida.
