/**
 * Path helpers of the client half.
 *
 * The panel only needs one question answered locally — which directory a typed
 * file path belongs to — because the chooser remembers that directory and the
 * Host does the real resolution. Both separators are accepted, so a Windows
 * path pasted into the panel works in the browser as well.
 */

/**
 * Lowercase extension of a file name, dot included.
 * @param {unknown} name - a file name.
 * @returns {string} the extension, or `''` when the name has none.
 */
export function extensionOf(name) {
  const value = String(name ?? '')
  const dot = value.lastIndexOf('.')
  if (dot <= 0) return ''
  return value.slice(dot).toLowerCase()
}

/**
 * Directory part of a file path, keeping the filesystem root intact.
 *
 * `E:\work\a.cf` → `E:\work`, `E:\a.cf` → `E:\`, `\\srv\share\a.cf` →
 * `\\srv\share\`, `/work/a.cf` → `/work`, and a bare name → an empty string.
 * @param {unknown} filePath - the typed path.
 * @returns {string} the containing directory, or `''` when there is none.
 */
export function dirOf(filePath) {
  const raw = String(filePath ?? '').trim()
  if (raw === '') return ''
  const trailing = /[\\/]$/.test(raw)
  const value = raw.replace(/[\\/]+$/, '')
  if (value === '') return raw.includes('\\') ? '\\' : '/'
  if (/^[a-zA-Z]:$/.test(value)) return `${value}\\`
  // A path that already ends with a separator names the directory itself.
  if (trailing) return value

  const cut = Math.max(value.lastIndexOf('\\'), value.lastIndexOf('/'))
  if (cut < 0) return ''

  if (/^[a-zA-Z]:$/.test(value.slice(0, cut))) return value.slice(0, cut + 1)

  const head = value.slice(0, cut)
  // A UNC share root has no parent to walk to: `\\server\share\` stays whole.
  if (/^\\\\[^\\/]+[\\/][^\\/]+$/.test(head)) return `${head}\\`
  if (head === '') return '/'
  return head
}
