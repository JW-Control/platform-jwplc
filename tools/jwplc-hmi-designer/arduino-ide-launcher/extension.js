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

function activate(context) {
  const command = vscode.commands.registerCommand('jwplc.openHmiDesigner', launchDesigner);
  context.subscriptions.push(command);

  const item = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 25);
  item.text = '$(window) JW HMI';
  item.tooltip = 'Abrir JWPLC HMI Designer';
  item.command = 'jwplc.openHmiDesigner';
  item.show();
  context.subscriptions.push(item);
}

function deactivate() {}

module.exports = {
  activate,
  deactivate
};
