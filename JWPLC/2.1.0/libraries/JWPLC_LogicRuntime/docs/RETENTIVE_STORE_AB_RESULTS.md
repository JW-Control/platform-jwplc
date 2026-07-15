# Resultado de validación — store retentivo A/B simulado

## Estado

```text
VALIDADO
Resultado: 38 PASS, 0 FAIL
STORE RETENTIVO A/B SIMULADO: PASS
```

## Compilación

```text
Flash:       423713 bytes / 3145728 bytes (13 %)
RAM global:   32524 bytes / 327680 bytes (9 %)
RAM restante: 295156 bytes
```

## Parámetros comprobados

```text
Región retentiva 8 KiB: 0x1440..0x1A3F
Tamaño total:            1536 bytes
Tamaño por copia:         768 bytes
Payload por copia:        704 bytes
Bitmap máximo v1:          50 bytes para 400 bloques
```

## Flujo validado

1. La región nueva inicia sin snapshot válido.
2. El snapshot A se guarda en copia 0 con secuencia 1.
3. La identidad completa y el bitmap A se recargan correctamente.
4. Se rechazan buffers insuficientes.
5. Se rechazan Program ID, generación y cantidad de bloques diferentes.
6. El snapshot B se guarda en copia 1 con secuencia 2.
7. La nueva identidad carga B y la identidad anterior todavía permite cargar A.
8. Al corromper la copia B, un reinicio descarta B y recupera A.
9. Se probaron los 66 posibles cortes parciales de una actualización completa.
10. Todos los cortes anteriores al commit conservaron A.
11. Con exactamente 66 bytes disponibles, B quedó confirmado y arrancable.

## Propiedades confirmadas

- Cada copia es autocontenida.
- La copia nueva se invalida antes de escribir su contenido.
- La firma `JWRT` se escribe al final como commit.
- La copia anterior permanece válida durante toda la actualización.
- La selección usa la secuencia válida más reciente.
- La identidad es estricta: Program ID, generación, cantidad de bloques y tamaño de bitmap.
- Un snapshot perteneciente a otro programa nunca se aplica por accidente.
- El formato admite holgadamente los 400 bloques físicos del perfil futuro.

## Seguridad de la prueba

La prueba usó `LogicMemoryStorage`:

- no inicializó E/S;
- no accedió a la FRAM física;
- no conmutó salidas.

## Decisión

El diseño transaccional queda aprobado para pasar a una prueba física reversible sobre la región retentiva real del JWPLC Basic.