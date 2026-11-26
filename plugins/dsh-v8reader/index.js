/**
 * Host half of the plugin: serves `/v8reader/*` from the panel's HTTP origin.
 *
 * The row config is read without a declared Config schema in 0.1: `cliPath`,
 * `timeoutMs`, `maxJsonBytes`, `maxConcurrent`. The route and the child
 * processes live exactly as long as the plugin, so unloading the bundle removes
 * both.
 */
import { fileURLToPath } from 'node:url'
import { createPlugin } from './src/host/plugin.js'

/** The Web runtime answers the panel's fetches; without it there is no surface. */
export const inject = ['webServer']

/**
 * Plugin body.
 * @param {import('@deepseek-ai/cordis').Context} ctx - Host context.
 * @param {object} [config] - the row's config.
 */
export function apply(ctx, config) {
  const packageDir = fileURLToPath(new URL('.', import.meta.url))
  const plugin = createPlugin({ config, packageDir })

  ctx.effect(
    () => ctx.webServer.register(plugin.route),
    'v8reader: HTTP routes'
  )
  ctx.effect(
    () => () => plugin.dispose(),
    'v8reader: CLI processes and cache'
  )
}
