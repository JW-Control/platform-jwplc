#!/usr/bin/env node
// Firma un directorio VPP con Ed25519 generando signature.json.
// Compatible con el modelo usado por OpenPLC Editor 4.2.7:
// - enumera archivos regulares, excluyendo signature.json
// - calcula SHA-256 por archivo
// - canonicaliza JSON con claves ordenadas
// - firma el payload con Ed25519

import { createHash, sign as cryptoSign } from 'node:crypto'
import { existsSync, lstatSync, readdirSync, readFileSync, writeFileSync } from 'node:fs'
import { join, relative, resolve, sep } from 'node:path'

const SIGNATURE_FILENAME = 'signature.json'

function usage() {
  console.error('Uso: node sign-vpp-package.mjs <vppDir> <keyId> <privateKeyPemPath>')
  console.error('Ejemplo: node tools/sign-vpp-package.mjs vpp jw-control-dev keys/jw-control-dev-private.pem')
  process.exit(2)
}

const [vppDirArg, keyId, privateKeyPathArg] = process.argv.slice(2)
if (!vppDirArg || !keyId || !privateKeyPathArg) usage()

const vppDir = resolve(vppDirArg)
const privateKeyPath = resolve(privateKeyPathArg)

if (!existsSync(vppDir)) throw new Error(`No existe vppDir: ${vppDir}`)
if (!existsSync(privateKeyPath)) throw new Error(`No existe privateKeyPemPath: ${privateKeyPath}`)

function canonicalize(value) {
  if (value === null || typeof value !== 'object') return JSON.stringify(value)
  if (Array.isArray(value)) return `[${value.map((v) => canonicalize(v)).join(',')}]`
  return `{${Object.keys(value)
    .sort()
    .map((k) => `${JSON.stringify(k)}:${canonicalize(value[k])}`)
    .join(',')}}`
}

function listFiles(root) {
  const out = []
  function walk(current) {
    for (const entry of readdirSync(current)) {
      const full = join(current, entry)
      const stat = lstatSync(full)
      if (stat.isDirectory()) walk(full)
      else if (stat.isFile()) out.push(relative(root, full).split(sep).join('/'))
      else throw new Error(`Entrada no soportada: ${relative(root, full)}`)
    }
  }
  walk(root)
  return out.filter((f) => f !== SIGNATURE_FILENAME).sort()
}

function sha256File(path) {
  return createHash('sha256').update(readFileSync(path)).digest('hex')
}

const manifest = JSON.parse(readFileSync(join(vppDir, 'manifest.json'), 'utf8'))
const files = {}
for (const rel of listFiles(vppDir)) files[rel] = sha256File(join(vppDir, rel))

const payload = {
  formatVersion: '1.0',
  alg: 'ed25519',
  keyId,
  packageId: manifest.package.id,
  version: manifest.package.version,
  signedAt: new Date().toISOString(),
  files,
}

const privateKeyPem = readFileSync(privateKeyPath, 'utf8')
const signature = cryptoSign(null, Buffer.from(canonicalize(payload), 'utf8'), privateKeyPem).toString('base64')

writeFileSync(join(vppDir, SIGNATURE_FILENAME), JSON.stringify({ ...payload, signature }, null, 2) + '\n', 'utf8')
console.log(`signature.json generado en: ${join(vppDir, SIGNATURE_FILENAME)}`)
console.log(`keyId: ${keyId}`)
console.log('Recuerda: OpenPLC Editor stock solo aceptara la firma si confia en la llave publica correspondiente.')
