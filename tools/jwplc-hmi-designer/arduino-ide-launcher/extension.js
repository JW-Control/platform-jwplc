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

function displayConfigCompletions(document, position) {
  const line = document.lineAt(position.line).text.slice(0, position.character);

  if (/JWPLC_Display\s*\.\s*setIdleWakeMode\s*\(\s*$/.test(line)) {
    return [
      enumCompletion('IDLE_WAKE_ANY_BUTTON', 'JWPLC Display · cualquier botón abre USER', 1),
      enumCompletion('IDLE_WAKE_BUTTON_ONLY', 'JWPLC Display · sólo el botón configurado abre USER', 2),
      enumCompletion('IDLE_WAKE_DISABLED', 'JWPLC Display · wake automático deshabilitado', 3)
    ];
  }

  if (/JWPLC_Display\s*\.\s*setIdleReturnMode\s*\(\s*$/.test(line)) {
    return [
      enumCompletion('IDLE_RETURN_ESC_ONLY', 'JWPLC Display · ESC regresa a IDLE', 1),
      enumCompletion('IDLE_RETURN_TIMEOUT', 'JWPLC Display · regresa a IDLE por inactividad', 2),
      enumCompletion('IDLE_RETURN_BUTTON_ONLY', 'JWPLC Display · botón configurado regresa a IDLE', 3),
      enumCompletion('IDLE_RETURN_DISABLED', 'JWPLC Display · retorno automático deshabilitado', 4)
    ];
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

  // Arduino IDE 2 usa el motor de extensiones VS Code/Theia. Este provider no
  // sustituye IntelliSense de C++; sólo añade las opciones JWPLC pertinentes
  // justo al escribir el paréntesis de los setters de configuración habituales.
  const selector = [
    { language: 'cpp', scheme: 'file' },
    { language: 'c', scheme: 'file' },
    { language: 'arduino', scheme: 'file' }
  ];

  const completions = vscode.languages.registerCompletionItemProvider(
    selector,
    { provideCompletionItems: displayConfigCompletions },
    '('
  );
  context.subscriptions.push(completions);
}

function deactivate() {}

module.exports = {
  activate,
  deactivate
};
