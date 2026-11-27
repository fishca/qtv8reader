/**
 * Preferences of the module code view.
 *
 * Kept out of the JSX component so the Node tests can read and clamp them
 * without a React/JSX toolchain. One storage key holds all three settings; a
 * corrupt or missing entry falls back one field at a time.
 */

/** Storage key of the code-view preferences. */
export const CODE_SETTINGS_KEY = 'dsh.v8reader.code.v1'

/** Font size bounds, in pixels. */
export const MIN_FONT_SIZE = 10
export const MAX_FONT_SIZE = 20

/** Defaults copied from `BslHighlightSettings`: highlighting on, no wrapping. */
export const DEFAULT_CODE_SETTINGS = Object.freeze({
  enabled: true,
  wrap: false,
  size: 12.5
})

/**
 * Clamp a numeric preference.
 * @param {unknown} value - the stored value.
 * @param {number} fallback - used when the value is not a number.
 * @param {number} min - lower bound.
 * @param {number} max - upper bound.
 * @returns {number} the effective value.
 */
export function clampNumber(value, fallback, min, max) {
  if (typeof value !== 'number' || !Number.isFinite(value)) return fallback
  return Math.min(max, Math.max(min, value))
}

/**
 * Read the stored preferences, falling back to the defaults one field at a time.
 * @param {{ getItem?: (key: string) => string | null } | undefined} storage - storage seam.
 * @returns {{ enabled: boolean, wrap: boolean, size: number }} the effective settings.
 */
export function readCodeSettings(storage = globalThis.localStorage) {
  let raw = null
  try {
    raw = storage?.getItem(CODE_SETTINGS_KEY) ?? null
  } catch {
    raw = null
  }
  if (raw === null) return { ...DEFAULT_CODE_SETTINGS }
  let parsed
  try {
    parsed = JSON.parse(raw)
  } catch {
    return { ...DEFAULT_CODE_SETTINGS }
  }
  if (parsed === null || typeof parsed !== 'object') return { ...DEFAULT_CODE_SETTINGS }
  return {
    enabled: typeof parsed.enabled === 'boolean' ? parsed.enabled : DEFAULT_CODE_SETTINGS.enabled,
    wrap: typeof parsed.wrap === 'boolean' ? parsed.wrap : DEFAULT_CODE_SETTINGS.wrap,
    size: clampNumber(parsed.size, DEFAULT_CODE_SETTINGS.size, MIN_FONT_SIZE, MAX_FONT_SIZE)
  }
}

/**
 * Store the preferences, ignoring storage failures.
 * @param {{ enabled: boolean, wrap: boolean, size: number }} settings - what to store.
 * @param {{ setItem?: (key: string, value: string) => void } | undefined} storage - storage seam.
 */
export function writeCodeSettings(settings, storage = globalThis.localStorage) {
  try {
    storage?.setItem(CODE_SETTINGS_KEY, JSON.stringify(settings))
  } catch {
    // preferences are a convenience
  }
}
