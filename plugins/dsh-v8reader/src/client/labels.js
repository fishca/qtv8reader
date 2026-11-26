/**
 * Pure label and shape helpers of the panel.
 *
 * Kept in a plain `.js` module so the Node tests can exercise them without a
 * React/JSX toolchain.
 */

/**
 * Format a byte count for the summary line.
 * @param {number} bytes - the file size.
 * @returns {string} a short human-readable size.
 */
export function formatBytes(bytes) {
  if (typeof bytes !== 'number' || !Number.isFinite(bytes) || bytes < 0) return ''
  const units = ['Б', 'КБ', 'МБ', 'ГБ']
  let value = bytes
  let index = 0
  while (value >= 1024 && index < units.length - 1) {
    value /= 1024
    index += 1
  }
  const digits = index === 0 ? 0 : value < 10 ? 1 : 0
  return `${value.toFixed(digits)} ${units[index]}`
}

/**
 * Localized label of a `file.kind` value.
 * @param {(key: string) => string} t - translate.
 * @param {string} kind - `configuration`, `extension`, `externalProcessing`, `externalReport`.
 * @returns {string} the label, or the raw kind when the dictionary has no key.
 */
export function fileKindLabel(t, kind) {
  const key = `kind.${String(kind ?? '')}`
  const label = t(key)
  return label === key ? String(kind ?? '') : label
}

/**
 * Localized label of a section.
 * @param {(key: string) => string} t - translate.
 * @param {{ kind: string, title: string }} section - one `object.sections[]` entry.
 * @returns {string} the tab label, or the CLI's own title when the dictionary has no key.
 */
export function sectionLabel(t, section) {
  const key = `section.${String(section.kind ?? '')}`
  const label = t(key)
  return label === key ? String(section.title ?? section.kind ?? '') : label
}

/**
 * Flatten a section's entries with their nesting depth.
 * @param {object[]} entries - entries of one section.
 * @param {number} [depth] - current depth.
 * @returns {Array<{ entry: object, depth: number }>} rows in display order.
 */
export function flattenEntries(entries, depth = 0) {
  const rows = []
  for (const entry of entries) {
    rows.push({ entry, depth })
    if (Array.isArray(entry.children) && entry.children.length > 0) {
      rows.push(...flattenEntries(entry.children, depth + 1))
    }
  }
  return rows
}
