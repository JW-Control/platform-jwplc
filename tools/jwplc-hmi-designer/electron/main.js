import { app, BrowserWindow, dialog, ipcMain, session, shell } from "electron";
import http from "node:http";
import { watch } from "node:fs";
import fs from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const isPackaged = app.isPackaged;
// Modo desarrollo (Abrir-Designer-Desarrollo.cmd): Electron sin empaquetar
// sirve ../poc del repo y recarga la ventana al guardar un archivo.
const devMode = !isPackaged;
const appUserModelId = "com.jwcontrol.jwplc.hmidesigner";
const HEADER_NAME = "JWPLC_HMI_Generated.h";
// Guía del sketch (poc/designer-sketch-guide.js): nombre fijo y sólo se
// reemplaza un archivo que el Designer escribió (primera línea con la marca).
const GUIDE_NAME = "JWPLC_HMI_LEEME.md";
const GUIDE_MARK = "<!-- Generado por JWPLC HMI Designer";
// «Crear sketch nuevo»: el .ino base lo arma poc/designer-sketch-guide.js.
const SKETCH_INCLUDE = `#include "${HEADER_NAME}"`;
// Regla de nombres del Arduino IDE 2: letra o número, luego letras, números, _ . -
const SKETCH_NAME = /^[A-Za-z0-9][A-Za-z0-9_.-]{0,62}$/;

const webRoot = isPackaged
  ? path.join(process.resourcesPath, "poc")
  : path.resolve(__dirname, "../poc");

const iconPath = isPackaged
  ? path.join(process.resourcesPath, "JWPLC-HMI-Designer.ico")
  : path.resolve(__dirname, "build/icon.ico");

const preloadPath = path.join(__dirname, "preload.cjs");

let mainWindow = null;
let localServer = null;
let linkedSketchPath = null;
let ipcInstalled = false;

const mimeTypes = new Map([
  [".html", "text/html; charset=utf-8"],
  [".css", "text/css; charset=utf-8"],
  [".js", "text/javascript; charset=utf-8"],
  [".json", "application/json; charset=utf-8"],
  [".webmanifest", "application/manifest+json; charset=utf-8"],
  [".svg", "image/svg+xml"],
  [".png", "image/png"],
  [".ico", "image/x-icon"],
  [".bin", "application/octet-stream"]
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
      if (request.method === "HEAD") response.end();
      else response.end(data);
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

  // Selector de puertos propio (poc/designer-port-picker.js): main manda la
  // lista a la ventana y espera el puerto elegido. Si la ventana no registró
  // el selector, se usa el cuadro de Windows como respaldo.
  ses.on("select-serial-port", (event, portList, webContents, callback) => {
    event.preventDefault();

    if (serialPortRequest) serialPortRequest.finish("");

    if (!serialPickerReady || !webContents || webContents.isDestroyed()) {
      showNativeSerialPortDialog(portList, callback);
      return;
    }

    const requestId = ++serialPortRequestSeq;
    const ports = new Map(portList.map((port) => [port.portId, port]));
    serialPortRequest = {
      requestId,
      webContents,
      ports,
      finish(portId) {
        if (serialPortRequest !== this) return;
        serialPortRequest = null;
        callback(portId && ports.has(portId) ? portId : "");
      }
    };
    webContents.send("jwplc-hmi:serial-port-request", { requestId, ports: portList.map(describeSerialPort) });
  });

  const sendSerialPortList = () => {
    const request = serialPortRequest;
    if (!request || request.webContents.isDestroyed()) return;
    request.webContents.send("jwplc-hmi:serial-port-list", {
      requestId: request.requestId,
      ports: [...request.ports.values()].map(describeSerialPort)
    });
  };

  ses.on("serial-port-added", (_event, port) => {
    if (!serialPortRequest) return;
    serialPortRequest.ports.set(port.portId, port);
    sendSerialPortList();
  });

  ses.on("serial-port-removed", (_event, port) => {
    if (!serialPortRequest) return;
    serialPortRequest.ports.delete(port.portId);
    sendSerialPortList();
  });

  ipcMain.on("jwplc-hmi:serial-picker-ready", () => {
    serialPickerReady = true;
  });

  ipcMain.on("jwplc-hmi:serial-port-chosen", (_event, payload = {}) => {
    if (serialPortRequest && serialPortRequest.requestId === payload.requestId) {
      serialPortRequest.finish(String(payload.portId || ""));
    }
  });
};

let serialPickerReady = false;
let serialPortRequest = null;
let serialPortRequestSeq = 0;

// Chips USB-serie habituales en placas ESP32 (vendorId en hexadecimal).
const USB_SERIAL_CHIPS = {
  "1a86": "CH340 / CH343",
  "10c4": "CP210x",
  "0403": "FTDI",
  "303a": "Espressif USB"
};

const toHexId = (value) => {
  if (value === undefined || value === null || value === "") return "";
  const n = Number.parseInt(String(value), 10);
  return Number.isFinite(n) ? n.toString(16).padStart(4, "0") : String(value);
};

const describeSerialPort = (port) => {
  const vid = toHexId(port.vendorId);
  const pid = toHexId(port.productId);
  return {
    portId: port.portId,
    portName: port.portName || "",
    displayName: port.displayName || "",
    vendorId: vid,
    productId: pid,
    chip: USB_SERIAL_CHIPS[vid] || ""
  };
};

const showNativeSerialPortDialog = (portList, callback) => {
  if (!portList.length) {
    callback("");
    return;
  }

  const labels = portList.map((port, index) => {
    const info = describeSerialPort(port);
    const name = [info.portName, info.displayName].filter(Boolean).join(" · ") || `Puerto ${index + 1}`;
    const detail = [info.vendorId, info.productId].filter(Boolean).join(":");
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
};

const linkedStateFile = () => path.join(app.getPath("userData"), "linked-sketch.json");

const directoryContainsSketch = async (directoryPath) => {
  try {
    const entries = await fs.readdir(directoryPath, { withFileTypes: true });
    return entries.some((entry) => entry.isFile() && /\.ino$/i.test(entry.name));
  } catch {
    return false;
  }
};

const fileExists = async (filePath) => {
  try {
    await fs.access(filePath);
    return true;
  } catch {
    return false;
  }
};

// El HMI del sketch es <carpeta>.jwhmi junto al .ino (lo escribe "Escribir en sketch").
const projectFileName = (directoryPath) => `${path.basename(directoryPath)}.jwhmi`;

// El .ino principal se llama como la carpeta (regla del Arduino IDE); si no, el primero.
const mainInoPath = async (directoryPath) => {
  const preferred = path.join(directoryPath, `${path.basename(directoryPath)}.ino`);
  if (await fileExists(preferred)) return preferred;
  const entries = await fs.readdir(directoryPath, { withFileTypes: true });
  const first = entries.find((entry) => entry.isFile() && /\.ino$/i.test(entry.name));
  return first ? path.join(directoryPath, first.name) : null;
};

const sketchInfo = async (directoryPath) => {
  if (!directoryPath || !(await directoryContainsSketch(directoryPath))) return null;
  return {
    ok: true,
    name: path.basename(directoryPath),
    path: directoryPath,
    headerExists: await fileExists(path.join(directoryPath, HEADER_NAME)),
    projectFile: projectFileName(directoryPath),
    projectExists: await fileExists(path.join(directoryPath, projectFileName(directoryPath)))
  };
};

const persistLinkedSketch = async () => {
  try {
    await fs.mkdir(path.dirname(linkedStateFile()), { recursive: true });
    await fs.writeFile(linkedStateFile(), JSON.stringify({ path: linkedSketchPath }, null, 2), "utf8");
  } catch {
    // Persistencia best-effort; la vinculación de la sesión sigue siendo válida.
  }
};

const clearPersistedLinkedSketch = async () => {
  try {
    await fs.rm(linkedStateFile(), { force: true });
  } catch {
    // Best-effort
  }
};

const resolveSketchFromArg = async (arg) => {
  if (!arg || typeof arg !== "string") return null;
  let candidate = arg.trim();

  if ((candidate.startsWith('"') && candidate.endsWith('"')) || (candidate.startsWith("'") && candidate.endsWith("'"))) {
    candidate = candidate.slice(1, -1).trim();
  }

  if (candidate.startsWith("jwplc-hmi://") || candidate.startsWith("jwplc-hmi:")) {
    try {
      const parsedUrl = new URL(candidate);
      const sketchParam = parsedUrl.searchParams.get("sketch");
      if (sketchParam) candidate = sketchParam;
      else if (parsedUrl.pathname) {
        candidate = decodeURIComponent(parsedUrl.pathname.replace(/^\/+/, ""));
      }
    } catch (_) {}
  }

  if (candidate.startsWith("--sketch=")) {
    candidate = candidate.slice("--sketch=".length);
  }

  if (/\.ino$/i.test(candidate)) {
    candidate = path.dirname(candidate);
  }

  if (await directoryContainsSketch(candidate)) {
    return path.resolve(candidate);
  }

  return null;
};

const extractSketchPathFromArgv = async (argv) => {
  if (!Array.isArray(argv)) return null;
  for (let i = 1; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === "--sketch" && i + 1 < argv.length) {
      const resolved = await resolveSketchFromArg(argv[i + 1]);
      if (resolved) return resolved;
    }
    if (arg.startsWith("--") && !arg.startsWith("--sketch")) continue;
    const resolved = await resolveSketchFromArg(arg);
    if (resolved) return resolved;
  }
  return null;
};

const restoreLinkedSketchPath = async () => {
  if (linkedSketchPath) return linkedSketchPath;
  try {
    const state = JSON.parse(await fs.readFile(linkedStateFile(), "utf8"));
    if (state?.path && await directoryContainsSketch(state.path)) linkedSketchPath = state.path;
  } catch {
    linkedSketchPath = null;
  }
  return linkedSketchPath;
};

const installNativeIpc = () => {
  if (ipcInstalled) return;
  ipcInstalled = true;

  ipcMain.handle("jwplc-hmi:select-sketch-directory", async () => {
    const result = await dialog.showOpenDialog(mainWindow, {
      title: "Vincular sketch Arduino",
      properties: ["openDirectory"]
    });
    if (result.canceled || !result.filePaths?.[0]) return { ok: false, canceled: true };

    const selectedPath = result.filePaths[0];
    if (!(await directoryContainsSketch(selectedPath))) {
      return { ok: false, error: "La carpeta seleccionada no contiene un archivo .ino." };
    }

    linkedSketchPath = selectedPath;
    await persistLinkedSketch();
    return await sketchInfo(linkedSketchPath);
  });

  // Crea <carpeta>/<nombre>/<nombre>.ino con el sketch base y lo vincula.
  // Nunca reemplaza: si la carpeta ya existe y no está vacía, no se toca.
  ipcMain.handle("jwplc-hmi:create-sketch", async (_event, payload = {}) => {
    const inoText = String(payload.inoText || "");
    if (!inoText.startsWith(SKETCH_INCLUDE) || inoText.length > 256 * 1024) {
      return { ok: false, error: "El contenido no corresponde al sketch base del HMI." };
    }
    const suggested = SKETCH_NAME.test(String(payload.suggestedName || "")) ? payload.suggestedName : "JWPLC_HMI";
    const result = await dialog.showSaveDialog(mainWindow, {
      title: "Crear sketch Arduino",
      defaultPath: path.join(app.getPath("documents"), "Arduino", `${suggested}.ino`),
      buttonLabel: "Crear sketch",
      filters: [{ name: "Sketch Arduino", extensions: ["ino"] }],
      properties: ["createDirectory"]
    });
    if (result.canceled || !result.filePath) return { ok: false, canceled: true };

    const name = path.basename(result.filePath).replace(/\.ino$/i, "");
    if (!SKETCH_NAME.test(name)) {
      return { ok: false, error: `Nombre de sketch inválido: «${name}». Usa letras, números, _ . - (empieza con letra o número, máximo 63).` };
    }
    // Si el usuario eligió como ubicación una carpeta con ese mismo nombre, se usa.
    const parent = path.dirname(result.filePath);
    const directoryPath = path.basename(parent) === name ? parent : path.join(parent, name);
    try {
      if ((await fs.readdir(directoryPath)).length) {
        return { ok: false, error: `Ya existe la carpeta «${directoryPath}» y no está vacía. Elige otro nombre o usa «Elegir sketch existente».` };
      }
    } catch (error) {
      if (error?.code !== "ENOENT") return { ok: false, error: error?.message || String(error) };
    }

    try {
      await fs.mkdir(directoryPath, { recursive: true });
      await fs.writeFile(path.join(directoryPath, `${name}.ino`), inoText.endsWith("\n") ? inoText : `${inoText}\n`, { encoding: "utf8", flag: "wx" });
    } catch (error) {
      return { ok: false, error: `No se pudo crear el sketch: ${error?.message || error}` };
    }

    linkedSketchPath = directoryPath;
    await persistLinkedSketch();
    return { ...(await sketchInfo(linkedSketchPath)), created: true };
  });

  // «Abrir»: el .jwhmi elegido y, si está junto a un .ino, ese sketch queda
  // vinculado. Un .jwhmi suelto desvincula el anterior, como «Nuevo proyecto».
  ipcMain.handle("jwplc-hmi:open-project-file", async () => {
    const result = await dialog.showOpenDialog(mainWindow, {
      title: "Abrir proyecto HMI",
      filters: [{ name: "Proyecto JWPLC HMI", extensions: ["jwhmi"] }],
      properties: ["openFile"]
    });
    if (result.canceled || !result.filePaths?.[0]) return { ok: false, canceled: true };

    const filePath = result.filePaths[0];
    let text;
    try {
      text = await fs.readFile(filePath, "utf8");
    } catch (error) {
      return { ok: false, error: `No se pudo leer ${path.basename(filePath)}: ${error?.message || error}` };
    }

    const directoryPath = path.dirname(filePath);
    if (await directoryContainsSketch(directoryPath)) {
      linkedSketchPath = directoryPath;
      await persistLinkedSketch();
    } else {
      linkedSketchPath = null;
      await clearPersistedLinkedSketch();
    }
    return { ok: true, fileName: path.basename(filePath), path: filePath, text, sketch: await sketchInfo(linkedSketchPath) };
  });

  ipcMain.handle("jwplc-hmi:get-linked-sketch", async () => {
    await restoreLinkedSketchPath();
    return (await sketchInfo(linkedSketchPath)) || { ok: false };
  });

  ipcMain.handle("jwplc-hmi:unlink-sketch", async () => {
    linkedSketchPath = null;
    await clearPersistedLinkedSketch();
    return { ok: true };
  });

  ipcMain.handle("jwplc-hmi:set-linked-sketch", async (_event, payload = {}) => {
    const targetPath = payload.path ? path.resolve(payload.path) : null;
    if (targetPath && await directoryContainsSketch(targetPath)) {
      linkedSketchPath = targetPath;
      await persistLinkedSketch();
      return await sketchInfo(linkedSketchPath);
    }
    return { ok: false, error: "Ruta de sketch no válida." };
  });

  ipcMain.handle("jwplc-hmi:write-generated-header", async (_event, payload = {}) => {
    await restoreLinkedSketchPath();
    const info = await sketchInfo(linkedSketchPath);
    if (!info) return { ok: false, error: "No hay un sketch vinculado válido." };

    const target = path.join(linkedSketchPath, HEADER_NAME);
    const exists = await fileExists(target);
    if (exists && !payload.overwrite) return { ok: false, requiresOverwrite: true, name: info.name };

    const text = String(payload.text || "");
    if (!text.startsWith("// Código generado por JWPLC HMI Designer") || !text.includes("#pragma once")) {
      return { ok: false, error: "El contenido generado no corresponde a un header HMI válido." };
    }

    await fs.writeFile(target, text, "utf8");
    return { ok: true, name: info.name, path: target };
  });

  ipcMain.handle("jwplc-hmi:read-project", async () => {
    await restoreLinkedSketchPath();
    const info = await sketchInfo(linkedSketchPath);
    if (!info) return { ok: false, error: "No hay un sketch vinculado válido." };
    if (!info.projectExists) return { ok: false, missing: true, fileName: info.projectFile };
    const text = await fs.readFile(path.join(linkedSketchPath, info.projectFile), "utf8");
    return { ok: true, fileName: info.projectFile, text };
  });

  ipcMain.handle("jwplc-hmi:open-sketch-ino", async () => {
    await restoreLinkedSketchPath();
    if (!(await sketchInfo(linkedSketchPath))) return { ok: false, error: "No hay un sketch vinculado válido." };
    const ino = await mainInoPath(linkedSketchPath);
    if (!ino) return { ok: false, error: "El sketch no tiene archivo .ino." };
    // Abre con el programa asociado a .ino (normalmente el Arduino IDE).
    const error = await shell.openPath(ino);
    return error ? { ok: false, error } : { ok: true, path: ino };
  });

  ipcMain.handle("jwplc-hmi:open-sketch-folder", async () => {
    await restoreLinkedSketchPath();
    if (!(await sketchInfo(linkedSketchPath))) return { ok: false, error: "No hay un sketch vinculado válido." };
    const error = await shell.openPath(linkedSketchPath);
    return error ? { ok: false, error } : { ok: true, path: linkedSketchPath };
  });

  ipcMain.handle("jwplc-hmi:write-guide", async (_event, payload = {}) => {
    await restoreLinkedSketchPath();
    const info = await sketchInfo(linkedSketchPath);
    if (!info) return { ok: false, error: "No hay un sketch vinculado válido." };

    const text = String(payload.text || "");
    if (!text.startsWith(GUIDE_MARK)) return { ok: false, error: "El contenido no corresponde a la guía del HMI." };

    const target = path.join(linkedSketchPath, GUIDE_NAME);
    if (await fileExists(target)) {
      const current = await fs.readFile(target, "utf8");
      if (!current.startsWith(GUIDE_MARK)) return { ok: false, foreign: true, name: GUIDE_NAME };
    }
    await fs.writeFile(target, text, "utf8");
    return { ok: true, name: GUIDE_NAME, path: target };
  });

  ipcMain.handle("jwplc-hmi:write-project", async (_event, payload = {}) => {
    await restoreLinkedSketchPath();
    const info = await sketchInfo(linkedSketchPath);
    if (!info) return { ok: false, error: "No hay un sketch vinculado válido." };

    const requestedName = String(payload.fileName || `${info.name}.jwhmi`);
    const safeName = path.basename(requestedName);
    if (!/^[^<>:\"/\\|?*]+\.jwhmi$/i.test(safeName)) {
      return { ok: false, error: "Nombre de proyecto .jwhmi inválido." };
    }

    const target = path.join(linkedSketchPath, safeName);
    const exists = await fileExists(target);
    if (exists && !payload.overwrite) return { ok: false, requiresOverwrite: true, name: info.name };

    await fs.writeFile(target, String(payload.text || ""), "utf8");
    return { ok: true, name: info.name, path: target };
  });
};

// --- Modo desarrollo ---------------------------------------------------------
// F5 / Ctrl+R: como reabrir la app (diseño en blanco y sin sketch vinculado).
// Guardar un archivo de poc/: recarga conservando el diseño, que se guarda en
// sessionStorage (sobrevive a la recarga de la misma ventana) y se vuelve a
// aplicar cuando el Designer arranca.
const DEV_SNAPSHOT_KEY = "jwplc-hmi.dev.snapshot";

const devRestart = async () => {
  if (!mainWindow || mainWindow.isDestroyed()) return;
  linkedSketchPath = null;
  await clearPersistedLinkedSketch();
  mainWindow.webContents.reloadIgnoringCache();
};

const devReload = async () => {
  if (!mainWindow || mainWindow.isDestroyed()) return;
  const contents = mainWindow.webContents;
  try {
    await contents.executeJavaScript(`(() => {
      try {
        const project = window.JWPLCHMIProject;
        if (project?.serialize) sessionStorage.setItem(${JSON.stringify(DEV_SNAPSHOT_KEY)}, JSON.stringify(project.serialize()));
      } catch (_) {}
    })()`);
  } catch {
    // Sin snapshot: se recarga igual.
  }
  contents.reloadIgnoringCache();
};

const devRestoreSnapshot = () => mainWindow?.webContents.executeJavaScript(`(async () => {
  const key = ${JSON.stringify(DEV_SNAPSHOT_KEY)};
  const text = sessionStorage.getItem(key);
  if (!text) return;
  sessionStorage.removeItem(key);
  for (let i = 0; i < 100 && !(window.JWPLCHMIProject?.apply && window.JWPLCHMIPages); i++) {
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
  // Los módulos terminan de montarse después de exponer la API.
  await new Promise((resolve) => setTimeout(resolve, 400));
  try { await window.JWPLCHMIProject.apply(JSON.parse(text)); } catch (error) { console.warn("[JWPLC HMI dev] No se pudo restaurar el diseño.", error); }
})()`).catch(() => {});

const installDevMode = () => {
  const contents = mainWindow.webContents;
  contents.on("did-finish-load", devRestoreSnapshot);
  contents.on("before-input-event", (event, input) => {
    if (input.type !== "keyDown") return;
    const key = input.key.toLowerCase();
    if (key === "f5" || ((input.control || input.meta) && key === "r")) {
      event.preventDefault();
      devRestart();
    } else if (key === "f12") {
      event.preventDefault();
      contents.toggleDevTools();
    }
  });

  // Guardar un archivo en poc/ recarga la ventana (los editores escriben en ráfagas).
  let timer = null;
  try {
    watch(webRoot, { recursive: true }, (_event, fileName) => {
      if (fileName && /(^|[\\/])(\.|~)|\.(tmp|swp)$/i.test(fileName)) return;
      clearTimeout(timer);
      timer = setTimeout(devReload, 250);
    });
  } catch (error) {
    console.warn("[JWPLC HMI dev] Sin recarga automática; usa F5.", error);
  }
};

const createWindow = async () => {
  const baseUrl = await startStaticServer();
  installSerialPermissions();
  installNativeIpc();

  mainWindow = new BrowserWindow({
    width: 1440,
    height: 900,
    minWidth: 900,
    minHeight: 620,
    show: false,
    backgroundColor: "#101a21",
    icon: iconPath,
    autoHideMenuBar: true,
    title: "JWPLC HMI Designer — 2.1.0-alpha.14",
    webPreferences: {
      preload: preloadPath,
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
  if (devMode) {
    // El <title> de la página reemplaza el de la ventana: se marca después.
    mainWindow.on("page-title-updated", (event, title) => {
      event.preventDefault();
      mainWindow.setTitle(`${title} [desarrollo]`);
    });
    installDevMode();
  }

  await mainWindow.loadURL(`${baseUrl}/desktop.html?app=alpha11-electron-v2`);
};

// En desarrollo no se registra jwplc-hmi:// (le quitaría el enlace a la app instalada).
if (process.platform === "win32" && !devMode) {
  app.setAppUserModelId(appUserModelId);
  app.setAsDefaultProtocolClient("jwplc-hmi");
}

const gotSingleInstanceLock = app.requestSingleInstanceLock();
if (!gotSingleInstanceLock) {
  app.quit();
} else {
  app.on("second-instance", async (_event, commandLine) => {
    if (!mainWindow) return;
    if (mainWindow.isMinimized()) mainWindow.restore();
    mainWindow.show();
    mainWindow.focus();

    const newSketch = await extractSketchPathFromArgv(commandLine);
    if (newSketch) {
      linkedSketchPath = newSketch;
      await persistLinkedSketch();
      const info = await sketchInfo(linkedSketchPath);
      mainWindow.webContents.send("jwplc-hmi:sketch-opened", info);
    }
  });

  app.whenReady().then(async () => {
    const cliSketch = await extractSketchPathFromArgv(process.argv);
    if (cliSketch) {
      linkedSketchPath = cliSketch;
      await persistLinkedSketch();
    } else {
      // Iniciar proyecto nuevo limpio sin vincular
      linkedSketchPath = null;
      await clearPersistedLinkedSketch();
    }
    await createWindow();
  }).catch((error) => {
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