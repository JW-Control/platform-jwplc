'use strict';

const vscode = require('vscode');
const fs = require('fs');
const path = require('path');
const childProcess = require('child_process');

function designerHome() {
  if (process.env.JWPLC_HMI_DESIGNER_HOME) {
    return process.env.JWPLC_HMI_DESIGNER_HOME;
  }
  const localAppData = process.env.LOCALAPPDATA;
  if (!localAppData) return null;
  return path.join(localAppData, 'JWPLC', 'HMI Designer');
}

function launcherCandidates() {
  const home = designerHome();
  if (!home) return [];
  return [
    path.join(home, 'Start-JWPLC-HMI-Designer.ps1'),
    path.join(home, 'JWPLC-HMI-Designer.cmd')
  ];
}

function findLauncher() {
  return launcherCandidates().find((candidate) => fs.existsSync(candidate)) || null;
}

function launchDesigner() {
  const launcher = findLauncher();
  if (!launcher) {
    vscode.window.showErrorMessage(
      'JWPLC HMI Designer no está instalado. Ejecuta Install-JWPLC-HMI-Designer.ps1 y reinicia Arduino IDE.'
    );
    return;
  }

  try {
    if (/\.ps1$/i.test(launcher)) {
      const child = childProcess.spawn(
        'powershell.exe',
        ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-WindowStyle', 'Hidden', '-File', launcher],
        { detached: true, stdio: 'ignore', windowsHide: true }
      );
      child.unref();
    } else {
      const child = childProcess.spawn('cmd.exe', ['/d', '/c', 'start', '""', launcher], {
        detached: true,
        stdio: 'ignore',
        windowsHide: true
      });
      child.unref();
    }
  } catch (error) {
    vscode.window.showErrorMessage(`No se pudo abrir JWPLC HMI Designer: ${error.message}`);
  }
}

function activate(context) {
  const command = vscode.commands.registerCommand('jwplc.openHmiDesigner', launchDesigner);
  context.subscriptions.push(command);

  // El status bar es el launcher visual más estable en Theia/VS Code API.
  // El aporte editor/title se mantiene como intento adicional y puede variar
  // según la versión de Arduino IDE.
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
