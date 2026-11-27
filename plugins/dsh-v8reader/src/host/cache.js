/**
 * A byte-budgeted LRU cache for panel responses.
 *
 * The panel asks for the same configuration once per screen; a 1.8 MB JSON
 * document is recomputed from a 760 MB container each time without this. Keys
 * already carry the file's size and mtime, so an edited file never hits a stale
 * entry and no invalidation hook is needed.
 */

/**
 * Create an LRU cache bounded by entry count and total string bytes.
 * @param {{ maxEntries?: number, maxBytes?: number }} [options] - the budget.
 * @returns {{
 *   get: (key: string) => string | undefined,
 *   set: (key: string, value: string) => boolean,
 *   clear: () => void,
 *   readonly size: number,
 *   readonly bytes: number
 * }} the cache.
 */
export function createLruCache({ maxEntries = 32, maxBytes = 64 * 1024 * 1024 } = {}) {
  /** @type {Map<string, { value: string, bytes: number }>} */
  const entries = new Map()
  let bytes = 0

  const cache = {
    get(key) {
      const entry = entries.get(key)
      if (entry === undefined) return undefined
      // Re-insert to move the key to the most-recently-used end.
      entries.delete(key)
      entries.set(key, entry)
      return entry.value
    },

    /**
     * Store one response.
     * @param {string} key - cache key, carrying the file generation.
     * @param {string} value - the response body.
     * @returns {boolean} false when the body exceeds the whole budget and is not stored.
     */
    set(key, value) {
      const size = Buffer.byteLength(value, 'utf8')
      if (size > maxBytes) return false

      const held = entries.get(key)
      if (held !== undefined) {
        bytes -= held.bytes
        entries.delete(key)
      }
      entries.set(key, { value, bytes: size })
      bytes += size

      while (entries.size > maxEntries || bytes > maxBytes) {
        const oldest = entries.keys().next()
        if (oldest.done) break
        const evicted = entries.get(oldest.value)
        entries.delete(oldest.value)
        bytes -= evicted.bytes
      }
      return true
    },

    clear() {
      entries.clear()
      bytes = 0
    },

    get size() {
      return entries.size
    },

    get bytes() {
      return bytes
    }
  }

  return cache
}
