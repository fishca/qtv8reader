/**
 * The panel's only Host contact: `GET /v8reader/state`.
 *
 * The route returns the CLI's `view --json` document unchanged, so this module
 * stays a thin wrapper that turns a failed response into one error shape the
 * panel can localize by `code`.
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
 * Build the state route URL.
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

  let body = null
  try {
    body = await response.json()
  } catch {
    body = null
  }

  if (!response.ok || body === null) {
    const failure = body?.error ?? {}
    throw new StateError(
      typeof failure.code === 'string' ? failure.code : 'cli-failed',
      typeof failure.message === 'string' ? failure.message : `HTTP ${response.status}`,
      response.status,
      failure.detail
    )
  }
  if (body.schema !== 1) {
    throw new StateError('cli-failed', `unsupported schema: ${String(body.schema)}`, response.status)
  }
  return body
}
