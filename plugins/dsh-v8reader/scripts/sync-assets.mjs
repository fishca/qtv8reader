/**
 * Copy the application's exact-form renderer into the plugin package.
 *
 * The plugin never imports `resources/` at runtime: the page, its scripts,
 * styles and standard pictures are copied here so an installed package is
 * self-contained. The copy is committed, so installing the plugin does not
 * need this script; run it after the renderer in `resources/formpreview`
 * changes.
 */
import { cp, mkdir, readdir, rm, stat } from 'node:fs/promises'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const here = path.dirname(fileURLToPath(import.meta.url))
const pluginDir = path.resolve(here, '..')
const source = path.resolve(pluginDir, '..', '..', 'resources', 'formpreview')
const target = path.join(pluginDir, 'assets', 'formpreview')

/**
 * Count files under a directory.
 * @param {string} dir - the directory.
 * @returns {Promise<number>} the number of regular files.
 */
async function countFiles(dir) {
  let total = 0
  for (const entry of await readdir(dir, { withFileTypes: true })) {
    if (entry.isDirectory()) total += await countFiles(path.join(dir, entry.name))
    else total += 1
  }
  return total
}

try {
  const info = await stat(source)
  if (!info.isDirectory()) throw new Error(`${source} is not a directory`)
} catch (error) {
  // A standalone plugin package keeps the committed assets and simply cannot
  // refresh them; that is not a build failure.
  console.warn(`sync-assets: ${source} is unavailable (${error.code ?? error.message}); keeping the committed copy`)
  process.exit(0)
}

await rm(target, { recursive: true, force: true })
await mkdir(target, { recursive: true })
await cp(source, target, { recursive: true })
console.log(`sync-assets: ${await countFiles(target)} files -> ${path.relative(pluginDir, target)}`)
