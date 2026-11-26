/**
 * Build `client.js` from `src/client` with esbuild.
 *
 * The artifact is a CommonJS bundle wrapped in the browser module loader's
 * factory: React comes from the page's module table, everything else is bundled.
 * `client.js` is committed, so installing the plugin never needs this script.
 */
import { build } from 'esbuild'
import { writeFile } from 'node:fs/promises'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const root = path.dirname(fileURLToPath(import.meta.url))
const ID = '@fishca/dsh-v8reader'

const result = await build({
  entryPoints: [path.join(root, 'src', 'client', 'index.jsx')],
  bundle: true,
  write: false,
  format: 'cjs',
  platform: 'browser',
  target: ['chrome120'],
  external: ['react'],
  jsx: 'transform',
  jsxFactory: 'React.createElement',
  jsxFragment: 'React.Fragment',
  legalComments: 'none',
  logLevel: 'warning'
})

const code = result.outputFiles[0].text
const banner = '/* Сгенерировано build.mjs из src/client; правьте исходники. */'
const wrapped = `${banner}
window.__ModuleLoader__.load({
  id: '${ID}',
  factory(require) {
    const module = { exports: {} };
    const exports = module.exports;
${code.replace(/^/gm, '    ')}
    return module.exports;
  },
});
`

await writeFile(path.join(root, 'client.js'), wrapped, 'utf8')

const bytes = Buffer.byteLength(wrapped, 'utf8')
console.log(`client.js: ${bytes} bytes, ${wrapped.split('\n').length} lines`)
