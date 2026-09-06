'use strict';

const vscode = require('vscode');

const DESIGNER_URI = 'jwplc-hmi://open';

async function launchDesigner() {
  vscode.window.setStatusBarMessage('JW HMI: abriendo Designer…', 1800);

  try {
    const opened = await vscode.env.openExternal(vscode.Uri.parse(DESIGNER_URI));
    if (!opened) {
      vscode.window.showErrorMessage(
        'Windows no pudo abrir JWPLC HMI Designer. Reinstala la aplicación y reinicia Arduino IDE.'
      );
    }
  } catch (error) {
    vscode.window.showErrorMessage(
      `No se pudo abrir JWPLC HMI Designer: ${error?.message || error}`
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
