/**
 * The error vocabulary shared by the routes, the CLI runner, and the panel.
 *
 * `code` is the stable contract with the client half: every failure the panel
 * shows has one of these values so the panel can localize the cause and hint,
 * while `message` stays a human-readable fallback from the Host.
 */

/** Stable codes returned in `{ error: { code } }`. */
export const ERROR_CODES = Object.freeze({
  badPath: 'bad-path',
  unsupportedFormat: 'unsupported-format',
  cliFailed: 'cli-failed',
  cliMissing: 'cli-missing',
  cliOld: 'cli-old',
  tooLarge: 'too-large',
  timeout: 'timeout',
  denied: 'denied',
  forbidden: 'forbidden',
  notFound: 'not-found',
  methodNotAllowed: 'method-not-allowed',
  badRequest: 'bad-request',
  assetsMissing: 'assets-missing'
})

/** An HTTP answer with a stable code, the status it maps to, and optional detail. */
export class RouteError extends Error {
  /**
   * @param {string} code - one of {@link ERROR_CODES}.
   * @param {string} message - human-readable fallback.
   * @param {number} status - HTTP status for the answer.
   * @param {object} [detail] - extra JSON-safe facts for the panel.
   */
  constructor(code, message, status, detail) {
    super(message)
    this.name = 'RouteError'
    this.code = code
    this.status = status
    this.detail = detail
  }
}

/**
 * Map a CLI failure to a RouteError.
 * @param {{ code?: number|null, timedOut?: boolean, overflow?: boolean, stderr?: string }} outcome - the run result.
 * @returns {RouteError} the mapped failure.
 */
export function cliFailure(outcome) {
  if (outcome.timedOut) {
    return new RouteError(
      ERROR_CODES.timeout,
      'v8reader_cli не успел ответить и был остановлен',
      504
    )
  }
  if (outcome.overflow) {
    return new RouteError(
      ERROR_CODES.tooLarge,
      'Ответ v8reader_cli больше допустимого размера',
      413
    )
  }
  const stderr = String(outcome.stderr ?? '').trim()
  return new RouteError(
    ERROR_CODES.cliFailed,
    stderr === '' ? 'v8reader_cli завершился с ошибкой' : stderr,
    500,
    { exitCode: outcome.code ?? null }
  )
}
