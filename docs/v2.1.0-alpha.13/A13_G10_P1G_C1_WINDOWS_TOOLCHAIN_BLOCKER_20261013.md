# Alpha13 — P1G-C1 bloqueo local de compilador C++ y continuidad C2

Fecha: 2026-10-13. Rama: `v2.1.0-alpha.13/feature/cleanup-robustness`.

## 1. Evidencia Windows del operador

Tras `git status --short` limpio y `git pull --ff-only` hasta
`a2fed24601b9dd67d510fa09af1162695095b060`:

```text
G10_P1G_C1_STATUS=REVIEW_STOP
G10_P1G_C1_REASON=RuntimeError('HOST_CXX_NOT_FOUND')
G10_P1G_C1_RESULTS=C:\Users\jeykc\Documentos\GitHub\platform-jwplc\tools\alpha13\results\g10_p1g_c1_20261013_000509_97cf9df1
```

Diagnóstico: el gate no encontró `clang++`, `g++`, `c++`
ni `cl` en el PATH de PowerShell. Esto no es fallo de código
del candidato ni implica que se haya ejecutado o fallado C++ nativo
en la máquina del operador. La ejecución falló **antes de compilar**.

```text
G10_P1G_C1_WINDOWS_LOCAL=REVIEW_STOP_ENV_HOST_CXX_NOT_FOUND
G10_P1G_C1_WINDOWS_LOCAL_NATIVE_COMPILE=NOT_RUN
G10_P1G_C1_PRODUCT_SOURCE_MODIFIED=NONE
G10_P1G_C1_PRECOMPILED=UNCHANGED
G10_P1G_C1_HARDWARE=NOT_TOUCHED
```

El compilador `xtensa-esp32-elf-g++` del package Arduino ESP32
es una cadena cruzada para ESP32, no puede producir un binario
host Windows ejecutable para estas pruebas. No usarlo como
sustituto del C++ nativo.

## 2. Mejora de ergonomía ya versionada

El gate `a13_g10_p1g_c1_native_prototype.py` fue ajustado para:

1. Respetar herramientas encontradas por el mecanismo previo
   en el PATH y el selector explícito `CXX`.
2. Detectar posibles instalaciones fuera de PATH de LLVM,
   MSYS2 UCRT64/MINGW64/CLANG64 y Scoop.
3. Localizar Visual Studio C++ Build Tools mediante
   `vswhere.exe` o rutas conocidas y ejecutar `vcvars64.bat`
   antes de compilar, sin exigir una sesión Developer Prompt.
4. Ofrecer modo diagnóstico `--compiler-probe` sin compilar,
   cargar firmware o generar un falso manifest PASS.
5. Mantener `REVIEW_STOP` si no existe compilador nativo
   válido; reportar error de compilación en `compile.log`
   si el binario está presente pero falla.

CI tras la mejora de ergonomía: [Windows/Linux PASS,
run 38349898868](https://github.com/JW-Control/platform-jwplc/actions/runs/38349898868).

La CI valida el camino compilable disponible en sus runners.
**No demuestra que una instalación MSVC/MSYS2 ausente o específica
de Windows del operador quede funcional**.

## 3. Reproducción local sin instalaciones automáticas

```powershell
git status --short
git pull --ff-only origin v2.1.0-alpha.13/feature/cleanup-robustness
python -B tools\alpha13\gates\a13_g10_p1g_c1_native_prototype.py --compiler-probe
```

Si `COMPILER_PROBE=FOUND` ejecutar después el BAT C1.
Si `COMPILER_PROBE=NOT_FOUND`, conservar bloqueo local
`ENV_HOST_CXX_NOT_FOUND`; NO repetir BAT hasta instalar
o habilitar manualmente un compilador C++ host.

Alternativas voluntarias: instalar MSVC Build Tools con
componentes C++ x64 o disponer de MinGW-w64 (p. ej., MSYS2).
No realizar instalaciones ni descargas por sorpresa.

## 4. Decisión de continuidad

```text
G10_P1G_C1_CI=PASS_WINDOWS_LINUX_RUN_38349405593
G10_P1G_C1_CI_ERGONOMICS=PASS_WINDOWS_LINUX_RUN_38349898868
G10_P1G_C1_LOCAL_NATIVE=NOT_PROVEN
G10_P1G_C2_DESIGN_WORK=ALLOWED_WITH_CI_PROVENANCE
G10_P1G_C2_NATIVE_ACTUAL_PRODUCT_METHODS=REQUIRED_BEFORE_ADOPTION
G10_P1G_C2_ARDUINO_BUILDS=REQUIRED_BEFORE_ADOPTION
G10_P1G_C2_PHYSICAL=SEPARATE_AUTHORIZATION
G10_P1G_GPIO4_DEFAULT=UNMODIFIED_255
A13-006=ADOPTION_BLOCKED
G10_FINAL=OPEN
```

Es admisible avanzar a diseño e integración en overlay C2
apoyándose en CI nativa C1. No es admisible declarar una
prueba local C1 PASS ni adoptar el candidato productivo.

El candidato C1 sigue solo bajo
`tools/alpha13/candidates/g10_p1g_c/`: no está
integrado en las bibliotecas Arduino de producto.
