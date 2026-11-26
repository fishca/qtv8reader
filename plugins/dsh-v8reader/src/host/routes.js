/**
 * The one HTTP surface of the plugin: `GET /v8reader/state`,
 * `GET /v8reader/browse`, and `GET /v8reader/health`, registered as a prefix
 * route on the Host.
 *
 * The resource is Fetch-shaped (`Request` in, `Response` out) because that is
 * what `ctx.webServer.register({ kind: 'prefix', resource })` accepts. Routes
 * registered this way are not CORS-enabled and are served to same-origin
 * requests only; a cross-site `Sec-Fetch-Site` is refused here on top of that.
 */
import path from 'node:path'
import { listDirectory } from './browse.js'
import { ERROR_CODES, RouteError, cliFailure } from './errors.js'
import {
  fileVersion,
  inspectPathValue,
  inspectTarget,
  selectionTag,
  stateCacheKey
} from './paths.js'

/** Prefix the plugin owns. */
export const ROUTE_PREFIX = '/v8reader'

/** JSON response of every failure and of the health probe. */
const JSON_HEADERS = Object.freeze({
  'content-type': 'application/json; charset=utf-8',
  'cache-control': 'no-store'
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
 *   cache: ReturnType<import('./cache.js').createLruCache>,
 *   settings: object,
 *   stat?: unknown,
 *   readdir?: unknown,
 *   home?: string,
 *   platform?: NodeJS.Platform
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
  platform = process.platform
}) {
  /**
   * Answer `GET /v8reader/state`.
   * @param {URL} url - the request URL.
   * @param {boolean} head - whether the body must be dropped.
   * @returns {Promise<Response>} the answer.
   */
  async function state(url, head) {
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
      maxBytes: settings.maxJsonBytes
    })
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
      if (route === '/state') {
        const ifNoneMatch = request.headers.get('if-none-match')
        const response = await state(url, head)
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
        { routes: [`${ROUTE_PREFIX}/health`, `${ROUTE_PREFIX}/state`, `${ROUTE_PREFIX}/browse`] }
      )
    } catch (error) {
      return errorResponse(error)
    }
  }
}
