# Parches del SDK

Todo lo que el proyecto necesita del SDK de ReXGlue vive aquí, como parches de
git que se aplican sobre un checkout limpio.

## Lo que se aplica automáticamente

`build.ps1` lee `sdk-patches.txt` y aplica, en orden, los parches que aparecen
listados (las líneas que empiezan por `#` se ignoran). Es idempotente: si un
parche ya está aplicado lo detecta y no hace nada.

| Parche | Contenido |
|--------|-----------|
| `rexglue-sdk-wo2.patch` | Todos los cambios locales del SDK para Warriors Orochi 2. Es el parche canónico. |

`rexglue-sdk-wo2.patch` incluye, en un solo diff:

- **`xobject.cpp` / `xobject.h`**: `XObject::GetNativeObject` rechaza el objeto
  huésped en la dirección 0 y añade el gancho virtual `SyncFromGuest`. Antes, un
  puntero nulo se traducía a una dirección de host válida y el proceso moría
  leyendo la cabecera de despacho en la dirección 0.
- **`xevent.cpp` / `xevent.h`**: sincronización bidireccional del estado de los
  KEVENT. El juego borra y lee `SignalState` con stores directos
  (`sub_8233F3A8`), así que el evento del host se reconciliaba mal: se conserva
  la dirección del objeto huésped, se serializan las actualizaciones y se
  reconcilia en cada búsqueda del objeto nativo. Esto es lo que arregla el BGM
  que se quedaba mudo al entrar al menú.
- **`xboxkrnl_threading.cpp`**: guarda en `KeWaitForMultipleObjects` para ranuras
  NULAS (con un evento de relleno que nunca se señaliza) y mensaje de aviso con
  los valores del array.
- **`command_processor.cpp` (xenos y d3d12)**: instrumentación opcional de trazas
  (cvar `wo2_queue_trace` / `wo2_queue_latency_trace` y `wo2_video_gpu_trace`).
  Desactivada por defecto; es la que usan `trace-stage.ps1` y los analizadores de
  `performance-investigation.md`.

Aplicar / revertir a mano:

    cd <rexglue-sdk>
    git apply wo2-recomp/patches/rexglue-sdk-wo2.patch        # aplicar
    git apply -R wo2-recomp/patches/rexglue-sdk-wo2.patch     # revertir

## Lo que NO se aplica automáticamente

| Parche | Uso |
|--------|-----|
| `rexglue-wo2-draw-gpu-timings.patch` | Diagnóstico opcional: mide el tiempo de GPU por draw con queries de timestamp y registra `[wo2-draw]` / `[wo2-heavy]` (hash de shader y máscara de texturas). Añade además la cvar `wo2_skip_empty_fullscreen_draws`. Se aplica a mano solo cuando se quiere volver a medir. |

## Historial

`history/` guarda la versión granular original de los parches (guarda de NULL,
sincronización de eventos, trazas de cola/vídeo/latencia). Se conservan como
referencia; no se aplican y pueden no aplicar sueltos sobre HEAD, porque
`rexglue-sdk-wo2.patch` ya contiene su resultado final.

## Sobre el código generado

El código recompilado (`wo2-recomp/generated/`) **no se parchea**: se regenera
con `rexglue codegen` a partir de:

- `warriors_orochi_2_manifest.toml` (imagen de entrada, salida, includes), y
- `warriors_orochi_2_config.toml` (67 funciones verificadas contra la imagen con
  TU #1 —destinos de análisis, thunks de ajuste y punteros de vtable— más el
  gancho `wo2_count_frame` en el `bl VdSwap` de la imagen parcheada).

Por eso `build.ps1` siempre ejecuta codegen antes de compilar: el resultado es
reproducible a partir de esos dos ficheros y del XEX con la Title Update.
