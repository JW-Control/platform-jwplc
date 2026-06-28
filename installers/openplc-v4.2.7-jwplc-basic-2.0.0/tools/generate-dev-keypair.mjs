#!/usr/bin/env node
// Genera una llave Ed25519 de desarrollo para firmar paquetes VPP internos.
// No subir la llave privada al repositorio.

import { generateKeyPairSync } from 'node:crypto'
import { mkdirSync, writeFileSync } from 'node:fs'
import { dirname, resolve } from 'node:path'

const outDir = resolve(process.argv[2] ?? 'keys')
mkdirSync(outDir, { recursive: true })

const { publicKey, privateKey } = generateKeyPairSync('ed25519', {
  publicKeyEncoding: { type: 'spki', format: 'pem' },
  privateKeyEncoding: { type: 'pkcs8', format: 'pem' },
})

const publicPath = resolve(outDir, 'jw-control-dev-public.pem')
const privatePath = resolve(outDir, 'jw-control-dev-private.pem')
mkdirSync(dirname(publicPath), { recursive: true })
writeFileSync(publicPath, publicKey, 'utf8')
writeFileSync(privatePath, privateKey, 'utf8')

console.log(`Llave publica : ${publicPath}`)
console.log(`Llave privada : ${privatePath}`)
console.log('IMPORTANTE: no subir la llave privada al repositorio.')
