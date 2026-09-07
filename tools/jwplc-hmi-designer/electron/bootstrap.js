import fs from "node:fs/promises";
import path from "node:path";

const portableRoot = process.env.PORTABLE_EXECUTABLE_DIR || path.dirname(process.execPath);
const externalPoc = path.join(portableRoot, "poc");
const packagedPoc = path.join(process.resourcesPath, "poc");

async function directoryExists(target) {
  try {
    const stat = await fs.stat(target);
    return stat.isDirectory();
  } catch {
    return false;
  }
}

async function syncExternalFrontend() {
  if (!(await directoryExists(externalPoc))) return false;
  try {
    await fs.rm(packagedPoc, { recursive: true, force: true });
    await fs.cp(externalPoc, packagedPoc, { recursive: true, force: true });
    return true;
  } catch (error) {
    console.warn("[JWPLC HMI] No se pudo sincronizar el frontend externo; se usará el embebido.", error);
    return false;
  }
}

await syncExternalFrontend();
await import("./main.js");
