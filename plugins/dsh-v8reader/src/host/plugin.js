/**
 * Composition root of the Host half: settings, the CLI runner, the response
 * cache, and the one prefix route. Kept free of DSH imports so the tests can
 * exercise it under plain Node.
 */
import { MIN_CLI_VERSION, createCli } from './cli.js'
import { createLruCache } from './cache.js'
import { ROUTE_PREFIX, createResource } from './routes.js'

/** Defaults of the row's `config`; the plugin declares no Config schema in 0.1. */
export const DEFAULT_SETTINGS = Object.freeze({
  cliPath: '',
  timeoutMs: 20000,
  maxJsonBytes: 16777216,
  maxConcurrent: 2,
  cacheEntries: 32,
  cacheBytes: 67108864,
  minCliVersion: MIN_CLI_VERSION
})

/**
 * Clamp an integer setting.
 * @param {unknown} value - the raw value.
 * @param {number} fallback - value for a missing or non-numeric setting.
 * @param {number} min - lower bound.
 * @param {number} max - upper bound.
 * @returns {number} the effective value.
 */
function integerSetting(value, fallback, min, max) {
  if (typeof value !== 'number' || !Number.isFinite(value)) return fallback
  return Math.min(max, Math.max(min, Math.trunc(value)))
}

/**
 * Merge the row's config with the defaults.
 * @param {object|undefined} raw - the plugin row's `config`.
 * @returns {typeof DEFAULT_SETTINGS} the effective settings.
 */
export function resolveSettings(raw = {}) {
  const config = raw !== null && typeof raw === 'object' ? raw : {}
  return {
    cliPath: typeof config.cliPath === 'string' ? config.cliPath.trim() : DEFAULT_SETTINGS.cliPath,
    timeoutMs: integerSetting(config.timeoutMs, DEFAULT_SETTINGS.timeoutMs, 1000, 120000),
    maxJsonBytes: integerSetting(
      config.maxJsonBytes,
      DEFAULT_SETTINGS.maxJsonBytes,
      1024,
      64 * 1024 * 1024
    ),
    maxConcurrent: integerSetting(config.maxConcurrent, DEFAULT_SETTINGS.maxConcurrent, 1, 8),
    cacheEntries: integerSetting(config.cacheEntries, DEFAULT_SETTINGS.cacheEntries, 1, 512),
    cacheBytes: integerSetting(
      config.cacheBytes,
      DEFAULT_SETTINGS.cacheBytes,
      1024 * 1024,
      512 * 1024 * 1024
    ),
    minCliVersion: DEFAULT_SETTINGS.minCliVersion
  }
}

/**
 * Create the Host half.
 * @param {{
 *   config?: object,
 *   packageDir?: string,
 *   env?: NodeJS.ProcessEnv,
 *   platform?: NodeJS.Platform,
 *   spawn?: typeof import('node:child_process').spawn,
 *   stat?: typeof import('node:fs').statSync,
 *   cache?: object
 * }} [options] - row config and test seams.
 * @returns {{
 *   settings: typeof DEFAULT_SETTINGS,
 *   cli: object,
 *   cache: object,
 *   resource: (request: Request) => Promise<Response>,
 *   route: { kind: 'prefix', path: string, resource: (request: Request) => Promise<Response> },
 *   dispose: () => void
 * }} the plugin parts.
 */
export function createPlugin({
  config = {},
  packageDir = '',
  env = process.env,
  platform = process.platform,
  spawn,
  stat,
  readdir,
  home,
  cache: providedCache
} = {}) {
  const settings = resolveSettings(config)
  const cli = createCli({
    settings,
    env,
    platform,
    packageDir,
    ...(spawn === undefined ? {} : { spawn }),
    ...(stat === undefined ? {} : { stat })
  })
  const cache = providedCache ?? createLruCache({
    maxEntries: settings.cacheEntries,
    maxBytes: settings.cacheBytes
  })
  const resource = createResource({
    cli,
    cache,
    settings,
    ...(stat === undefined ? {} : { stat }),
    ...(readdir === undefined ? {} : { readdir }),
    ...(home === undefined ? {} : { home }),
    platform
  })

  return {
    settings,
    cli,
    cache,
    resource,
    route: { kind: 'prefix', path: ROUTE_PREFIX, resource },
    dispose() {
      cli.dispose()
      cache.clear()
    }
  }
}
