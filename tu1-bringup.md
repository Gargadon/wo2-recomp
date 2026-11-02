# Bring-up con Title Update #1

## Corrección verificada del BGM (2026-10-31)

El usuario confirmó que el BGM vuelve a escucharse con
`patches/rexglue-event-guest-sync.patch`, aplicado después del guard de NULL.
Las conclusiones anteriores sobre un fallo propio del juego no estaban
demostradas: había una discrepancia real entre el evento huésped y el host.

Hallazgos comprobados en código TU1 y SDK:

- `sub_8233F3A8` borra SignalState con un store directo en evento+4.
- `XEvent::InitializeNative` no conservaba la dirección del KEVENT, por lo
  que `SetGuestSignalState` no actualizaba estos eventos.
- Faltaba reconciliar esos stores directos al recuperar el objeto nativo.
- `Initialize` tampoco guardaba manual_reset_ ni inicializaba tipo/estado
  del header. El parche corrige estos puntos y serializa las actualizaciones.

Prueba opcional `--wo2_event_self_test`: PASS para retención de dirección,
Set, clear/set inline y waits de eventos manuales/automáticos. Build Release
completo y arranque/cierre normales verificados. Las tres ranuras del pool
siguen válidas después de27s (40060AD0,40060AF0,40060B10), tanto con límite
como sin él. No se repara el índice ni se fuerza un resultado de espera.

Medición aproximada de CPU en arranque/menú, tras12s de calentamiento:
baseline anterior49.031s CPU /15.374s reales; corrección sin límite de FPS
33.547s /15.303s. Con límite60:25.469s /15.303s. Son muestras individuales,
no un benchmark de batalla. Datos en out/profiles/tu1-event-*.json.

Se restauró sólo el hook de frames TU1, verificado contra el bl VdSwap en
8231B874 (retorno8231B878). Los hooks de espera/video antiguos permanecen
desactivados; sus direcciones sin TU no deben reutilizarse.
La lentitud gráfica previa no está resuelta. El siguiente perfil de stage
debe usar este build, con el streaming funcionando y el contador TU1 activo.

Estado de la recompilación de **Warriors Orochi 2 (USA)** contra la imagen
parcheada con **Title Update #1**.

## Por qué fallaba el arranque (log 046)

El ejecutable cargaba la imagen **sin parchear**:

- `game:\default.xex` (base, versión 0.0.0.3) tiene entry point **0x823027E0**.
- El código recompilado se generó desde la imagen **parcheada** (0.0.1.3), cuyo
  entry point es **0x823028F8** (el TU mueve el entry point +0x118 y la sección
  de código crece 0x200: `code=82110000-824BFAC4`).
- El runtime solo aplica un XEXP que encuentre como `<modulo>.xexp` junto al
  módulo (`src/system/user_module.cpp`). El parche estaba en
  `Warriors Orochi 2 Title Update #1\default.xexp`, así que nunca se aplicaba.
- Resultado: el hilo principal arrancaba en 0x823027E0, dirección que no existe
  como función en la tabla recompilada →
  `XThread::Execute - No function registered at 823027E0` y salida con
  `0xC0000374`.

### Arreglo

`src/prepare_tu.cpp` ahora copia `default.xexp` junto a `default.xex`, de modo
que codegen y runtime cargan exactamente la misma imagen. El log debe mostrar
`XEX patch applied successfully` y `Calling function at 823028F8`.

## Configuración recompilada desde cero

`warriors_orochi_2_config.toml` se reconstruyó contra la imagen TU1 con 67
entradas, cada una verificada contra la imagen parcheada volcada en memoria:

| grupo     | nº | criterio |
|-----------|----|----------|
| analysis  | 2  | `b` a una dirección que el escaneo no atribuye a ninguna función |
| thunks    | 15 | thunks de ajuste MSVC (`addi r3,r3,-N; b target`) alcanzables solo por vtable |
| vtable    | 50 | punteros a código almacenados en `.rdata`/data que el vtable scanner no ve |

Los punteros de vtable se obtuvieron volcando la imagen parcheada con
`cdb` (`.writemem`) y cruzando los valores de 32 bits que apuntan a
`[0x82110000, 0x824BFAC4)` contra la tabla de funciones y las etiquetas
`loc_` generadas. Herramientas en `tmp/` (`scan_vtables.py`, `scan_thunks.py`,
`describe.py`, `decode_thunks.py`).

## Estado actual

- Arranca, inicializa audio/vídeo, traduce los shaders y ejecuta código de
  invitado.
- Reproduce el intro (lecturas `NtReadFile`, `XMPGetStatus`, texturas
  1280x720).
- A los ~9 s se cae con
  `Unhandled guest access violation: read of guest 0x00000000` en el hilo
  `0xF80000A0`. La pila es
  `KeWaitForMultipleObjects_entry` ← `sub_8233F410` ← `sub_822FAEC8` ←
  `sub_8230AED8` (trampolín del hilo).
- El hilo hace `KeWaitForMultipleObjects(3, pool + 0x1190, ...)` con timeout
  infinito y uno de los tres punteros del array vale 0. El pool está en
  `0x830BC870`, el array en `0x830BDA00`.
- Con `--log_level=trace --log_noisy` aguanta >40 s sin caerse: es una carrera
  entre la creación del pool de workers y la inicialización de esos objetos.

## Cierre al entrar al menú (estado actual)

Síntoma: el juego arranca, reproduce el intro y se cierra a los ~9 s, justo
cuando el menú empieza a usar el pool de trabajos.

Traza obtenida con `cdb` (segunda excepción):

```
rexruntime!rex::system::XObject::GetNativeObject+0x7a
rexruntime!rex::kernel::xboxkrnl::KeWaitForMultipleObjects_entry+0xa1
rexruntime!HostToGuestFunction<&KeWaitForMultipleObjects_entry>
warriors_orochi_2!__imp__sub_8233F410+0x123     <- envoltorio de espera del juego
warriors_orochi_2!__imp__sub_822FAEC8+0x1ef     <- cuerpo del worker
warriors_orochi_2!__imp__sub_8230AED8+0x110     <- trampolín de hilo
rexruntime!rex::system::XThread::Execute+0x334
```

Es un AV de lectura en `0x00000000` dentro de `GetNativeObject`: la lista de
objetos que se pasa a `KeWaitForMultipleObjects` contiene un puntero nulo.

### Evidencia

- Pool de trabajos: `0x830BC870` (lo crea `sub_822FC208` con sus 4 hilos).
- El worker espera con `KeWaitForMultipleObjects(3, pool + 0x1190, WaitAny,
  timeout infinito)`; el array está en `0x830BDA00`.
- Volcado en el momento del fallo (dos ejecuciones, idéntico):

  | ranura | dirección     | valor        |
  |--------|---------------|--------------|
  | 0      | `0x830BDA00`  | `0x40060AD0` |
  | 1      | `0x830BDA04`  | `0x00000000` |
  | 2      | `0x830BDA08`  | `0x40060B10` |
  | 3      | `0x830BDA0C`  | `0xF80000A0` (handle del worker) |

- Los tres eventos los crea `sub_822FC208` en un bucle que llama a
  `sub_8233F318` (equivalente a `KeInitializeEvent`: 16 bytes del heap del
  juego, tipo/estado en +0/+4 y lista de espera auto-referenciada en +8/+12) y
  guarda el resultado en `+0x1190`, `+0x1194`, `+0x1198`.
- Sólo hay dos escritores de esa zona: ese bucle y el constructor
  `sub_82179E08`, que escribe 0 en `+0x1194`. El constructor no pudo correr
  después del bucle porque también dejaría 0 en `+0x1190`/`+0x1198`.
- El fallo es determinista: siempre la ranura del medio.

### Conclusión

Los tres eventos se crean bien: las sondas (`src/alloc_probe.cpp`) registran las
21 reservas del arranque, todas correctas, incluidos los tres de esta cola
(`0x40060AD0`, `0x40060AF0`, `0x40060B10`). Con un *breakpoint de hardware*
(`ba w4 0x1830BDA04`) se ven sólo dos escrituras en la ranura:

1. el bucle de `sub_822FC208` guardando el evento correcto (`0x40060AF0`);
2. **más tarde, código dentro del propio worker (`sub_822FAEC8`), que escribe 0**.

O sea: no hay ninguna reserva que devuelva 0; es el juego el que "suelta" el
evento y deja el puntero a cero, y después vuelve a esperar sobre las tres
ranuras. Por qué lo hace sigue abierto: el worker decompilado con Ghidra
(`0x822FAED0`) espera con `KeWaitForMultipleObjects(3, pool+0x1190, ...)` y
sólo toca los eventos con set/reset, así que la escritura debe venir de una
rama que el decompilador trunca (avisa de "subroutine does not return" en los
`memset`).

### Parche aplicado

`patches/rexglue-null-object-guard.patch` (SDK, aplicado en este workspace):

- `XObject::GetNativeObject` rechaza la dirección de invitado 0.
- `KeWaitForMultipleObjects_entry` valida cada ranura; una ranura NULL se
  sustituye por un `XEvent` privado que nunca se señaliza (mantiene los índices
  de las demás y deja que el waiter se bloquee en vez de fallar y reintentar).

Efecto: el juego ya no se cierra al entrar al menú (`>45 s` vivo, sin AV), pero
el worker sigue entrando en la espera ~1 200 veces/s (visible con `--log_noisy`),
así que la cola afectada continúa sin funcionar como en consola.

## BGM mudo y stage a ~20 fps

Ambos síntomas apuntan al mismo sitio: el pool de streaming (`0x830BC870`) con
un worker que no puede correr.

- El worker espera con `KeWaitForMultipleObjects(3, pool+0x1190, WaitAny,
  infinito)`. Ojo: el envoltorio del juego transforma `wait_type` con
  `cntlzw`+`rlwinm`, así que el `0` que pasa se convierte en **WaitAny**, no
  WaitAll. Con una ranura NULL el paseante queda inservible.
- El pool tiene 4 workers y buffers de 32 KB por índice (`memset(ptr +
  i*0x8000, 0, 0x8000)`), es decir, es el gestor de E/S en streaming: encaja
  con que sea el que alimenta el BGM (SFX y voces vienen de otras cargas).
- Sonda temporal en `KeWaitForMultipleObjects_entry` (ya retirada): la espera
  caliente es `count=2, type=1 (WaitAny), timeout infinito -> 0x0` repetida
  ~24 000 veces/s en un solo hilo. Es un livelock: un evento *manual-reset*
  queda señalizado y nunca se consume porque el worker que debería hacerlo no
  corre, así que el resto de esperas vuelven al instante y saturan la CPU
  (medido: ~90 s de CPU en 30 s de reloj, 3 núcleos).

Separación importante: los 19-20 swaps/s del stage están documentados en
`performance-investigation.md` **antes** del Title Update (hilo principal al
97.7 % de un núcleo, bucles de polling), así que el techo de ~20 fps es un
problema previo; el livelock lo empeora porque se come 3 núcleos.

### Siguiente paso

1. Confirmar si la ranura NULL es comportamiento real del juego o artefacto de
   nuestro troceado de funciones: las 50 entradas de vtable de la config son
   especulativas (se añadieron para resolver un `FATAL`). Probar una config
   mínima (sólo las 2 entradas de análisis + thunks verificados) y ver si el
   NULL desaparece; si desaparece, bisecar cuál de las 50 rompe el worker.
2. Si es real, localizar la rama que retira la cola (escribe 0 en `+0x1194`) y
   por qué corre: el `stwx` del worker escribe el puntero del pool en
   `pool + (r25+283)*16`, no un 0, así que la escritura viene de una rama que
   el decompilador trunca.
3. Rendimiento: retomar el trabajo ya documentado (polling del hilo principal,
   `sub_8231A690`/`sub_8231C9A0`) una vez que el pool deje de girar.

## Resultado de la prueba 1 (config mínima)

Se regeneró con sólo 4 entradas (2 de análisis + `0x8234B320` + `0x82351F30`):

- El **NULL sigue apareciendo**, así que no lo provoca ninguna de las 63 entradas
  especulativas (thunks + vtable): es comportamiento del juego/TU.
- La config mínima además **muere antes**: `FATAL: unregistered 0x82139008`,
  un destino indirecto que sí cubría la lista de 50 punteros de vtable. Esas
  entradas hacen falta.
- Conclusión: la config de 67 entradas se mantiene.

## Hueco encontrado en el kernel: `SignalState`

`KeSetEvent`/`KeResetEvent`/`KePulseEvent` sólo tocaban el evento del host y
**nunca actualizaban `Header.SignalState` en memoria del invitado**. El juego
inicializa ese header a mano (`sub_8233F318`) y puede leerlo directamente, así
que veía un evento siempre "no señalizado" mientras el host lo tenía señalizado
(exactamente el patrón que produce bucles de polling y retiradas de cola
inesperadas).

Corregido en el parche: `XEvent::Set/Pulse/Reset/Clear` sincronizan el header y
`XEvent::WaitCallback` lo limpia en eventos auto-reset tras un wait consumido.

No elimina el NULL observado, pero es corrección de comportamiento y se queda.

## Nota para el siguiente intento

El watchpoint sobre `0x830BDA00+4` da dos escrituras (bucle de init con el
evento correcto, y más tarde una puesta a 0), pero **bajo el depurador sólo se
reproduce la primera en 80 s**: el paseante que se queja puede ser *otra* cola
de 3 ranuras. El mensaje del guard no dice la dirección del array; el siguiente
paso barato es añadirla (`objects_ptr` del invitado) para identificar qué
waiter está roto y poner el watchpoint en su dirección.

## Estado del pool de streaming (última medición)

Con sondas de ciclo de vida (`wo2_probe_pool_ctor/start/event` en
`src/alloc_probe.cpp`) una ejecución normal muestra:

```
pool.ctor   1
pool.start  1
pool.event  1  r3=40060AD0
pool.event  2  r3=40060AF0
pool.event  3  r3=40060B10
```

Es decir: el pool se construye **una vez**, se arranca **una vez** y los tres
eventos se guardan correctamente. Sin embargo, en la espera el array aparece
como `{40060AD0, 00000000, 40060B10}` (mensaje del guard con los valores):

```
1 of 3 object slots are NULL (array 830BDA00): 40060AD0 00000000 40060B10
```

Conclusión: alguien pone a 0 la ranura **después** del arranque del pool, y sólo
a velocidad normal: con `cdb` puesto (watchpoint `ba w4 0x1830BDA04`) en 150 s
sólo se reproduce la escritura del init, nunca la puesta a 0. Es una carrera o
un camino que el depurador altera.

Siguiente herramienta recomendada: en lugar del depurador, marcar en el SDK la
página de `0x830BDA00` como sólo lectura tras el arranque del pool, capturar el
AV en el callback de memoria (registrando el RIP anfitrión → línea del
recompilado), volver a permitir la escritura y seguir. Así se ve la escritura a
velocidad real.

## Causa raíz del NULL: escritura fuera de rango del juego

Con la trampa de página de sólo lectura (`src/write_probe.cpp`) se capturaron
todas las escrituras a la página y su valor:

```
guest 830BDA04 offset +A04  rva=0x263BF6  value=00000000   (2912 veces)
guest 830BDA04 offset +A04  rva=0x263DFF  value=00000000   (25 veces)
```

Ambas instrucciones están dentro del worker. La primera, mapeada con la tabla de
líneas, es `recomp.4.cpp @ 16090` = `stw r27,4516(r8)` con
`r8 = (r30 << 4) + pool` y `r27 = 0`.

Es decir: el juego escribe `entries[i].field0 = 0` sobre un array de entradas de
16 bytes que empieza en `pool+0x11A4`, con **i = -1**:

```
pool + (-1 * 16) + 0x11A4 = pool + 0x1194
```

que es exactamente la ranura del evento del medio. De ahí el `{evento, 0,
evento}`.

El índice -1 sale de una búsqueda que recorre las entradas hasta encontrar una
con estado 4; si no encuentra ninguna devuelve -1, y la comprobación posterior
(`if (i < count - 1)`) deja pasar ese -1 cuando `count > 0`, así que escribe una
entrada antes del array. Con `count == 0` no ocurriría (toma la otra rama).

Conclusión: el NULL es un **bug de estado del juego** (índice inválido) que se
dispara porque ninguna entrada está en estado 4 cuando el worker la busca. Lo que
queda por averiguar es por qué el productor no marcó la entrada como lista.

## Estados de las entradas en el momento del write inválido

Sonda en `0x822FB100` (`entries[r30].field0 = 0`) volcando el array de entradas
(`pool+0x11A4`, 16 bytes por entrada; primer campo = estado, segundo = índice):

```
entries index=-1 count=27 states: [0]=3/0 [1]=4/1 [2]=4/2 [3]=2/3 [4]=2/4 ...
entries index=-1 count=27 states: [0]=3/0 [1]=2/1 [2]=3/2 [3]=2/3 [4]=2/4 ...
entries index=-1 count=27 states: [0]=3/0 [1]=2/1 [2]=3/2 [3]=2/3 [4]=2/4 ...
```

- `count = 27` entradas.
- Al principio hay entradas en **estado 4** (lista); después quedan en 2/3
  (consumidas/en curso) y ninguna en 4.
- El worker busca una entrada en estado 4; al no haberla el índice queda en
  `-1`, y escribe `entries[-1].field0 = 0`, que cae en `pool+0x1194` (el puntero
  del evento del medio).

Detalle a aclarar: en la primera captura el índice ya es -1 **aunque** las
entradas 1 y 2 están en estado 4. Eso sugiere que el índice no procede de esa
búsqueda en ese punto (o que la búsqueda ocurrió antes de que cambiaran los
estados), así que el siguiente instrumento debe volcar los estados **en la
búsqueda**, no en el write, y registrar de dónde sale `r30`.

## Pendiente

1. **Evento nulo del pool de trabajos** (causa del cierre al entrar al menú):
   la reserva ya está descartada; falta ver qué rama del worker escribe 0 en
   `pool+0x1194`. Pistas: el decompilado de `0x822FAED0` sólo muestra set/reset
   sobre los eventos, y la escritura aparece en una zona que Ghidra trunca por
   los `memset` marcados como "no retorna". Reproducible con:

       ba w4 0x1830BDA04

   y después `lsa .` sobre el segundo impacto (tarda ~10-20 s en salir).

   Herramientas dejadas listas:
   - `tmp/ghidra-scripts/DecompileForced.java` (decompila funciones forzando
     desensamblado y permisos de ejecución; usar con `analyzeHeadless` sobre
     `tmp/guest-image.bin`, base `0x82000000`, `PowerPC:BE:32:default`).
   - `src/alloc_probe.cpp` + los `[[midasm_hook]]` de sondeo en
     `warriors_orochi_2_config.toml` (quitar ambos para volver a un build
     limpio; escriben `alloc-probe.txt` junto al ejecutable).
2. **Hooks midasm**: las direcciones de `queue_trace`, `queue_wait`,
   `frame_stats` y `video_trace` se calcularon sobre la imagen *sin* TU. Con el
   TU el código se desplaza (p. ej. `SetInterruptCallback` 0x8231BF80 →
   0x8231C0A8), así que los hooks actuales disparan en instrucciones
   equivocadas: `wo2_queue_trace_enter` llegó a leer `[r3+0x2A90]` con `r3=3`.
   Se quitaron de la config; el respaldo con los hooks está en
   `tmp/config-backup-with-hooks.toml` y hay que re-derivarlos contra la imagen
   TU1 antes de reactivarlos.
