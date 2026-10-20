# Alpha13 G10 PH0 — banco de dos JWPLC conectados al router

Estado: preparación y compilación sin hardware. Aprobado preparar COM4 Master + COM14 Slave, pero todavía NO se ha concedido consentimiento de sobrescritura/backup por placa. PR #124 sigue Draft; F1 CS GPIO5 queda intacta.

## Topología de banco

    Router/switch de laboratorio (DHCP activo, no planta)
          | Ethernet RJ45             | Ethernet RJ45
    JWPLC COM4 Master                 JWPLC COM14 Slave
          |___________ RS-485 A/B ____________|
                  PC en misma LAN para FC03 TCP

No confundir RJ45 con el bus RS-485. Cada PLC tiene USB serial independiente y una dirección IP propia. COM14 no tiene microSD; el sketch no la accede.

- COM4: G10P_COM4_Master. Ethernet DHCP, Modbus TCP server (Unit 1 / TCP 502), UDP eco puerto 15020, RTU master 115200 8N1 que lee cuatro holding del Slave ID 2. Periféricos autoload normales, SD/FRAM solo lectura y Q sin tocar.
- COM14: G10P_COM14_Slave. Ethernet DHCP, Modbus TCP server (Unit 1 / TCP 502), RTU slave ID 2 / 115200 / 8N1. Holding RTU separado de holding TCP para no crear carreras entre protocolos.

## Gates y orden

1. CI dual Ubuntu/Windows build-only: compilar los dos sketches con FQBN aislado jwplc_g10ph0:esp32:jwplcbasic; comprobar plataforma fuente, dependencias GCC .d, objeto .o, ELF y SHA. Ningún USB ni upload.
2. Gate físico PH0 con autorización explícita posterior. Confirmar ambos PLC de desarrollo, sin cargas/tensiones externas Q, dos puertos/roles y MACs contra registro previo independiente, 4 MB flash por PLC, red aislada, consentimiento de pérdida/backup por separado. No colocar MACs en GitHub ni usar una placa como identidad de la otra.
3. Tras autorización y upload validado, comprobar COM14 con etiquetas G10P_COM14_BOOT/HEARTBEAT y COM4 con etiquetas G10P_COM4_BOOT/HEARTBEAT, 115200. Ambos deben progresar sin WDT/reset imprevisto.
4. Conectar AMBOS al mismo router/switch aislado y observar PHY=1, ETH=1, TCP=1 e IPs LAN diferentes (sin conflictos DHCP) en COM4 y COM14. PHY=2 inicial puede significar negociación. PHY=0 persistente o watchdog significa parar.
5. Desde PC en la misma LAN, ejecutar exclusivamente la consulta FC03 de lectura a las dos IP ya observadas. El script usa Modbus TCP 502/Unit ID 1/holding 0..3 y no escribe registros, firmware ni flash. Valores esperados COM4: 1234,2345,3456,4567 hex; COM14: 5678,6789,789A,89AB hex.

    py -3 tools/alpha13/g10_promoted_bench/g10p_modbus_tcp_readonly.py --com4-ip 192.168.X.Y --com14-ip 192.168.X.Z

6. Confirmar por UART que COM4 RTU_OK crece y COM14 RTU_REQUESTS crece mientras RS-485 A/P, B/N y tierra/terminación permanecen como en banco validado. Este gate comprueba coexistencia TCP y RTU, no comunicación TCP directa entre PLCs.
7. Una prueba opcional posterior COM4→COM14 por Modbus TCP y las campañas físicas R1–R4 requieren sus propios criterios/autorizaciones. No reutilizar el sketch experimental g10_r_integrated.ino con sentinela LINK=-9, hooks RTC y funciones privadas.

## Comandos build-only locales (cuando CI esté verde)

En una rama limpia del PR #124:

    $head = (git rev-parse HEAD).Trim()
    $out = Join-Path $env:TEMP ("g10p_dual_" + $head.Substring(0,12) + "_" + (Get-Date -Format "yyyyMMddHHmmss"))
    py -3 tools/alpha13/g10_promoted_bench/g10p_dual_build.py --expected-head $head --output $out

El comando no abre COM4/COM14. Deja manifiesto SHA y logs en %TEMP%; no publica ELF/binarios. El uploader dual es un flujo separado, aún pendiente de revisión y consentimiento.

## Estado

    G10_PH0_SCOPE=COM4_AND_COM14_APPROVED_FOR_PREPARATION
    G10_PH0_DUAL_ETHERNET_ROUTER=PLANNED_NOT_RUN
    G10_PH0_BUILD_BOTH=CI_PENDING_OR_COMPLETED_REVIEW
    G10_PH0_UPLOAD=NOT_AUTHORIZED_NOT_RUN
    G10_PH0_TCP_FC03=NOT_RUN
    G10_PH0_RTU_MASTER_SLAVE=NOT_RUN
    G10_R_R1_R2_R3_R4=NOT_RUN_NOT_AUTHORIZED
    G10_ALPHA13_BASE_MERGE=NOT_RUN
