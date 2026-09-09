!include "WinMessages.nsh"

; Alpha11 instala por usuario y conserva la ruta ya validada por el instalador
; técnico: %LOCALAPPDATA%\JWPLC\HMI Designer.
!macro preInit
  SetRegView 64
  WriteRegExpandStr HKCU "${INSTALL_REGISTRY_KEY}" InstallLocation "$LOCALAPPDATA\JWPLC\HMI Designer"
  SetRegView 32
  WriteRegExpandStr HKCU "${INSTALL_REGISTRY_KEY}" InstallLocation "$LOCALAPPDATA\JWPLC\HMI Designer"
!macroend

!macro customInstall
  ; Migración desde el instalador técnico Alpha11 anterior. El Setup NSIS usa
  ; resources\poc y su propio ejecutable/desinstalador, por lo que estos
  ; archivos raíz ya no son necesarios.
  RMDir /r "$INSTDIR\poc"
  Delete "$INSTDIR\Start-JWPLC-HMI-Designer.ps1"
  Delete "$INSTDIR\JWPLC-HMI-Server.ps1"
  Delete "$INSTDIR\JWPLC-HMI-Designer.cmd"
  Delete "$INSTDIR\JWPLC-HMI-Designer.ico"

  ; Extensión para Arduino IDE 2 incluida dentro del Setup.
  CreateDirectory "$PROFILE\.arduinoIDE\plugins"
  Delete "$PROFILE\.arduinoIDE\plugins\jwplc-hmi-launcher-*.vsix"
  CopyFiles /SILENT "$INSTDIR\resources\installer\jwplc-hmi-launcher-*.vsix" "$PROFILE\.arduinoIDE\plugins"

  ; API estable usada por la extensión para localizar el Designer instalado.
  WriteRegExpandStr HKCU "Environment" "JWPLC_HMI_DESIGNER_HOME" "$INSTDIR"

  ; Protocolo de fallback: jwplc-hmi://open
  WriteRegStr HKCU "Software\Classes\jwplc-hmi" "" "URL:JWPLC HMI Designer"
  WriteRegStr HKCU "Software\Classes\jwplc-hmi" "URL Protocol" ""
  WriteRegStr HKCU "Software\Classes\jwplc-hmi\shell\open\command" "" "$\"$INSTDIR\JWPLC-HMI-Designer.exe$\" $\"%1$\""

  ; Propagar la variable a nuevas aplicaciones iniciadas desde Explorer.
  SendMessage ${HWND_BROADCAST} ${WM_SETTINGCHANGE} 0 "STR:Environment" /TIMEOUT=5000
!macroend

!macro customUnInstall
  ; La extensión forma parte del producto: el desinstalador la retira también.
  Delete "$PROFILE\.arduinoIDE\plugins\jwplc-hmi-launcher-*.vsix"

  DeleteRegValue HKCU "Environment" "JWPLC_HMI_DESIGNER_HOME"
  DeleteRegKey HKCU "Software\Classes\jwplc-hmi"
  SendMessage ${HWND_BROADCAST} ${WM_SETTINGCHANGE} 0 "STR:Environment" /TIMEOUT=5000
!macroend
