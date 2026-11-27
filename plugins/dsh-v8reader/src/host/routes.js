/**
 * The one HTTP surface of the plugin: `GET /v8reader/state`,
 * `GET /v8reader/browse`, `GET /v8reader/form`, `GET /v8reader/mxl`, and
 * `GET /v8reader/health`, registered as a prefix route on the Host.
 *
 * The resource is Fetch-shaped (`Request` in, `Response` out) because that is
 * what `ctx.webServer.register({ kind: 'prefix', resource })` accepts. Routes
 * registered this way are not CORS-enabled and are served to same-origin
 * requests only; a cross-site `Sec-Fetch-Site` is refused here on top of that.
 */
import path from 'node:path'
import { ASSETS_ROUTE, buildFormPage, readAsset } from './assets.js'
import { listDirectory } from './browse.js'
import { abortError } from './cli.js'
import { ERROR_CODES, RouteError, cliFailure } from './errors.js'
import {
  fileVersion,
  formCacheKey,
  inspectPathValue,
  inspectTarget,
  mxlCacheKey,
  selectionTag,
  stateCacheKey
} from './paths.js'

/** Prefix the plugin owns. */
export const ROUTE_PREFIX = '/v8reader'

/** Route of the ready-to-show form page (inline model, assets via `<base>`). */
export const FORM_ROUTE = '/form'

/** Route of the exported table document (MXL) page. */
export const TEMPLATE_ROUTE = '/mxl'

/** JSON response of every failure and of the health probe. */
const JSON_HEADERS = Object.freeze({
  'content-type': 'application/json; charset=utf-8',
  'cache-control': 'no-store'
})

/**
 * Headers of an exported table document. The page is the renderer's whole
 * world — one table with inline styles and no scripts — so the policy allows
 * inline styles and embedded images and nothing else.
 */
const PAGE_HEADERS = Object.freeze({
  'content-type': 'text/html; charset=utf-8',
  'cache-control': 'no-store',
  'content-security-policy': "default-src 'none'; style-src 'unsafe-inline'; img-src data:; font-src data:",
  'x-content-type-options': 'nosniff'
})

/**
 * Headers of a renderer asset. Revalidated instead of immutable: a plugin
 * upgrade changes these files under the same URLs, and a year-long immutable
 * entry would keep old scripts next to a new page.
 */
const ASSET_HEADERS = Object.freeze({
  'cache-control': 'no-cache'
})

/**
 * Build a JSON response.
 * @param {number} status - HTTP status.
 * @param {unknown} body - JSON-safe body.
 * @param {Record<string, string>} [headers] - extra headers.
 * @returns {Response} the response.
 */
function jsonResponse(status, body, headers = {}) {
  return new Response(JSON.stringify(body), { status, headers: { ...JSON_HEADERS, ...headers } })
}

/**
 * Build the answer for one thrown failure.
 * @param {unknown} error - the thrown value.
 * @returns {Response} a JSON error response.
 */
export function errorResponse(error) {
  if (error instanceof RouteError) {
    const body = {
      error: {
        code: error.code,
        message: error.message,
        ...(error.detail === undefined ? {} : { detail: error.detail })
      }
    }
    const headers = {}
    if (error.code === ERROR_CODES.methodNotAllowed) headers.allow = 'GET, HEAD'
    return jsonResponse(error.status, body, headers)
  }
  const message = error instanceof Error ? error.message : String(error)
  return jsonResponse(500, { error: { code: ERROR_CODES.cliFailed, message } })
}

/**
 * Whether a request may read the plugin's routes.
 * @param {Request} request - the incoming request.
 * @returns {boolean} false for an explicit foreign fetch site.
 */
export function sameOriginRequest(request) {
  const site = request.headers.get('sec-fetch-site')
  if (site === null) return true
  return site === 'same-origin' || site === 'none'
}

/**
 * Create the route resource.
 * @param {{
 *   cli: ReturnType<import('./cli.js').createCli>,
 *   cache: ReturnType<import('./cache.js').createCache>,
 *   settings: object,
 *   stat?: unknown,
 *   readdir?: unknown,
 *   home?: string,
 *   platform?: NodeJS.Platform,
 *   assetsDir?: string
 * }} options - the plugin's parts and test seams.
 * @returns {(request: Request) => Promise<Response>} the resource.
 */
export function createResource({
  cli,
  cache,
  settings,
  stat,
  readdir,
  home,
  platform = process.platform,
  assetsDir = ''
}) {
  /**
   * Answer `GET /v8reader/state`.
   * @param {URL} url - the request URL.
   * @param {boolean} head - whether the body must be dropped.
   * @param {Request} request - the incoming request, for cancellation.
   * @returns {Promise<Response>} the answer.
   */
  async function state(url, head, request) {
    const pathValue = inspectPathValue(url.searchParams.get('path'), { platform })
    const object = (url.searchParams.get('object') ?? '').trim()
    const moduleName = (url.searchParams.get('module') ?? '').trim()
    const file = inspectTarget(pathValue, { stat: stat ?? undefined, platform })

    const version = fileVersion(file)
    const etag = `"${version}-${selectionTag(object, moduleName)}"`
    const key = stateCacheKey(file, object, moduleName)
    const cached = cache.get(key)
    const versionHeaders = {
      etag,
      'x-v8reader-version': version,
      'cache-control': 'no-cache'
    }

    if (cached !== undefined) {
      return new Response(head ? null : cached, {
        status: 200,
        headers: { ...JSON_HEADERS, ...versionHeaders }
      })
    }

    const args = ['view', file.path]
    if (object !== '') args.push(object)
    args.push('--json')
    if (moduleName !== '') args.push('--module', moduleName)

    const outcome = await cli.run(args, {
      cwd: path.dirname(file.path),
      timeoutMs: settings.timeoutMs,
      maxBytes: settings.maxJsonBytes,
      signal: request.signal
    })
    if (outcome.aborted) throw abortError()
    if (outcome.timedOut || outcome.overflow || outcome.code !== 0) {
      throw cliFailure(outcome)
    }

    // The contract is `schema: 1`; anything else is a CLI/plugin mismatch,
    // reported as a CLI failure instead of handing the panel unknown JSON.
    let parsed
    try {
      parsed = JSON.parse(outcome.stdout)
    } catch {
      throw new RouteError(
        ERROR_CODES.cliFailed,
        'v8reader_cli вернул не JSON',
        502,
        { bytes: outcome.stdout.length }
      )
    }
    if (parsed === null || typeof parsed !== 'object' || parsed.schema !== 1) {
      throw new RouteError(
        ERROR_CODES.cliFailed,
        'v8reader_cli вернул ответ неизвестной схемы',
        502,
        { schema: parsed?.schema ?? null }
      )
    }

    cache.set(key, outcome.stdout)
    return new Response(head ? null : outcome.stdout, {
      status: 200,
      headers: { ...JSON_HEADERS, ...versionHeaders }
    })
  }

  /**
   * Answer `GET /v8reader/health`.
   * @returns {Promise<Response>} the answer; 200 even when the CLI is missing.
   */
  async function health() {
    const body = {
      ok: false,
      schema: 1,
      route: ROUTE_PREFIX,
      minCliVersion: settings.minCliVersion
    }
    try {
      const resolvedCli = await cli.resolve()
      body.ok = true
      body.cli = {
        found: true,
        path: resolvedCli.path,
        version: resolvedCli.version,
        searched: resolvedCli.searched
      }
    } catch (error) {
      const failure = error instanceof RouteError
        ? error
        : new RouteError(ERROR_CODES.cliMissing, String(error?.message ?? error), 503)
      body.cli = {
        found: false,
        code: failure.code,
        message: failure.message,
        ...(failure.detail === undefined ? {} : { detail: failure.detail }),
        searched: failure.detail?.searched ?? cli.searchedCandidates()
      }
    }
    return jsonResponse(200, body)
  }

  /**
   * Answer `GET /v8reader/browse`.
   * @param {URL} url - the request URL.
   * @param {boolean} head - whether the body must be dropped.
   * @returns {Promise<Response>} the listing of one directory level.
   */
  async function browseRoute(url, head) {
    const body = await listDirectory(url.searchParams.get('path'), {
      all: url.searchParams.get('all') === '1',
      ...(readdir === undefined ? {} : { readdir }),
      ...(stat === undefined ? {} : { stat }),
      ...(home === undefined ? {} : { home }),
      platform
    })
    const response = jsonResponse(200, body)
    if (!head) return response
    return new Response(null, { status: response.status, headers: response.headers })
  }

  /**
   * Run one CLI argument vector for the form model.
   * @param {{ path: string }} file - inspected container.
   * @param {string} object - object query.
   * @param {string} form - form name or synonym.
   * @param {AbortSignal} [signal] - aborted when the panel disconnects.
   * @returns {Promise<object>} the validated `form-model` document.
   * @throws {RouteError} any CLI or contract failure.
   */
  async function readFormModel(file, object, form, signal) {
    const outcome = await cli.run(['form-model', file.path, object, '--form', form], {
      cwd: path.dirname(file.path),
      timeoutMs: settings.timeoutMs,
      maxBytes: settings.maxJsonBytes,
      signal
    })
    if (outcome.aborted) throw abortError()
    if (outcome.timedOut || outcome.overflow || outcome.code !== 0) {
      throw cliFailure(outcome)
    }
    let parsed
    try {
      parsed = JSON.parse(outcome.stdout)
    } catch {
      throw new RouteError(
        ERROR_CODES.cliFailed,
        'v8reader_cli вернул не JSON модели формы',
        502,
        { bytes: outcome.stdout.length }
      )
    }
    if (parsed === null || typeof parsed !== 'object' || typeof parsed.tag !== 'string') {
      throw new RouteError(
        ERROR_CODES.cliFailed,
        'v8reader_cli вернул модель формы без поля tag',
        502
      )
    }
    return parsed
  }

  /**
   * Answer `GET /v8reader/form`.
   *
   * The page is the renderer's own `model.html` with the model injected before
   * `model-app.js` reads it, so one request yields either a complete page or a
   * JSON error. A model cached for the file generation skips the CLI call; the
   * page itself is rebuilt because it is cheap and must carry the new inline
   * copy when another form of the same file is asked for.
   * @param {URL} url - the request URL.
   * @param {boolean} head - whether the body must be dropped.
   * @param {Request} request - the incoming request, for cancellation.
   * @returns {Promise<Response>} the page or a failure.
   */
  async function formRoute(url, head, request) {
    const pathValue = inspectPathValue(url.searchParams.get('path'), { platform })
    const object = (url.searchParams.get('object') ?? '').trim()
    const form = (url.searchParams.get('form') ?? '').trim()
    if (object === '') {
      throw new RouteError(ERROR_CODES.badRequest, 'Параметр object обязателен для формы', 400)
    }
    if (form === '') {
      throw new RouteError(ERROR_CODES.badRequest, 'Параметр form обязателен', 400)
    }
    const file = inspectTarget(pathValue, { stat: stat ?? undefined, platform })

    const key = formCacheKey(file, object, form)
    const cached = cache.get(key)
    const model = cached === undefined
      ? await readFormModel(file, object, form, request.signal)
      : JSON.parse(cached)
    if (cached === undefined) {
      cache.set(key, JSON.stringify(model))
    }

    const template = await readAsset(assetsDir, 'model.html')
    const page = buildFormPage(template.body.toString('utf8'), model, {
      baseHref: `${ROUTE_PREFIX}${ASSETS_ROUTE}/`
    })
    return new Response(head ? null : page, {
      status: 200,
      headers: {
        'content-type': 'text/html; charset=utf-8',
        'cache-control': 'no-store',
        'x-v8reader-version': fileVersion(file)
      }
    })
  }

  /**
   * Run one CLI argument vector for an exported table document.
   * @param {{ path: string }} file - inspected container.
   * @param {string} object - object query.
   * @param {string} template - template name or synonym.
   * @param {AbortSignal} [signal] - aborted when the panel disconnects.
   * @returns {Promise<string>} the self-contained HTML page.
   * @throws {RouteError} any CLI or contract failure.
   */
  async function readTemplateHtml(file, object, template, signal) {
    const outcome = await cli.run(['mxl', file.path, object, '--template', template, '--html', '-'], {
      cwd: path.dirname(file.path),
      timeoutMs: settings.timeoutMs,
      maxBytes: settings.maxJsonBytes,
      signal
    })
    if (outcome.aborted) throw abortError()
    if (outcome.timedOut || outcome.overflow || outcome.code !== 0) {
      throw cliFailure(outcome)
    }
    // The exporter's page starts with a doctype; anything else is not the
    // renderer's answer and is reported instead of being painted in a frame.
    if (!/^\s*(?:<!doctype\s+html|<html)/i.test(outcome.stdout)) {
      throw new RouteError(
        ERROR_CODES.cliFailed,
        'v8reader_cli вернул не HTML табличного документа',
        502,
        { bytes: outcome.stdout.length }
      )
    }
    return outcome.stdout
  }

  /**
   * Answer `GET /v8reader/mxl`.
   *
   * The page comes from `mxl --html -`: a self-contained table document with
   * inline styles, so one request yields either a complete page or a JSON
   * error. The panel shows it in a sandboxed frame and scales the frame's
   * document itself, so the page is cached per file generation without the
   * zoom baked in.
   * @param {URL} url - the request URL.
   * @param {boolean} head - whether the body must be dropped.
   * @param {Request} request - the incoming request, for cancellation.
   * @returns {Promise<Response>} the page or a failure.
   */
  async function templateRoute(url, head, request) {
    const pathValue = inspectPathValue(url.searchParams.get('path'), { platform })
    const object = (url.searchParams.get('object') ?? '').trim()
    const template = (url.searchParams.get('template') ?? '').trim()
    if (object === '') {
      throw new RouteError(ERROR_CODES.badRequest, 'Параметр object обязателен для макета', 400)
    }
    if (template === '') {
      throw new RouteError(ERROR_CODES.badRequest, 'Параметр template обязателен', 400)
    }
    const file = inspectTarget(pathValue, { stat: stat ?? undefined, platform })

    const key = mxlCacheKey(file, object, template)
    let html = cache.get(key)
    if (html === undefined) {
      html = await readTemplateHtml(file, object, template, request.signal)
      cache.set(key, html)
    }
    return new Response(head ? null : html, {
      status: 200,
      headers: { ...PAGE_HEADERS, 'x-v8reader-version': fileVersion(file) }
    })
  }

  /**
   * Answer `GET /v8reader/assets/<path>` with one renderer file.
   * @param {string} rawPath - the path after the assets route.
   * @param {Request} request - the incoming request, for revalidation.
   * @param {boolean} head - whether the body must be dropped.
   * @returns {Promise<Response>} the file, 304, or a failure.
   */
  async function assetRoute(rawPath, request, head) {
    const asset = await readAsset(assetsDir, rawPath)
    const etag = `"${asset.mtimeMs.toString(36)}-${asset.size.toString(36)}"`
    const headers = {
      'content-type': asset.mime,
      etag,
      'content-length': String(asset.size),
      ...ASSET_HEADERS
    }
    if (request.headers.get('if-none-match') === etag) {
      return new Response(null, { status: 304, headers })
    }
    return new Response(head ? null : asset.body, { status: 200, headers })
  }

  return async function resource(request) {
    try {
      const method = request.method.toUpperCase()
      if (method !== 'GET' && method !== 'HEAD') {
        throw new RouteError(
          ERROR_CODES.methodNotAllowed,
          'Поддерживаются только GET и HEAD',
          405
        )
      }
      if (!sameOriginRequest(request)) {
        throw new RouteError(ERROR_CODES.forbidden, 'Запрос с чужого сайта отклонён', 403)
      }

      const url = new URL(request.url)
      const route = url.pathname.slice(ROUTE_PREFIX.length) || '/'
      const head = method === 'HEAD'

      if (route === '/health') return await health()
      if (route === '/browse') return await browseRoute(url, head)
      if (route === FORM_ROUTE) return await formRoute(url, head, request)
      if (route === TEMPLATE_ROUTE) return await templateRoute(url, head, request)
      if (route.startsWith(`${ASSETS_ROUTE}/`)) {
        const rawPath = url.pathname.slice(ROUTE_PREFIX.length + ASSETS_ROUTE.length + 1)
        return await assetRoute(rawPath, request, head)
      }
      if (route === '/state') {
        const ifNoneMatch = request.headers.get('if-none-match')
        const response = await state(url, head, request)
        if (ifNoneMatch !== null && response.headers.get('etag') === ifNoneMatch) {
          return new Response(null, {
            status: 304,
            headers: { etag: ifNoneMatch, 'cache-control': 'no-cache' }
          })
        }
        return response
      }

      throw new RouteError(
        ERROR_CODES.notFound,
        `Неизвестный маршрут: ${url.pathname}`,
        404,
        { routes: [`${ROUTE_PREFIX}/health`, `${ROUTE_PREFIX}/state`, `${ROUTE_PREFIX}/browse`, `${ROUTE_PREFIX}${FORM_ROUTE}`, `${ROUTE_PREFIX}${TEMPLATE_ROUTE}`, `${ROUTE_PREFIX}${ASSETS_ROUTE}/`] }
      )
    } catch (error) {
      return errorResponse(error)
    }
  }
}
