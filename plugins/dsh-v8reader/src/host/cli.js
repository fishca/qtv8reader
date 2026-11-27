/**
 * The `v8reader_cli` side of the plugin: where the executable is, what version
 * it reports, and one bounded run at a time.
 *
 * Arguments are always an argv array (never a shell string), every run has a
 * timeout that kills the whole process tree, and stdout above the configured
 * budget ends the run instead of filling memory. Nothing here imports DSH: the
 * module is runnable under plain Node for the tests.
 */
import path from 'node:path'
import { spawn } from 'node:child_process'
import { statSync } from 'node:fs'
import { ERROR_CODES, RouteError } from './errors.js'

/** Smallest CLI release whose `view --json` contract this plugin speaks. */
export const MIN_CLI_VERSION = '0.14.0'

/** stderr kept per run; the panel shows it as the failure text. */
const STDERR_LIMIT = 64 * 1024

/**
 * Extract a dotted version from `v8reader_cli --version` output.
 *
 * The program name is required so another executable named `v8reader_cli` (or
 * an unrelated file that happens to print a version) is not mistaken for ours;
 * a bare version line is accepted for hand-built probes.
 * @param {string} text - command output, for example `v8reader_cli 0.14.0`.
 * @returns {string} the version, or an empty string when absent.
 */
export function parseCliVersion(text) {
  const output = String(text ?? '')
  const named = /v8reader_cli\s+(\d+)\.(\d+)\.(\d+)/i.exec(output)
  const bare = /^\s*(\d+)\.(\d+)\.(\d+)\s*$/.exec(output)
  const match = named ?? bare
  return match === null ? '' : `${match[1]}.${match[2]}.${match[3]}`
}

/**
 * Compare two dotted versions.
 * @param {string} left - a version.
 * @param {string} right - another version.
 * @returns {number} negative when left is older, positive when newer, 0 when equal.
 */
export function compareVersions(left, right) {
  const parse = (value) => String(value).split('.').map((part) => Number.parseInt(part, 10) || 0)
  const a = parse(left)
  const b = parse(right)
  for (let index = 0; index < Math.max(a.length, b.length); index += 1) {
    const difference = (a[index] ?? 0) - (b[index] ?? 0)
    if (difference !== 0) return difference
  }
  return 0
}

/**
 * The executable name searched on `PATH` for one platform.
 * @param {NodeJS.Platform} platform - the Host platform.
 * @returns {string} `v8reader_cli.exe` on Windows, `v8reader_cli` elsewhere.
 */
export function cliExecutableName(platform) {
  return platform === 'win32' ? 'v8reader_cli.exe' : 'v8reader_cli'
}

/**
 * Candidate executable paths in search order, without touching the filesystem.
 *
 * Order: the plugin row's `cliPath`, `V8READER_CLI`, the checkout's build
 * outputs (development convenience for this repository layout), then `PATH`.
 * @param {{
 *   cliPath?: string,
 *   env?: NodeJS.ProcessEnv,
 *   platform?: NodeJS.Platform,
 *   packageDir?: string
 * }} options - search inputs.
 * @returns {string[]} unique candidate paths in the order they are tried.
 */
export function cliCandidates({ cliPath = '', env = process.env, platform = process.platform, packageDir = '' } = {}) {
  const candidates = []
  const push = (value) => {
    if (typeof value !== 'string') return
    const trimmed = value.trim()
    if (trimmed === '' || candidates.includes(trimmed)) return
    candidates.push(trimmed)
  }

  push(cliPath)
  push(env.V8READER_CLI)

  if (packageDir !== '') {
    for (const output of ['cli-deploy', 'mingw', 'vs', 'build']) {
      push(path.resolve(packageDir, '..', '..', 'out', output, cliExecutableName(platform)))
    }
  }

  const pathValue = env.PATH ?? env.Path ?? ''
  for (const directory of String(pathValue).split(path.delimiter)) {
    if (directory.trim() === '') continue
    push(path.join(directory.trim(), cliExecutableName(platform)))
  }
  return candidates
}

/**
 * A counting semaphore that hands its slot to the next waiter on release.
 * @param {number} limit - maximum concurrent holders.
 * @returns {{ acquire: (signal?: AbortSignal) => Promise<void>, release: () => void, readonly active: number }} the semaphore.
 */
export function createSemaphore(limit) {
  let active = 0
  /** @type {Array<() => void>} */
  const waiters = []

  return {
    async acquire(signal) {
      if (signal?.aborted) throw abortError()
      if (active < limit && waiters.length === 0) {
        active += 1
        return
      }
      await new Promise((resolve, reject) => {
        const waiter = () => {
          signal?.removeEventListener('abort', onAbort)
          resolve()
        }
        const onAbort = () => {
          const index = waiters.indexOf(waiter)
          if (index >= 0) waiters.splice(index, 1)
          reject(abortError())
        }
        waiters.push(waiter)
        signal?.addEventListener('abort', onAbort, { once: true })
      })
    },

    release() {
      const next = waiters.shift()
      if (next !== undefined) {
        next()
        return
      }
      active = Math.max(0, active - 1)
    },

    get active() {
      return active
    }
  }
}

/**
 * The rejection of an aborted request.
 * @returns {RouteError} a `timeout`-coded error with the client-closed status.
 */
export function abortError() {
  return new RouteError(ERROR_CODES.timeout, 'Запрос отменён панелью', 499)
}

/**
 * Create the CLI runner.
 * @param {{
 *   settings?: object,
 *   env?: NodeJS.ProcessEnv,
 *   platform?: NodeJS.Platform,
 *   packageDir?: string,
 *   spawn?: typeof spawn,
 *   stat?: typeof statSync,
 *   killTree?: (child: import('node:child_process').ChildProcess) => void
 * }} [options] - test seams and search inputs.
 * @returns the runner: `resolve`, `run`, `dispose`, `searchedCandidates`.
 */
export function createCli({
  settings = {},
  env = process.env,
  platform = process.platform,
  packageDir = '',
  spawn: spawnImpl = spawn,
  stat = statSync,
  killTree = (child) => killProcessTree(child, { platform, spawn: spawnImpl })
} = {}) {
  const timeoutMs = settings.timeoutMs ?? 20000
  const maxJsonBytes = settings.maxJsonBytes ?? 16 * 1024 * 1024
  const minCliVersion = settings.minCliVersion ?? MIN_CLI_VERSION
  const semaphore = createSemaphore(settings.maxConcurrent ?? 2)
  /** @type {Set<import('node:child_process').ChildProcess>} */
  const live = new Set()
  /** @type {Promise<{ path: string, version: string, searched: string[] }> | undefined} */
  let resolving

  const candidates = cliCandidates({ cliPath: settings.cliPath ?? '', env, platform, packageDir })

  const isFile = (candidate) => {
    try {
      return stat(candidate).isFile()
    } catch {
      return false
    }
  }

  /**
   * Run one child process with the byte and time budgets.
   * @param {string} executable - absolute executable path.
   * @param {string[]} args - argv, passed without a shell.
   * @param {{ cwd?: string, timeoutMs?: number, maxBytes?: number, signal?: AbortSignal }} [options] - run bounds.
   * @returns {Promise<object>} the run outcome.
   */
  async function runOnce(executable, args, { cwd, timeoutMs: runTimeout = timeoutMs, maxBytes = maxJsonBytes, signal } = {}) {
    const startedAt = Date.now()
    const stdout = []
    const stderr = []
    let stdoutBytes = 0
    let stderrBytes = 0
    let overflow = false
    let timedOut = false
    let aborted = false
    let settled = false

    const child = spawnImpl(executable, args, {
      cwd,
      windowsHide: true,
      stdio: ['ignore', 'pipe', 'pipe'],
      env
    })
    live.add(child)

    const finish = (error) => {
      if (settled) return
      settled = true
      clearTimeout(timer)
      signal?.removeEventListener('abort', onAbort)
      live.delete(child)
      resolveOnce({
        code: error ? null : exitCode,
        stdout: Buffer.concat(stdout).toString('utf8'),
        stderr: error
          ? `${error.message}\n${Buffer.concat(stderr).toString('utf8')}`.trim()
          : Buffer.concat(stderr).toString('utf8'),
        timedOut,
        overflow,
        aborted,
        durationMs: Date.now() - startedAt
      })
    }

    let exitCode = null
    let resolveOnce = () => {}
    const outcome = new Promise((resolve) => {
      resolveOnce = resolve
    })

    child.stdout?.on('data', (chunk) => {
      if (settled) return
      stdoutBytes += chunk.length
      if (stdoutBytes > maxBytes) {
        overflow = true
        killTree(child)
        return
      }
      stdout.push(chunk)
    })
    child.stderr?.on('data', (chunk) => {
      if (stderrBytes >= STDERR_LIMIT) return
      const room = STDERR_LIMIT - stderrBytes
      const kept = chunk.length > room ? chunk.subarray(0, room) : chunk
      stderr.push(kept)
      stderrBytes += kept.length
    })
    child.on('error', (error) => finish(error))
    child.on('close', (code) => {
      exitCode = code
      finish(null)
    })

    const timer = setTimeout(() => {
      timedOut = true
      killTree(child)
    }, runTimeout)
    timer.unref?.()

    const onAbort = () => {
      aborted = true
      killTree(child)
    }
    signal?.addEventListener('abort', onAbort, { once: true })

    return outcome
  }

  /**
   * Run the CLI under the concurrency limit.
   * @param {string[]} args - argv after the executable.
   * @param {{ cwd?: string, timeoutMs?: number, maxBytes?: number, signal?: AbortSignal }} [options] - run bounds.
   * @returns {Promise<object>} the run outcome with the resolved executable.
   */
  async function run(args, options = {}) {
    const resolvedCli = await resolve()
    await semaphore.acquire(options.signal)
    try {
      const outcome = await runOnce(resolvedCli.path, args, options)
      return { ...outcome, executable: resolvedCli.path, version: resolvedCli.version }
    } finally {
      semaphore.release()
    }
  }

  /**
   * Find and probe the executable once per successful plugin lifetime.
   *
   * A failed resolution is not cached: the panel's own hint tells the user to
   * build and deploy the binary, and Retry or `/health` must see it afterwards.
   * @returns {Promise<{ path: string, version: string, searched: string[] }>} the CLI facts.
   * @throws {RouteError} `cli-missing` or `cli-old`.
   */
  function resolve() {
    if (resolving === undefined) {
      const attempt = resolveOnce()
      resolving = attempt
      attempt.catch(() => {
        if (resolving === attempt) resolving = undefined
      })
    }
    return resolving
  }

  async function resolveOnce() {
    const searched = []
    for (const candidate of candidates) {
      searched.push(candidate)
      if (!isFile(candidate)) continue
      const probe = await runOnce(candidate, ['--version'], { timeoutMs: 5000, maxBytes: 4096 })
      const version = probe.timedOut || probe.overflow || probe.code !== 0
        ? ''
        : parseCliVersion(probe.stdout)
      if (version === '') continue
      if (compareVersions(version, minCliVersion) < 0) {
        throw new RouteError(
          ERROR_CODES.cliOld,
          `v8reader_cli ${version} старее требуемой ${minCliVersion}`,
          503,
          { path: candidate, version, min: minCliVersion, searched }
        )
      }
      return { path: candidate, version, searched }
    }
    throw new RouteError(
      ERROR_CODES.cliMissing,
      'v8reader_cli не найден; задайте config.cliPath в строке плагина или V8READER_CLI',
      503,
      { searched, min: minCliVersion }
    )
  }

  return {
    resolve,
    run,
    searchedCandidates: () => [...candidates],
    dispose() {
      for (const child of live) killTree(child)
      live.clear()
    }
  }
}

/**
 * Kill a child and its descendants. Windows needs `taskkill /T`; POSIX kills the
 * child's process group when it owns one, else the child itself.
 * @param {import('node:child_process').ChildProcess} child - the process to stop.
 * @param {{ platform?: NodeJS.Platform, spawn?: typeof spawn }} [options] - platform seam.
 */
export function killProcessTree(child, { platform = process.platform, spawn: spawnImpl = spawn } = {}) {
  if (child === undefined || child === null) return
  const pid = child.pid
  if (pid === undefined) {
    try {
      child.kill('SIGKILL')
    } catch {
      // already gone
    }
    return
  }
  if (platform === 'win32') {
    try {
      const killer = spawnImpl('taskkill', ['/PID', String(pid), '/T', '/F'], {
        windowsHide: true,
        stdio: 'ignore'
      })
      killer.on('error', () => {})
      return
    } catch {
      // fall through to the direct kill
    }
  }
  try {
    child.kill('SIGKILL')
  } catch {
    // already gone
  }
}
