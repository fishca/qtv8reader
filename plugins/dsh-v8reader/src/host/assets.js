/**
 * Static assets of the form renderer and the one page that injects a model.
 *
 * The page under `assets/formpreview` is the application's own exact-form
 * renderer. Its `model.html` normally fetches `model.json` over HTTP; here the
 * Host puts the model into the page instead, so a form needs one request and a
 * CLI failure becomes a real error status rather than JSON painted inside an
 * iframe. Everything else is read from disk with a traversal guard.
 */
import path from 'node:path'
import { readFile, stat } from 'node:fs/promises'
import { ERROR_CODES, RouteError } from './errors.js'

/** URL prefix of the static surface, under the plugin's route prefix. */
export const ASSETS_ROUTE = '/assets'

/** Extension → content type of the renderer's files. */
export const ASSET_MIME = Object.freeze({
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.gif': 'image/gif',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.webp': 'image/webp',
  '.ico': 'image/x-icon',
  '.woff': 'font/woff',
  '.woff2': 'font/woff2',
  '.ttf': 'font/ttf'
})

/**
 * Content type of one asset path.
 * @param {string} file - path or name.
 * @returns {string} the MIME type, `application/octet-stream` when unknown.
 */
export function assetMimeType(file) {
  return ASSET_MIME[path.extname(file).toLowerCase()] ?? 'application/octet-stream'
}

/**
 * Map one URL-relative asset path to an absolute file inside the root.
 *
 * The path arrives percent-encoded from the URL. It is decoded once, then
 * rejected when it is empty, carries a NUL, looks like a Windows path, or
 * escapes the root after resolution. Symlinks are not resolved, matching the
 * rest of the plugin: the renderer directory is ours and read-only.
 * @param {string} rawPath - the part after `/v8reader/assets/`.
 * @param {string} root - absolute directory of the assets.
 * @returns {string} the absolute file path.
 * @throws {RouteError} `bad-request` for a path outside the root.
 */
export function resolveAssetPath(rawPath, root) {
  if (typeof rawPath !== 'string' || rawPath === '') {
    throw new RouteError(ERROR_CODES.badRequest, 'Не указан файл ресурса', 400)
  }
  let decoded
  try {
    decoded = decodeURIComponent(rawPath)
  } catch {
    throw new RouteError(ERROR_CODES.badRequest, 'Некорректный путь ресурса', 400, {
      path: rawPath
    })
  }
  if (decoded.includes('\u0000') || decoded.includes('\\') || /^[a-zA-Z]:/.test(decoded)) {
    throw new RouteError(ERROR_CODES.badRequest, 'Некорректный путь ресурса', 400, {
      path: decoded
    })
  }
  const base = path.resolve(root)
  const full = path.resolve(base, decoded)
  if (full !== base && !full.startsWith(base + path.sep)) {
    throw new RouteError(ERROR_CODES.badRequest, 'Ресурс вне каталога плагина', 400, {
      path: decoded
    })
  }
  return full
}

/**
 * Read one asset.
 * @param {string} assetsDir - absolute directory of the renderer assets.
 * @param {string} rawPath - the URL path after the assets prefix.
 * @param {{ readFileImpl?: typeof readFile, statImpl?: typeof stat }} [options] - test seams.
 * @returns {Promise<{ body: Buffer, mime: string, size: number, mtimeMs: number }>} the file.
 * @throws {RouteError} `assets-missing` when the directory was never synced, `not-found` for a missing file.
 */
export async function readAsset(assetsDir, rawPath, { readFileImpl = readFile, statImpl = stat } = {}) {
  if (typeof assetsDir !== 'string' || assetsDir === '') {
    throw new RouteError(
      ERROR_CODES.assetsMissing,
      'Ресурсы рендерера формы не установлены: выполните npm run assets',
      503
    )
  }
  let rootInfo
  try {
    rootInfo = await statImpl(assetsDir)
  } catch {
    throw new RouteError(
      ERROR_CODES.assetsMissing,
      'Каталог ресурсов рендерера формы отсутствует: выполните npm run assets',
      503,
      { dir: assetsDir }
    )
  }
  if (!rootInfo.isDirectory()) {
    throw new RouteError(
      ERROR_CODES.assetsMissing,
      'Путь ресурсов рендерера формы не является каталогом',
      503,
      { dir: assetsDir }
    )
  }

  const file = resolveAssetPath(rawPath, assetsDir)
  let info
  try {
    info = await statImpl(file)
  } catch {
    throw new RouteError(ERROR_CODES.notFound, `Ресурс не найден: ${rawPath}`, 404, {
      path: rawPath
    })
  }
  if (!info.isFile()) {
    throw new RouteError(ERROR_CODES.notFound, `Ресурс не найден: ${rawPath}`, 404, {
      path: rawPath
    })
  }
  const body = await readFileImpl(file)
  return {
    body,
    mime: assetMimeType(file),
    size: info.size,
    mtimeMs: Math.trunc(info.mtimeMs)
  }
}

/**
 * Make a JSON value safe to inline in a `<script>` element.
 *
 * `</script>` inside a form caption would otherwise end the script element and
 * break the page; `<` is escaped as `\u003c`, which is still valid JSON/JS.
 * @param {unknown} value - the value to serialize.
 * @returns {string} a JavaScript expression that evaluates to the value.
 */
export function inlineJson(value) {
  return JSON.stringify(value)
    .replace(/</g, '\\u003c')
    .replace(/\u2028/g, '\\u2028')
    .replace(/\u2029/g, '\\u2029')
}

/** Marker the page template has to carry for the model script. */
const PAGE_SCRIPT_MARKER = '<script src="js/model-app.js"></script>'

/**
 * Build the form page from the renderer's `model.html` template.
 *
 * The `<base>` goes immediately after `<head>`, before the template's relative
 * stylesheet links: a `<base>` later in the head does not apply retroactively,
 * so the styles would be requested from `/v8reader/css/…` and the form would
 * paint unstyled. `base` makes every relative `css/…`, `js/…` and `icons.svg`
 * URL resolve under the assets route even though the page itself is served from
 * `/form`. The model goes before `model-app.js`, which reads
 * `window.__QV8_MODEL__` on its first synchronous step and would otherwise
 * fetch `model.json`.
 * @param {string} template - contents of `model.html`.
 * @param {object} model - the `form-model` document.
 * @param {{ baseHref: string }} options - absolute path of the assets directory on the origin.
 * @returns {string} the page HTML.
 */
export function buildFormPage(template, model, { baseHref }) {
  const html = String(template)
  const head = /<head[^>]*>/i.exec(html)
  if (head === null) {
    throw new RouteError(ERROR_CODES.assetsMissing, 'Шаблон страницы формы повреждён: нет <head>', 503)
  }
  const baseTag = `\n<base href="${escapeAttribute(baseHref)}">`
  const withBase = html.slice(0, head.index + head[0].length) + baseTag + html.slice(head.index + head[0].length)

  const modelScript = `<script>window.__QV8_MODEL__ = ${inlineJson(model)};</script>\n`
  if (!withBase.includes(PAGE_SCRIPT_MARKER)) {
    throw new RouteError(
      ERROR_CODES.assetsMissing,
      'Шаблон страницы формы повреждён: нет подключения model-app.js',
      503
    )
  }
  return withBase.replace(PAGE_SCRIPT_MARKER, `${modelScript}${PAGE_SCRIPT_MARKER}`)
}

/**
 * Escape one HTML attribute value.
 * @param {string} value - the attribute text.
 * @returns {string} the escaped text.
 */
function escapeAttribute(value) {
  return String(value)
    .replace(/&/g, '&amp;')
    .replace(/"/g, '&quot;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
}
