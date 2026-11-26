/**
 * One directory level for the panel's own file chooser.
 *
 * A browser file input cannot reveal the absolute path it picked, and the panel
 * needs one: the Host reads the container from disk, not the page. So the
 * chooser asks the Host for a directory listing and walks it one level at a
 * time. Everything here is read-only — `readdir` and `stat`, never a write.
 */
import path from 'node:path'
import { homedir } from 'node:os'
import { readdirSync, statSync } from 'node:fs'
import { ERROR_CODES, RouteError } from './errors.js'
import { SUPPORTED_EXTENSIONS, inspectPathValue } from './paths.js'

/** Entries answered per directory; the panel's filter covers the rest. */
export const MAX_BROWSE_ENTRIES = 1000

/** Selectable files whose size is probed in one answer; the rest report none. */
const MAX_SIZE_PROBES = 300

/** Names never shown: unreadable for a user or plain noise. */
const SKIP_NAMES = new Set(['$RECYCLE.BIN', 'System Volume Information'])

/**
 * The `path` module of the Host platform, so `path` answers match what the
 * platform's filesystem accepts even when a test asks about another platform.
 * @param {NodeJS.Platform} platform - Host platform.
 * @returns {typeof path.win32 | typeof path.posix} the matching implementation.
 */
function pathImpl(platform) {
  return platform === 'win32' ? path.win32 : path.posix
}

/**
 * Map a filesystem failure to the panel's error vocabulary.
 * @param {unknown} error - the thrown value.
 * @param {string} target - the path that failed.
 * @returns {RouteError} a `bad-path` (404/400) or `denied` (403) failure.
 */
function fsFailure(error, target) {
  const code = error !== null && typeof error === 'object' ? error.code : undefined
  if (code === 'ENOENT' || code === 'ENOTDIR') {
    return new RouteError(ERROR_CODES.badPath, `Папка не найдена: ${target}`, 404, {
      path: target,
      reason: 'directory-missing'
    })
  }
  if (code === 'EACCES' || code === 'EPERM') {
    return new RouteError(ERROR_CODES.denied, `Нет доступа к папке: ${target}`, 403, {
      path: target
    })
  }
  return new RouteError(ERROR_CODES.badPath, `Не удалось прочитать папку: ${target}`, 400, {
    path: target,
    reason: 'read-failed'
  })
}

/**
 * Resolve the requested directory.
 *
 * A missing value means the Host home directory, which is where the chooser
 * starts the first time. A file path is accepted and answered by its own
 * directory, so the panel can seed the chooser from the opened document.
 * @param {unknown} value - the `path` query parameter.
 * @param {{ stat?: typeof statSync, platform?: NodeJS.Platform, home?: string }} options - the Host facts.
 * @returns {string} the absolute directory to list.
 * @throws {RouteError} when the path is not absolute, missing, or not a directory.
 */
export function resolveDirectory(value, { stat = statSync, platform = process.platform, home } = {}) {
  const requested = value === null || value === undefined || value === ''
    ? (home ?? homedir())
    : inspectPathValue(value, { platform })

  let info
  try {
    info = stat(requested)
  } catch (error) {
    throw fsFailure(error, requested)
  }

  if (info.isFile()) return pathImpl(platform).dirname(requested)
  if (info.isDirectory()) return requested
  throw new RouteError(ERROR_CODES.badPath, `Это не папка: ${requested}`, 400, {
    path: requested,
    reason: 'not-a-directory'
  })
}

/**
 * Ancestors from the filesystem root down to the directory, for the chooser's
 * breadcrumb row.
 * @param {string} dir - an absolute directory.
 * @param {NodeJS.Platform} platform - Host platform.
 * @returns {Array<{ name: string, path: string }>} root first, directory last.
 */
export function breadcrumbsOf(dir, platform = process.platform) {
  const paths = pathImpl(platform)
  const crumbs = []
  let current = paths.normalize(dir)
  for (let guard = 0; guard < 64; guard += 1) {
    const parent = paths.dirname(current)
    if (parent === current) {
      crumbs.unshift({ name: current, path: current })
      break
    }
    crumbs.unshift({ name: paths.basename(current), path: current })
    current = parent
  }
  return crumbs
}

/**
 * Whether a name should be hidden from the chooser at all.
 * @param {string} name - an entry name.
 * @returns {boolean} true for dot-files and well-known system directories.
 */
function isSkippedName(name) {
  return name.startsWith('.') || SKIP_NAMES.has(name)
}

/**
 * List one directory level.
 * @param {unknown} value - the `path` query parameter.
 * @param {{
 *   all?: boolean,
 *   readdir?: typeof readdirSync,
 *   stat?: typeof statSync,
 *   platform?: NodeJS.Platform,
 *   home?: string
 * }} [options] - what to list and the Host facts.
 * @returns {{
 *   schema: 1,
 *   dir: string,
 *   parent: string | null,
 *   breadcrumbs: Array<{ name: string, path: string }>,
 *   entries: Array<{ name: string, path: string, kind: 'dir' | 'file', size?: number }>,
 *   truncated: boolean,
 *   hiddenCount: number,
 *   all: boolean,
 *   supported: string[]
 * }} the listing document the panel renders.
 * @throws {RouteError} when the path is not an absolute readable directory.
 */
export function listDirectory(value, {
  all = false,
  readdir = readdirSync,
  stat = statSync,
  platform = process.platform,
  home
} = {}) {
  const paths = pathImpl(platform)
  const dir = resolveDirectory(value, { stat, platform, home })

  let dirents
  try {
    dirents = readdir(dir, { withFileTypes: true })
  } catch (error) {
    throw fsFailure(error, dir)
  }

  const entries = []
  let hiddenCount = 0
  let truncated = false
  let probes = 0

  for (const dirent of dirents) {
    if (entries.length >= MAX_BROWSE_ENTRIES) {
      truncated = true
      break
    }
    const name = typeof dirent.name === 'string' ? dirent.name : String(dirent.name)
    if (isSkippedName(name)) continue

    const childPath = paths.join(dir, name)
    let kind = dirent.isDirectory() ? 'dir' : dirent.isFile() ? 'file' : ''

    // Links and reparse points carry no kind of their own: resolve and skip
    // whatever is neither a directory nor a regular file.
    if (kind === '') {
      let info
      try {
        info = stat(childPath)
      } catch {
        continue
      }
      kind = info.isDirectory() ? 'dir' : info.isFile() ? 'file' : ''
      if (kind === '') continue
    }

    const entry = { name, path: childPath, kind }
    if (kind === 'file') {
      const extension = paths.extname(name).toLowerCase()
      const selectable = SUPPORTED_EXTENSIONS.includes(extension)
      if (!selectable && !all) {
        hiddenCount += 1
        continue
      }
      if (selectable && probes < MAX_SIZE_PROBES) {
        probes += 1
        try {
          entry.size = stat(childPath).size
        } catch {
          // A size is a convenience; an unreadable file still lists by name.
        }
      }
    }
    entries.push(entry)
  }

  entries.sort((left, right) => {
    if (left.kind !== right.kind) return left.kind === 'dir' ? -1 : 1
    return left.name.localeCompare(right.name, 'ru', { numeric: true, sensitivity: 'base' })
  })

  const parent = paths.dirname(dir)
  return {
    schema: 1,
    dir,
    parent: parent === dir ? null : parent,
    breadcrumbs: breadcrumbsOf(dir, platform),
    entries,
    truncated,
    hiddenCount,
    all,
    supported: [...SUPPORTED_EXTENSIONS]
  }
}
