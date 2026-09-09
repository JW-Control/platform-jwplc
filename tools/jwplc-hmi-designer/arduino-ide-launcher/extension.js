'use strict';

const vscode = require('vscode');
const fs = require('fs');
const path = require('path');
const childProcess = require('child_process');

const DESIGNER_URI = 'jwplc-hmi://open';

function designerRoots() {
  const roots = [];
  if (process.env.JWPLC_HMI_DESIGNER_HOME) {
    roots.push(process.env.JWPLC_HMI_DESIGNER_HOME);
  }
  if (process.env.LOCALAPPDATA) {
    roots.push(path.join(process.env.LOCALAPPDATA, 'JWPLC', 'HMI Designer'));
  }
  return [...new Set(roots.filter(Boolean))];
}

function findDesignerExe() {
  for (const root of designerRoots()) {
    const candidate = path.join(root, 'JWPLC-HMI-Designer.exe');
    if (fs.existsSync(candidate)) return candidate;
  }
  return null;
}

function cleanElectronChildEnv() {
  const env = { ...process.env };

  // Arduino IDE 2/Theia ejecuta extensiones dentro de un host Electron.
  // Ese host puede exportar ELECTRON_RUN_AS_NODE=1. Si se hereda al lanzar
  // nuestro ejecutable Electron, JWPLC HMI Designer arranca como proceso Node
  // y no crea BrowserWindow aunque spawn/Start-Process reporten éxito.
  delete env.ELECTRON_RUN_AS_NODE;

  return env;
}

async function openByProtocol() {
  try {
    return await vscode.env.openExternal(vscode.Uri.parse(DESIGNER_URI));
  } catch (_) {
    return false;
  }
}

function psQuote(value) {
  return String(value).replace(/'/g, "''");
}

function launchViaWindowsShell(exe) {
  return new Promise((resolve) => {
    const workDir = path.dirname(exe);
    const command =
      `Remove-Item Env:ELECTRON_RUN_AS_NODE -ErrorAction SilentlyContinue; ` +
      `Start-Process -FilePath '${psQuote(exe)}' ` +
      `-WorkingDirectory '${psQuote(workDir)}'`;

    let settled = false;
    const finish = (result) => {
      if (settled) return;
      settled = true;
      resolve(result);
    };

    try {
      const child = childProcess.spawn(
        'powershell.exe',
        [
          '-NoProfile',
          '-NonInteractive',
          '-ExecutionPolicy',
          'Bypass',
          '-WindowStyle',
          'Hidden',
          '-Command',
          command
        ],
        {
          cwd: workDir,
          detached: true,
          stdio: 'ignore',
          windowsHide: true,
          env: cleanElectronChildEnv()
        }
      );

      child.once('spawn', () => {
        child.unref();
        finish({ ok: true });
      });
      child.once('error', (error) => finish({ ok: false, error }));
    } catch (error) {
      finish({ ok: false, error });
    }
  });
}

function launchDirect(exe) {
  return new Promise((resolve) => {
    let settled = false;
    const finish = (result) => {
      if (settled) return;
      settled = true;
      resolve(result);
    };

    try {
      const child = childProcess.spawn(exe, [], {
        cwd: path.dirname(exe),
        detached: true,
        stdio: 'ignore',
        windowsHide: false,
        env: cleanElectronChildEnv()
      });

      child.once('spawn', () => {
        child.unref();
        finish({ ok: true });
      });
      child.once('error', (error) => finish({ ok: false, error }));
    } catch (error) {
      finish({ ok: false, error });
    }
  });
}

async function launchDesigner() {
  vscode.window.setStatusBarMessage('JW HMI: abriendo Designer…', 1800);

  const exe = findDesignerExe();
  if (exe) {
    let result;

    // Primero intentamos directamente con un entorno saneado. Es la ruta más
    // determinista para una app Electron lanzada desde otro host Electron.
    result = await launchDirect(exe);
    if (result.ok) {
      vscode.window.setStatusBarMessage('JW HMI: Designer iniciado', 1800);
      return;
    }

    // Fallback Windows Shell, también con ELECTRON_RUN_AS_NODE eliminado.
    if (process.platform === 'win32') {
      result = await launchViaWindowsShell(exe);
      if (result.ok) {
        vscode.window.setStatusBarMessage('JW HMI: Designer iniciado', 1800);
        return;
      }
    }
  }

  const opened = await openByProtocol();
  if (!opened) {
    vscode.window.showErrorMessage(
      'JWPLC HMI Designer no está instalado o Windows no pudo iniciarlo. Reinstala la aplicación y reinicia Arduino IDE.'
    );
  }
}

function enumCompletion(label, detail, index) {
  const item = new vscode.CompletionItem(label, vscode.CompletionItemKind.EnumMember);
  item.insertText = label;
  item.detail = detail;
  item.sortText = String(index).padStart(2, '0');
  return item;
}

function methodCompletion(label, insertText, detail, index) {
  const item = new vscode.CompletionItem(label, vscode.CompletionItemKind.Method);
  item.insertText = new vscode.SnippetString(insertText);
  item.detail = detail;
  item.sortText = `00-${String(index).padStart(2, '0')}`;
  return item;
}

function recommendedDisplayMethods() {
  return [
    // Configuración habitual IDLE / USER.
    methodCompletion('setIdleWakeMode(...)', 'setIdleWakeMode(${1:IDLE_WAKE_BUTTON_ONLY})', 'JWPLC Display · modo de entrada de IDLE a USER', 1),
    methodCompletion('setIdleWakeButton(...)', 'setIdleWakeButton(${1:BTN_OK})', 'JWPLC Display · botón físico de entrada a USER', 2),
    methodCompletion('setIdleReturnMode(...)', 'setIdleReturnMode(${1:IDLE_RETURN_ESC_ONLY})', 'JWPLC Display · modo de retorno a IDLE', 3),
    methodCompletion('setIdleReturnButton(...)', 'setIdleReturnButton(${1:BTN_ESC})', 'JWPLC Display · botón físico de retorno a IDLE', 4),
    methodCompletion('setIdleTimeoutMs(...)', 'setIdleTimeoutMs(${1:15000})', 'JWPLC Display · tiempo de inactividad antes de volver a IDLE', 5),
    methodCompletion('setIdleRefreshPeriodMs(...)', 'setIdleRefreshPeriodMs(${1:50})', 'JWPLC Display · periodo de refresco de la pantalla IDLE', 6),
    methodCompletion('setUserRefreshMode(...)', 'setUserRefreshMode(${1:USER_REFRESH_ON_DEMAND})', 'JWPLC Display · estrategia de refresco de la HMI USER', 7),
    methodCompletion('setUserRefreshPeriodMs(...)', 'setUserRefreshPeriodMs(${1:50})', 'JWPLC Display · periodo de refresco USER cuando el modo es periódico', 8),

    // Navegación y uso habitual de la HMI.
    methodCompletion('enterUserUI()', 'enterUserUI()', 'JWPLC Display · entrar manualmente a USER', 9),
    methodCompletion('goIdle()', 'goIdle()', 'JWPLC Display · volver manualmente a IDLE', 10),
    methodCompletion('setUserPage(...)', 'setUserPage(${1:0})', 'JWPLC Display · seleccionar página USER', 11),
    methodCompletion('setValue(...)', 'setValue(${1:FIELD_ID}, ${2:value})', 'JWPLC Display · actualizar valor, texto o booleano de un campo HMI', 12),
    methodCompletion('setBar(...)', 'setBar(${1:FIELD_ID}, ${2:0.0f})', 'JWPLC Display · actualizar una barra HMI', 13),
    methodCompletion('setPixelMapVisible(...)', 'setPixelMapVisible(${1:PIXELMAP_INDEX}, ${2:true})', 'JWPLC Display · mostrar u ocultar un PixelMap generado', 14),

    // Estado e indicadores que sí son útiles directamente desde el sketch.
    methodCompletion('isReady()', 'isReady()', 'JWPLC Display · indica si la TFT está inicializada', 15),
    methodCompletion('isIdleMode()', 'isIdleMode()', 'JWPLC Display · indica si la pantalla está en IDLE', 16),
    methodCompletion('setRunLed(...)', 'setRunLed(${1:true})', 'JWPLC Display · indicador RUN de la pantalla IDLE', 17),
    methodCompletion('setErrCode(...)', 'setErrCode("${1:A01}")', 'JWPLC Display · código ERR de aplicación', 18),
    methodCompletion('setBusLedAuto(...)', 'setBusLedAuto(${1:true})', 'JWPLC Display · control automático del indicador BUS', 19),
    methodCompletion('setEthLedAuto(...)', 'setEthLedAuto(${1:true})', 'JWPLC Display · control automático del indicador ETH', 20),

    // Acceso raw canónico. display() existe sólo como alias de compatibilidad
    // y deliberadamente no se ofrece para evitar dos nombres equivalentes.
    methodCompletion('tft()', 'tft()', 'JWPLC Display · acceso directo al Adafruit_ST7789', 21)
  ];
}

function buttonCompletions(contextLabel) {
  return [
    enumCompletion('BTN_LEFT', `JWPLC Display · ${contextLabel}: LEFT`, 1),
    enumCompletion('BTN_UP', `JWPLC Display · ${contextLabel}: UP`, 2),
    enumCompletion('BTN_RIGHT', `JWPLC Display · ${contextLabel}: RIGHT`, 3),
    enumCompletion('BTN_ESC', `JWPLC Display · ${contextLabel}: ESC`, 4),
    enumCompletion('BTN_OK', `JWPLC Display · ${contextLabel}: OK`, 5),
    enumCompletion('BTN_DOWN', `JWPLC Display · ${contextLabel}: DOWN`, 6)
  ];
}

function displayConfigCompletions(document, position) {
  const line = document.lineAt(position.line).text.slice(0, position.character);

  if (/JWPLC_Display\s*\.\s*$/.test(line)) {
    return recommendedDisplayMethods();
  }

  if (/JWPLC_Display\s*\.\s*setIdleWakeMode\s*\(\s*$/.test(line)) {
    return [
      enumCompletion('IDLE_WAKE_ANY_BUTTON', 'JWPLC Display · cualquier botón abre USER', 1),
      enumCompletion('IDLE_WAKE_BUTTON_ONLY', 'JWPLC Display · sólo el botón configurado abre USER', 2),
      enumCompletion('IDLE_WAKE_DISABLED', 'JWPLC Display · wake automático deshabilitado', 3)
    ];
  }

  if (/JWPLC_Display\s*\.\s*setIdleWakeButton\s*\(\s*$/.test(line)) {
    return buttonCompletions('botón de wake');
  }

  if (/JWPLC_Display\s*\.\s*setIdleReturnMode\s*\(\s*$/.test(line)) {
    return [
      enumCompletion('IDLE_RETURN_ESC_ONLY', 'JWPLC Display · ESC regresa a IDLE', 1),
      enumCompletion('IDLE_RETURN_TIMEOUT', 'JWPLC Display · regresa a IDLE por inactividad', 2),
      enumCompletion('IDLE_RETURN_BUTTON_ONLY', 'JWPLC Display · botón configurado regresa a IDLE', 3),
      enumCompletion('IDLE_RETURN_DISABLED', 'JWPLC Display · retorno automático deshabilitado', 4)
    ];
  }

  if (/JWPLC_Display\s*\.\s*setIdleReturnButton\s*\(\s*$/.test(line)) {
    return buttonCompletions('botón de retorno');
  }

  if (/JWPLC_Display\s*\.\s*setUserRefreshMode\s*\(\s*$/.test(line)) {
    return [
      enumCompletion('USER_REFRESH_ON_DEMAND', 'JWPLC Display · redibujar sólo cuando cambia contenido', 1),
      enumCompletion('USER_REFRESH_PERIODIC', 'JWPLC Display · refresco USER periódico', 2)
    ];
  }

  return undefined;
}

function activate(context) {
  const command = vscode.commands.registerCommand('jwplc.openHmiDesigner', launchDesigner);
  context.subscriptions.push(command);

  const item = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 25);
  item.text = '$(window) JW HMI';
  item.tooltip = 'Abrir JWPLC HMI Designer';
  item.command = 'jwplc.openHmiDesigner';
  item.show();
  context.subscriptions.push(item);

  // Arduino IDE 2 usa VS Code/Theia internamente, pero la clasificación del
  // lenguaje de un .ino puede variar entre versiones. Se cubren los languageId
  // habituales y además cualquier archivo local .ino.
  const selector = [
    { language: 'cpp', scheme: 'file' },
    { language: 'c', scheme: 'file' },
    { language: 'arduino', scheme: 'file' },
    { scheme: 'file', pattern: '**/*.ino' }
  ];

  // Este provider no intenta replicar toda la clase C++. Ofrece únicamente la
  // API recomendada para sketches y los valores válidos justo dentro de los
  // setters. Alias, getters de configuración y funciones de registro generadas
  // por el Designer siguen disponibles en C++, pero no ensucian las sugerencias.
  const completions = vscode.languages.registerCompletionItemProvider(
    selector,
    { provideCompletionItems: displayConfigCompletions },
    '.',
    '('
  );
  context.subscriptions.push(completions);
}

function deactivate() {}

module.exports = {
  activate,
  deactivate
};
