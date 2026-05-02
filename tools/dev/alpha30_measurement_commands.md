# Alpha30 - Comandos de medición

## 1. Verificar Arduino CLI

```powershell
arduino-cli version
```

## 2. Primera compilación limpia

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\dev\alpha30_measure_build.ps1 -Clean
```

Registrar tiempo.

## 3. Segunda compilación sin cambios

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\dev\alpha30_measure_build.ps1
```

Registrar tiempo.

## 4. Compilación + subida

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\dev\alpha30_measure_build.ps1 -Upload -Port COM14
```

Cambiar `COM14` si corresponde.

## 5. Compilación + subida limpia

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\dev\alpha30_measure_build.ps1 -Clean -Upload -Port COM14
```

## 6. Medición de otro sketch

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\dev\alpha30_measure_build.ps1 `
  -Sketch "C:\ruta\al\sketch" `
  -Upload `
  -Port COM14
```

## 7. Datos a registrar

| Prueba | Tiempo |
|---|---:|
| Build limpio | |
| Build sin cambios | |
| Build + upload sin cambios | |
| Cambio mínimo en sketch | |
| Cambio en core | |
| Cambio en librería interna | |
