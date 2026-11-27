/**
 * Pure parts of the table-document (MXL) view.
 *
 * The exported page is a self-contained HTML table; the panel only chooses how
 * much of it fits. Zoom is a preference, not part of the request, so it is
 * stored next to the form zoom and clamped here, where the Node tests can
 * check it without a browser.
 */

/** Storage key of the table-document preference. */
export const MXL_SETTINGS_KEY = 'dsh.v8reader.mxl.v1'

/** Zoom steps offered by the table-document header, in percent. */
export const MXL_ZOOM_STEPS = Object.freeze([25, 50, 75, 100, 125, 150, 200, 300, 400])

/** Zoom used when nothing was stored and for "fit width" fallbacks. */
export const DEFAULT_MXL_ZOOM = 100

/** Smallest and largest zoom the exporter itself accepts. */
export const MXL_MIN_ZOOM = 25
export const MXL_MAX_ZOOM = 400

/** Chrome around the sheet: the frame's scrollbar and the page margins. */
const FIT_FRAME_PX = 32

/** The sheet's own padding on both sides. */
const SHEET_PADDING_PX = 24

/**
 * Clamp a zoom value to the supported range.
 * @param {unknown} value - a number from storage or an input.
 * @param {number} [fallback] - value for a non-numeric input.
 * @returns {number} an integer percentage inside {@link MXL_MIN_ZOOM}–{@link MXL_MAX_ZOOM}.
 */
export function clampMxlZoom(value, fallback = DEFAULT_MXL_ZOOM) {
  const number = Number(value)
  if (!Number.isFinite(number)) return fallback
  return Math.min(MXL_MAX_ZOOM, Math.max(MXL_MIN_ZOOM, Math.round(number)))
}

/**
 * Zoom that makes one sheet fit the frame width.
 *
 * The sheet keeps its authored pixel width (the exporter writes fixed column
 * widths), so fit width is a ratio of the visible width to the sheet width.
 * A zero width means the frame has not been laid out yet; the default keeps
 * the page readable instead of collapsing it.
 * @param {number} sheetWidth - authored width of the table, unscaled pixels.
 * @param {number} frameWidth - visible width of the frame, pixels.
 * @returns {number} a clamped percentage.
 */
export function fitWidthZoom(sheetWidth, frameWidth) {
  if (!(sheetWidth > 0) || !(frameWidth > 0)) return DEFAULT_MXL_ZOOM
  const available = frameWidth - FIT_FRAME_PX
  if (!(available > 0)) return MXL_MIN_ZOOM
  return clampMxlZoom((available / (sheetWidth + SHEET_PADDING_PX)) * 100)
}

/**
 * Read the stored table-document preference.
 * @param {Storage|null} [storage] - storage to read, `localStorage` by default.
 * @returns {{ zoom: number, fit: boolean }} the preference.
 */
export function readMxlSettings(storage = globalThis.localStorage) {
  let raw = ''
  try {
    raw = storage?.getItem(MXL_SETTINGS_KEY) ?? ''
  } catch {
    raw = ''
  }
  if (raw === '') return { zoom: DEFAULT_MXL_ZOOM, fit: false }
  try {
    const parsed = JSON.parse(raw)
    return {
      zoom: clampMxlZoom(parsed?.zoom),
      fit: parsed?.fit === true
    }
  } catch {
    return { zoom: DEFAULT_MXL_ZOOM, fit: false }
  }
}

/**
 * Remember the table-document preference, ignoring storage failures.
 * @param {{ zoom: number, fit: boolean }} settings - what to remember.
 * @param {Storage|null} [storage] - storage to write, `localStorage` by default.
 */
export function writeMxlSettings(settings, storage = globalThis.localStorage) {
  try {
    storage?.setItem(MXL_SETTINGS_KEY, JSON.stringify({
      zoom: clampMxlZoom(settings?.zoom),
      fit: settings?.fit === true
    }))
  } catch {
    // remembering a preference is a convenience
  }
}
