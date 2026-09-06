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

async function openByProtocol() {
  try {
    return await vscode.env.openExternal(vscode.Uri.parse(DESIGNER_URI));
  } catch (_) {
    return false;
  }
}

async function launchDesigner() {
  vscode.window.setStatusBarMessage('JW HMI: abriendo Designer…', 1800);

  const exe = findDesignerExe();
  if (exe) {
    try {
      const child = childProcess.spawn(exe, [], {
        cwd: path.dirname(exe),
        detached: true,
        stdio: 'ignore',
        windowsHide: true
      });

      child.once('error', async (error) => {
        const opened = await openByProtocol();
        if (!opened) {
          vscode.window.showErrorMessage(
            `No se pudo abrir JWPLC HMI Designer (${error.message}). Reinstala la aplicación.`
          );
        }
      });

      child.unref();
      return;
    } catch (error) {
      const opened = await openByProtocol();
      if (opened) return;
      vscode.window.showErrorMessage(
        `No se pudo ejecutar JWPLC HMI Designer: ${error?.message || error}`
      );
      return;
    }
  }

  const opened = await openByProtocol();
  if (!opened) {
    vscode.window.showErrorMessage(
      'JWPLC HMI Designer no está instalado. Reinstala la aplicación y reinicia Arduino IDE.'
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
