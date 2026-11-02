<p align="center">
  <img src="docs/RECOMPILED.png" alt="Warriors Orochi 2 — Recompiled" width="820">
</p>

# Warriors Orochi 2 (USA) — recompilación con ReXGlue

Proyecto de recompilación estática para **Warriors Orochi 2 (USA) (En,Fr,De)** de
Xbox 360, construido contra la imagen parcheada con la **Title Update #1**.

Estado actual: arranca, audio correcto (BGM, SFX y voces), vídeo WMV visible,
~60 FPS en menús y ~25-35 FPS en batalla en una iGPU AMD (Ryzen 5 4500U). Los
detalles de la investigación de rendimiento están en
[performance-investigation.md](performance-investigation.md) y el proceso de
puesta en marcha con la TU en [tu1-bringup.md](tu1-bringup.md).

---

## Estructura esperada de la carpeta de ReXGlue

```
rexglue-sdk\
├─ .git\                                  # SDK de ReXGlue (checkout de git)
├─ cmake\
├─ docs\
├─ include\                               # cabeceras del SDK
├─ resources\
├─ scripts\
├─ src\                                   # fuentes del SDK (aquí aplican los parches)
├─ tests\
├─ thirdparty\
├─ tools\
├─ out\                                   # salida de compilación del SDK
│  └─ win-amd64\
│     ├─ rexglue.exe                      # herramienta de codegen
│     ├─ rexgpu-xenos.dll                 # plugin gráfico (D3D12)
│     └─ rexruntime.dll
├─ Warriors Orochi 2 (USA) (En,Fr,De)\    # datos del juego (NO va en git)
│  ├─ default.xex                         # ejecutable base (retail)
│  ├─ default.xexp                        # TU #1 instalada por build.ps1
│  ├─ data\                               # LINKDATA_*, movie\, ...
│  ├─ $SystemUpdate\
│  └─ Warriors Orochi 2 Title Update #1\  # generada por build.ps1
│     ├─ default.xex                      # copia del ejecutable base
│     ├─ default.xexp                     # TU #1 extraída del contenedor
│     └─ TU_15KU1V3_..._0000000000083     # contenedor original (copiado)
├─ wo2-recomp\                            # ESTE proyecto
│  ├─ warriors_orochi_2_manifest.toml     # entrada/salida del codegen
│  ├─ warriors_orochi_2_config.toml       # funciones y hooks verificados
│  ├─ CMakeLists.txt
│  ├─ CMakePresets.json
│  ├─ build.ps1                           # parches + TU + codegen + build
│  ├─ run.ps1                             # build si hace falta y ejecutar
│  ├─ build.bat / run.bat / build.sh / run.sh
│  ├─ trace-stage.ps1 / profile-stage.ps1 # trazas opcionales
│  ├─ docs\
│  │  └─ RECOMPILED.png                   # cabecera del README
│  ├─ patches\                            # parches del SDK (ver patches\README.md)
│  │  ├─ sdk-patches.txt                  # los que aplica build.ps1
│  │  ├─ rexglue-sdk-wo2.patch            # todos los cambios del SDK
│  │  ├─ rexglue-wo2-draw-gpu-timings.patch  # diagnóstico opcional
│  │  └─ history\                         # parches granulares originales
│  ├─ src\                                # código propio del proyecto
│  │  ├─ main.cpp                         # app ReXGlue
│  │  ├─ prepare_tu.cpp                   # extrae e instala la TU #1
│  │  ├─ event_diagnostics.cpp            # auto-test de eventos
│  │  ├─ frame_stats.cpp                  # contador de FPS del invitado
│  │  ├─ queue_trace.cpp / queue_wait.cpp # instrumentación de la cola
│  │  └─ video_trace.cpp                  # trazas de vídeo/GPU
│  ├─ generated\                          # generado por codegen (no editar)
│  │  ├─ rexglue.cmake
│  │  └─ default\
│  │     ├─ warriors_orochi_2_init.cpp
│  │     ├─ warriors_orochi_2_register.cpp
│  │     ├─ warriors_orochi_2_recomp.*.cpp
│  │     └─ sources.cmake
│  └─ out\build\win-amd64-release\        # binarios finales
│     ├─ warriors_orochi_2.exe
│     ├─ rexgpu-xenos.dll
│     ├─ rexruntime.dll
│     ├─ warriors_orochi_2.toml           # configuración de ejecución
│     └─ logs\
└─ CMakeLists.txt
```

## Requisitos

- Windows 10/11 x64.
- **Visual Studio 2022** con la carga de trabajo *Desarrollo para el escritorio
  con C++* (incluye clang/LLVM, CMake y Ninja).
- **Git** en el PATH: se usa para aplicar y detectar los parches del SDK.
- Los **datos del juego** en `Warriors Orochi 2 (USA) (En,Fr,De)\` (mismo nivel
  que `wo2-recomp\`), con `default.xex` y la carpeta `data\`.
- El **contenedor de la Title Update #1** que corresponda a tu disco. Ojo: hay
  dos TU #1 para este juego, con Media ID distinto (`4454AE3B` y `3D32F1BD`), y
  cada una parchea solo el ejecutable base de su prensa. Con el dump de este
  proyecto aplica la `4454AE3B`; la otra se rechaza (ver *Notas*).

## Puesta en marcha

```powershell
cd rexglue-sdk\wo2-recomp

# Compila todo: aplica los parches del SDK, instala la TU #1, genera el código
# y compila. Deja los binarios listos. Usa la TU del Media ID que corresponda a
# tu disco (con el dump de este proyecto, 4454AE3B).
.\run.ps1 -UpdateFile "D:\Warriors Orochi 2 Title Update #1\Title Update 1 - Media ID 4454AE3B\TU_15KU1V3_000000C000000.0000000000083"
```

También se le puede pasar directamente la carpeta que contiene el contenedor:

```powershell
.\run.ps1 -UpdateFile "D:\Warriors Orochi 2 Title Update #1"
```

Opciones útiles:

| Comando | Efecto |
|---------|--------|
| `.\run.ps1` | Solo ejecuta el binario ya compilado |
| `.\run.ps1 -UpdateFile ... -NoRun` | Compila y no ejecuta |
| `.\run.ps1 -Build` | Fuerza recompilación antes de ejecutar |
| `.\build.ps1 -UpdateFile ...` | Solo compila |
| `.\build.ps1 -Prepared` | Compila usando la TU ya preparada en el disco |

`run.ps1` pasa al juego estos valores por defecto (se pueden sobrescribir
añadiendo argumentos al final):

```
--game_data_root="<ruta>\Warriors Orochi 2 (USA) (En,Fr,De)"
--gpu_plugin=xenos --render_target_path_d3d12=rov --no-fullscreen
--resolution=720p --video_mode_refresh_rate=60 --no-vsync
--mnk_mode --input_backend=xinput
```

Teclas: **F3** muestra/oculta el contador de FPS del juego. En modo teclado/ratón
(mnk_mode) los controles son los del mando emulado: `WASD` stick izquierdo,
flechas stick derecho, `Espacio`/`;` botón A, `Retroceso` botón B, `X`/`Enter`
Start.

## Qué hace `build.ps1` paso a paso

1. Localiza Visual Studio (vswhere) y prepara el entorno (clang, CMake, Ninja).
2. Aplica los parches del SDK listados en `patches\sdk-patches.txt`; si ya están
   aplicados los detecta y continúa.
3. Configura CMake con `-DREXSDK_DIR=<rexglue-sdk>` y `-DWO2_TU_PACKAGE=<TU>`.
4. Construye el target de codegen, que antes ejecuta `wo2_prepare_tu`:
   - extrae `default.xexp` del contenedor STFS,
   - crea `Warriors Orochi 2 Title Update #1\` con una copia del `default.xex` y
     la `default.xexp`,
   - copia la `default.xexp` junto al `default.xex` base, porque el runtime solo
     aplica un parche que encuentre como `<módulo>.xexp` al lado del ejecutable.
5. Ejecuta `rexglue codegen` con el manifest y la configuración.
6. Compila el SDK, el código recompilado y el ejecutable final.

## Qué se versiona y qué no

`.gitignore` deja fuera todo lo que se regenera o es local:

| Ruta | Motivo |
|------|--------|
| `out/` | árbol de CMake/Ninja, binarios, TOML de ejecución y logs |
| `generated/**` | ~70 MB de código producido por `rexglue codegen` (se regenera) |
| `tmp/` | TU extraídas, diagnósticos, copias de configuración, capturas |
| `*.etl`, `*.csv`, `*.png`, `*.log` | trazas, perfiles y capturas de las investigaciones |
| `*.toml.bak`, `warriors_orochi_2.toml` | copias locales y la configuración que escribe el runtime |

**Excepción importante:** `generated/rexglue.cmake` sí se versiona. Es el fichero
que el `CMakeLists.txt` incluye en la fase de configuración (localiza el SDK y
aplica los ajustes de recompilación); sin él un checkout limpio no puede ni
configurarse. El resto de `generated/` (incluido
`generated/default/sources.cmake`) no se versiona: `sources.cmake` solo se
incluye cuando existe, y los `.cpp` los produce codegen antes de compilar.

Los datos del juego (`Warriors Orochi 2 (USA) (En,Fr,De)/`) viven dentro del
checkout del SDK pero no son código. Al estar sin trackear, basta con añadirlos a
`.git/info/exclude` del checkout del SDK para que no ensucien `git status` (es lo
que está hecho en este proyecto; no se toca el `.gitignore` del SDK para no
alterar el repositorio ajeno).


## Notas

- La Title Update es **obligatoria**: el código recompilado corresponde a la
  imagen parcheada (`XEX patch applied successfully`, versión 0.0.1.3) y el
  runtime debe arrancar esa misma imagen.
- El código de `generated\` no se edita a mano; cualquier cambio va en
  `warriors_orochi_2_config.toml` y se regenera.
- Los cambios al SDK **no** se hacen en el checkout: van en
  `patches\rexglue-sdk-wo2.patch`, que es lo que aplica `build.ps1`.
- **Cada Title Update solo aplica a su disco.** Si codegen falla con
  `XEX patch signature hash doesn't match base XEX signature hash` y
  `XEX patch image key doesn't match original XEX`, has pasado la TU de
  otra prensa del juego (otro Media ID): usa la que corresponda a tu
  imagen. Con el dump de este proyecto aplica la de Media ID `4454AE3B`;
  la de Media ID `3D32F1BD` está pensada para otro ejecutable base y se
  rechaza (verificado).
- Logs de ejecución en `out\build\win-amd64-release\logs\` (rotan cada 5 MB).
