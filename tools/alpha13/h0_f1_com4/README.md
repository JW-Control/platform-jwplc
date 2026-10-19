# JWPLC Basic Alpha13 — H0-F1 COM4 (kit versionado)

**Esta es la única vía de entrega del kit: repositorio Git, nunca ZIP.**
**Estado:** pendiente de prueba física por el operador; CI no equivale a PASS de H0.
**Firmware:** package real JWPLC/2.1.0, con F1 integrado en PR #116.
**Seguridad:** solo banco de desarrollo COM4 (Master); COM14 intocado; R1–R4 bloqueados; sin GPIO4 ni cambios de perifericos.

## 1. Traer todo con Git

Desde PowerShell, en el repositorio platform-jwplc existente:

~~~powershell
git fetch origin
git switch v2.1.0-alpha.13/feature/cleanup-robustness
git pull --ff-only
git status --short
$head = (git rev-parse HEAD).Trim()
& .\tools\alpha13\h0_f1_com4\A13_H0_F1_COM4_BANCO.ps1 -ExpectedHead $head
~~~

La ejecución anterior **solo compila y NO abre COM4**. El script exige rama y SHA exactos,
worktree limpio y merge de F1 presente. Instala una copia aislada del package en %TEMP%,
compila el smoke sketch directamente desde ella y verifica biblioteca Ethernet,
archivo de dependencias .d, objeto w5100.cpp.o, ELF y hashes. El manifiesto
se guarda fuera del repositorio.

Requisitos: Windows PowerShell 5.1 o superior, Arduino CLI 1.5.1 y la toolchain
de ESP32 de JWPLC instalada (en CI se usa jwplc:esp32@2.1.0-alpha.10).
Esperado: H0F1_BUILD=PASS_SOURCE_FIRST, H0F1_UPLOAD=NOT_RUN.

Si Arduino CLI escribe `runtime.platform.path` con barras `/`, `\\`, comillas, una barra final o diferente capitalización de Windows, el ejecutor normaliza ambas rutas antes de compararlas. **No acepta una plataforma diferente**: si realmente difieren, imprime `H0F1_PLATFORM_SELECTED` y `H0F1_PLATFORM_EXPECTED`, deja `H0F1_REVIEW_STOP` y conserva `SHOW_PROPERTIES_LOCAL.log` en `%TEMP%`. La salida `H0F1_PLATFORM=PASS_ISOLATED_PACKAGE_PATH_CANONICAL` confirma la procedencia exacta antes de compilar.

## 2. Subir a COM4 (solamente despues del build)

El operador debe confirmar que **COM4** es su equipo JWPLC de desarrollo,
sin cargas ni tensiones en relés Q, sin maquinaria ni LAN industrial, con
fuente segura y monitor serial cerrado. El backup del firmware anterior
fue renunciado expresamente para estas placas de laboratorio; la renuncia
**no** aplica a clientes, otros equipos o campañas futuras.

~~~powershell
$head = (git rev-parse HEAD).Trim()
& .\tools\alpha13\h0_f1_com4\A13_H0_F1_COM4_BANCO.ps1 -ExpectedHead $head -UploadCOM4 -ExpectedMac 'XX:XX:XX:XX:XX:XX'
~~~

Reemplazar solo en la consola local la MAC por la que ya conoce de su COM4;
no pegar la MAC en GitHub. El script vuelve a compilar para asegurar
el fuente real, verifica CH340 VID/PID 1A86:7523, MAC y flash 4MB,
y pregunta de forma interactiva exactamente:

    H0F1 AUTORIZO SOLO COM4 PACKAGE CORREGIDO SIN BACKUP

Sin consentimiento no carga nada. La unica llamada de upload tiene puerto COM4
fijo. COM14 no se abre en ningún modo. El manifiesto queda bajo %TEMP%.

## 3. Observación UART

Después del upload, abrir monitor serie COM4 a **115200**.
Se requiere en un mismo boot:

    H0F1_BOOT=<id>
    H0F1_EARLY_LINK_BEGIN
    H0F1_EARLY_LINK_RETURN=<0|1|2>
    H0F1_READY:BOOT=<id> COUNT=<creciente> STAGE=2 LINK=<0|1|2>

0=Unknown, 1=LinkON, 2=LinkOFF. Unknown persistente NO confirma el PHY.
LINK=-9 de H0-F nunca es lectura real. El criterio mínimo temprano es
retorno de linkStatus() sin watchdog TG1WDT_SYS_RESET ni reinicio espontáneo.
Si se observa REVIEW_STOP_PREVIOUS_EARLY_LINK_NOT_RETURNED, detenerse
y no repetir automáticamente la sonda.

Observaciones de 15 minutos, desconexión/reconexión manual RJ45, resets EN
y ciclos de alimentación requieren decisión separada del operador.
Ninguno se automatiza en el ejecutor. R1–R4 todavía NO autorizados.

Guardar MANIFEST.json, logs de compilación y los mensajes relevantes UART;
**no publicar** datos MAC/PnP, flash cruda ni archivos de evidencia privada.
Informar H0F1_BUILD, H0F1_UPLOAD, EARLY_LINK, READY/STAGE, resets y PHY.
