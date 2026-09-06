import { app, BrowserWindow, dialog, session } from "electron";
import http from "node:http";
import fs from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const isPackaged = app.isPackaged;
const appUserModelId = "com.jwcontrol.jwplc.hmidesigner";

const webRoot = isPackaged
  ? path.join(process.resourcesPath, "poc")
  : path.resolve(__dirname, "../poc");

const iconPath = isPackaged
  ? path.join(process.resourcesPath, "JWPLC-HMI-Designer.ico")
  : path.resolve(__dirname, "build/icon.ico");

let mainWindow = null;
let localServer = null;

const mimeTypes = new Map([
  [".html", "text/html; charset=utf-8"],
  [".css", "text/css; charset=utf-8"],
  [".js", "text/javascript; charset=utf-8"],
  [".json", "application/json; charset=utf-8"],
  [".webmanifest", "application/manifest+json; charset=utf-8"],
  [".svg", "image/svg+xml"],
  [".png", "image/png"],
  [".ico", "image/x-icon"]
]);

const resolveWebPath = (requestUrl) => {
  const url = new URL(requestUrl, "http://127.0.0.1");
  let pathname = decodeURIComponent(url.pathname || "/");
  if (pathname === "/") pathname = "/desktop.html";

  const candidate = path.resolve(webRoot, `.${pathname}`);
  const rootPrefix = `${path.resolve(webRoot)}${path.sep}`.toLowerCase();
  const candidateLower = candidate.toLowerCase();
  if (candidateLower !== path.resolve(webRoot).toLowerCase() && !candidateLower.startsWith(rootPrefix)) {
    return null;
  }
  return candidate;
};

const startStaticServer = async () => {
  localServer = http.createServer(async (request, response) => {
    try {
      if (request.url === "/__health") {
        response.writeHead(200, { "Content-Type": "text/plain; charset=utf-8", "Cache-Control": "no-store" });
        response.end("JWPLC_HMI_OK");
        return;
      }

      if (request.method !== "GET" && request.method !== "HEAD") {
        response.writeHead(405);
        response.end();
        return;
      }

      const filePath = resolveWebPath(request.url || "/");
      if (!filePath) {
        response.writeHead(403);
        response.end("Forbidden");
        return;
      }

      const data = await fs.readFile(filePath);
      const contentType = mimeTypes.get(path.extname(filePath).toLowerCase()) || "application/octet-stream";
      response.writeHead(200, {
        "Content-Type": contentType,
        "Cache-Control": "no-store"
      });
      if (request.method === "HEAD") {
        response.end();
      } else {
        response.end(data);
      }
    } catch (error) {
      response.writeHead(error?.code === "ENOENT" ? 404 : 500, { "Content-Type": "text/plain; charset=utf-8" });
      response.end(error?.code === "ENOENT" ? "Not found" : "Internal server error");
    }
  });

  await new Promise((resolve, reject) => {
    localServer.once("error", reject);
    localServer.listen(0, "127.0.0.1", () => resolve());
  });

  const address = localServer.address();
  return `http://127.0.0.1:${address.port}`;
};

const installSerialPermissions = () => {
  const ses = session.defaultSession;

  ses.setPermissionCheckHandler((_webContents, permission) => permission === "serial");
  ses.setPermissionRequestHandler((_webContents, permission, callback) => callback(permission === "serial"));
  ses.setDevicePermissionHandler((details) => details.deviceType === "serial");

  ses.on("select-serial-port", (event, portList, _webContents, callback) => {
    event.preventDefault();

    if (!portList.length) {
      callback("");
      return;
    }

    const labels = portList.map((port, index) => {
      const name = port.displayName || port.portName || `Puerto ${index + 1}`;
      const detail = [port.vendorId, port.productId].filter(Boolean).join(":");
      return detail ? `${name} (${detail})` : name;
    });
    labels.push("Cancelar");

    dialog.showMessageBox(mainWindow, {
      type: "question",
      title: "Conectar JWPLC",
      message: "Selecciona el puerto serie para LIVE",
      buttons: labels,
      cancelId: labels.length - 1,
      defaultId: 0,
      noLink: true
    }).then(({ response }) => {
      callback(response >= 0 && response < portList.length ? portList[response].portId : "");
    }).catch(() => callback(""));
  });
};

const createWindow = async () => {
  const baseUrl = await startStaticServer();
  installSerialPermissions();

  mainWindow = new BrowserWindow({
    width: 1440,
    height: 900,
    minWidth: 900,
    minHeight: 620,
    show: false,
    backgroundColor: "#101a21",
    icon: iconPath,
    autoHideMenuBar: true,
    title: "JWPLC HMI Designer — Alpha11",
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
      backgroundThrottling: false
    }
  });

  mainWindow.setMenuBarVisibility(false);
  mainWindow.once("ready-to-show", () => {
    mainWindow.show();
    mainWindow.maximize();
  });
  mainWindow.on("closed", () => {
    mainWindow = null;
  });

  await mainWindow.loadURL(`${baseUrl}/desktop.html?app=alpha11-electron-v1`);
};

if (process.platform === "win32") {
  app.setAppUserModelId(appUserModelId);
}

const gotSingleInstanceLock = app.requestSingleInstanceLock();
if (!gotSingleInstanceLock) {
  app.quit();
} else {
  app.on("second-instance", () => {
    if (!mainWindow) return;
    if (mainWindow.isMinimized()) mainWindow.restore();
    mainWindow.show();
    mainWindow.focus();
  });

  app.whenReady().then(createWindow).catch((error) => {
    dialog.showErrorBox("JWPLC HMI Designer", `No se pudo iniciar la aplicación.\n\n${error?.stack || error}`);
    app.quit();
  });
}

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") app.quit();
});

app.on("before-quit", () => {
  if (localServer) {
    try {
      localServer.close();
    } catch {
      // Cierre best-effort: nunca bloquear la salida por el servidor local.
    }
  }
});
