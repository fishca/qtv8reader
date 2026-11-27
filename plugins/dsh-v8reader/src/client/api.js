/**
 * The panel's only Host contact: `GET /v8reader/state` and
 * `GET /v8reader/browse`.
 *
 * Both routes answer plain JSON, so this module stays a thin wrapper that turns
 * a failed response into one error shape the panel can localize by `code`.
 */

/** A failed panel request with the Host's stable code. */
export class StateError extends Error {
  /**
   * @param {string} code - the Host error code.
   * @param {string} message - the Host message.
   * @param {number} status - the HTTP status.
   * @param {object} [detail] - extra facts, for example the searched CLI paths.
   */
  constructor(code, message, status, detail) {
    super(message)
    this.name = 'StateError'
    this.code = code
    this.status = status
    this.detail = detail
  }
}

/**
 * Read one JSON answer, turning a refusal into a `StateError`.
 * @param {Response} response - the Host answer.
 * @param {string} fallbackCode - code for a body that is not the contract.
 * @returns {Promise<object>} the parsed body of a successful answer.
 * @throws {StateError} when the Host refused or the body is not JSON.
 */
async function readJson(response, fallbackCode) {
  let body = null
  try {
    body = await response.json()
  } catch {
    body = null
  }

  if (!response.ok || body === null) {
    const failure = body?.error ?? {}
    throw new StateError(
      typeof failure.code === 'string' ? failure.code : fallbackCode,
      typeof failure.message === 'string' ? failure.message : `HTTP ${response.status}`,
      response.status,
      failure.detail
    )
  }
  return body
}

/**
 * Build a state route URL.
 * @param {string} path - absolute container path.
 * @param {string} [object] - object query (GUID, name, or synonym).
 * @param {string} [module] - module name.
 * @returns {string} a same-origin URL with every value encoded once.
 */
export function buildStateUrl(path, object = '', module = '') {
  const params = new URLSearchParams({ path })
  if (object !== '') params.set('object', object)
  if (module !== '') params.set('module', module)
  return `/v8reader/state?${params.toString()}`
}

/**
 * Build a browse route URL.
 * @param {string} [dir] - absolute directory, or a file path the Host reduces to its directory; omitted means the Host home.
 * @param {boolean} [all] - whether unsupported files are listed too.
 * @returns {string} a same-origin URL with every value encoded once.
 */
export function buildBrowseUrl(dir = '', all = false) {
  const params = new URLSearchParams()
  if (dir !== '') params.set('path', dir)
  if (all) params.set('all', '1')
  const search = params.toString()
  return search === '' ? '/v8reader/browse' : `/v8reader/browse?${search}`
}

/**
 * Build the form page URL.
 *
 * The Host answers with a complete page (renderer assets plus the inlined
 * model), so the panel can point an iframe at it and needs no second request.
 * @param {string} path - absolute container path.
 * @param {string} object - object query (GUID, name, or synonym).
 * @param {string} form - form name or synonym.
 * @returns {string} a same-origin URL with every value encoded once.
 */
export function buildFormUrl(path, object, form) {
  const params = new URLSearchParams({ path, object, form })
  return `/v8reader/form?${params.toString()}`
}

/**
 * Build the exported table document (MXL) page URL.
 *
 * Like the form page, the Host answers with one complete self-contained page;
 * the panel scales the loaded document itself, so zoom is not a query value.
 * @param {string} path - absolute container path.
 * @param {string} object - object query (GUID, name, or synonym).
 * @param {string} template - template name or synonym.
 * @returns {string} a same-origin URL with every value encoded once.
 */
export function buildTemplateUrl(path, object, template) {
  const params = new URLSearchParams({ path, object, template })
  return `/v8reader/mxl?${params.toString()}`
}

/**
 * Fetch one panel document.
 * @param {{
 *   path: string,
 *   object?: string,
 *   module?: string,
 *   signal?: AbortSignal,
 *   noStore?: boolean,
 *   fetchImpl?: typeof fetch
 * }} request - what to read.
 * @returns {Promise<object>} the `view --json` document (`schema: 1`).
 * @throws {StateError} when the Host refuses or the body is not the contract.
 */
export async function fetchState({
  path,
  object = '',
  module = '',
  signal,
  noStore = false,
  fetchImpl = globalThis.fetch
}) {
  const response = await fetchImpl(buildStateUrl(path, object, module), {
    signal,
    cache: noStore ? 'no-store' : 'default',
    headers: { accept: 'application/json' }
  })
  const body = await readJson(response, 'cli-failed')
  if (body.schema !== 1) {
    throw new StateError('cli-failed', `unsupported schema: ${String(body.schema)}`, response.status)
  }
  return body
}

/**
 * Fetch one directory level for the file chooser.
 * @param {{
 *   dir?: string,
 *   all?: boolean,
 *   signal?: AbortSignal,
 *   fetchImpl?: typeof fetch
 * }} request - the directory to list.
 * @returns {Promise<object>} the listing document (`schema: 1`).
 * @throws {StateError} when the Host refuses or the body is not the contract.
 */
export async function fetchBrowse({
  dir = '',
  all = false,
  signal,
  fetchImpl = globalThis.fetch
}) {
  const response = await fetchImpl(buildBrowseUrl(dir, all), {
    signal,
    cache: 'no-store',
    headers: { accept: 'application/json' }
  })
  const body = await readJson(response, 'bad-path')
  if (body.schema !== 1 || typeof body.dir !== 'string' || !Array.isArray(body.entries)) {
    throw new StateError('bad-path', 'browse answer is not the contract', response.status)
  }
  return body
}
