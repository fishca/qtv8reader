/**
 * Path validation for the one route family that reads arbitrary absolute files.
 *
 * Only a regular file with a 1C container suffix inside an existing absolute
 * path is accepted; the check never resolves symlinks, so a link to a container
 * stays usable, and it never writes.
 */
import path from 'node:path'
import { statSync } from 'node:fs'
import { ERROR_CODES, RouteError } from './errors.js'

/** Container suffixes the panel can open, lowercase. */
export const SUPPORTED_EXTENSIONS = Object.freeze(['.cf', '.cfe', '.epf', '.erf'])

/** Longest accepted path; the OS limits are lower, this only stops abuse. */
const MAX_PATH_LENGTH = 4096

/**
 * Whether a value names an absolute location for the platform.
 * @param {string} value - the typed path.
 * @param {NodeJS.Platform} platform - the Host platform.
 * @returns {boolean} true for `C:\…`, `\\server\share\…`, `\\?\…` and `/…`.
 */
export function isAbsolutePath(value, platform) {
  if (platform === 'win32') {
    if (/^[a-zA-Z]:[\\/]/.test(value)) return true
    if (/^\\\\\?\\/.test(value)) return true
    return /^\\\\[^\\]+\\[^\\]+/.test(value)
  }
  return value.startsWith('/')
}

/**
 * Validate the shape of a path before touching the filesystem.
 * @param {unknown} value - the `path` query parameter.
 * @param {{ platform?: NodeJS.Platform }} [options] - platform of the Host.
 * @returns {string} the path as typed.
 * @throws {RouteError} `bad-path` for a missing, relative, or NUL-carrying value.
 */
export function inspectPathValue(value, { platform = process.platform } = {}) {
  if (typeof value !== 'string') {
    throw new RouteError(ERROR_CODES.badPath, 'Параметр path обязателен', 400)
  }
  if (value === '') {
    throw new RouteError(ERROR_CODES.badPath, 'Параметр path пуст', 400)
  }
  if (value.includes('\u0000')) {
    throw new RouteError(ERROR_CODES.badPath, 'Путь содержит нулевой символ', 400)
  }
  if (value.length > MAX_PATH_LENGTH) {
    throw new RouteError(ERROR_CODES.badPath, 'Путь слишком длинный', 400)
  }
  if (!isAbsolutePath(value, platform)) {
    throw new RouteError(ERROR_CODES.badPath, 'Нужен абсолютный путь к файлу', 400, {
      path: value
    })
  }
  return value
}

/**
 * Check that the path is a regular file with a supported suffix.
 * @param {string} value - an already shape-checked path.
 * @param {{ stat?: typeof statSync, platform?: NodeJS.Platform }} [options] - test seams.
 * @returns {{ path: string, size: number, mtimeMs: number, extension: string }} file facts.
 * @throws {RouteError} `bad-path` for a missing path or a non-file, `unsupported-format` for the suffix.
 */
export function inspectTarget(value, { stat = statSync, platform = process.platform } = {}) {
  let info
  try {
    info = stat(value)
  } catch (error) {
    const missing = error && typeof error === 'object' && error.code === 'ENOENT'
    throw new RouteError(
      ERROR_CODES.badPath,
      missing ? `Файл не найден: ${value}` : `Не удалось прочитать путь: ${value}`,
      missing ? 404 : 400,
      { path: value, reason: missing ? 'ENOENT' : 'stat-failed' }
    )
  }
  if (!info.isFile()) {
    throw new RouteError(ERROR_CODES.badPath, `Это не обычный файл: ${value}`, 400, {
      path: value,
      reason: 'not-a-file'
    })
  }

  const extension = path.extname(value).toLowerCase()
  if (!SUPPORTED_EXTENSIONS.includes(extension)) {
    throw new RouteError(
      ERROR_CODES.unsupportedFormat,
      `Поддерживаются только ${SUPPORTED_EXTENSIONS.join(', ')}`,
      415,
      { path: value, extension, supported: SUPPORTED_EXTENSIONS }
    )
  }

  return {
    path: path.normalize(value),
    size: info.size,
    mtimeMs: Math.trunc(info.mtimeMs),
    extension
  }
}

/**
 * Build the server-side cache key of one panel request.
 * @param {{ path: string, size: number, mtimeMs: number }} file - inspected file facts.
 * @param {string} object - requested object query or an empty string.
 * @param {string} module - requested module name or an empty string.
 * @returns {string} a key that changes when the file or the selection changes.
 */
export function stateCacheKey(file, object, module) {
  return [file.path, file.size, file.mtimeMs, object, module].join('\u0000')
}

/**
 * Version tag of one file generation, sent to the panel and used as its ETag.
 * @param {{ size: number, mtimeMs: number }} file - inspected file facts.
 * @returns {string} an opaque, stable-per-generation tag.
 */
export function fileVersion(file) {
  return `${file.mtimeMs.toString(36)}-${file.size.toString(36)}`
}

/**
 * Short tag of one object/module selection.
 *
 * The ETag of a `state` answer must identify the whole answer, not only the
 * file: two objects of one file share the file generation, and reusing their
 * ETags interchangeably would let a revalidation answer 304 for a different
 * body.
 * @param {string} object - object query or an empty string.
 * @param {string} module - module name or an empty string.
 * @returns {string} a stable short tag of the selection.
 */
export function selectionTag(object, module) {
  const text = `${object}\u0000${module}`
  let hash = 0x811c9dc5
  for (let index = 0; index < text.length; index += 1) {
    hash ^= text.charCodeAt(index)
    hash = Math.imul(hash, 0x01000193) >>> 0
  }
  return hash.toString(36)
}
