/**
 * The form view's pure parts.
 *
 * The iframe cannot throw: a failed `/v8reader/form` answer is a JSON document
 * painted inside the frame. Recognising it there lets the panel show its usual
 * localized banner instead of a wall of JSON, and keeps the check testable
 * without a browser.
 */

/**
 * Whether a frame document is showing the requested page.
 *
 * While the next page loads the frame keeps the previous document, whose state
 * may still say "ready". Comparing the location by origin, pathname and parsed
 * query (rather than the raw href) tolerates the browser normalizing
 * percent-encoding or `+` versus `%20` in the query.
 * @param {string} actualHref - `contentWindow.location.href` of the frame.
 * @param {string} source - the `src` that was requested.
 * @param {string} [baseHref] - the page URL used to resolve a relative `src`.
 * @returns {boolean} true when the frame is at the requested location.
 */
export function sameFrameUrl(actualHref, source, baseHref) {
  try {
    const actual = new URL(actualHref)
    const wanted = new URL(source, baseHref ?? `${actual.origin}/`)
    if (actual.origin !== wanted.origin) return false
    if (actual.pathname !== wanted.pathname) return false
    return new URLSearchParams(actual.search).toString()
      === new URLSearchParams(wanted.search).toString()
  } catch {
    return false
  }
}

/**
 * Recognise the JSON error the Host returned as the frame document.
 * @param {{ contentType?: string, body?: { textContent?: string } } | null} doc - the frame document, if readable.
 * @returns {{ code: string, message: string } | null} the error, or null when the document is a page.
 */
export function frameDocumentError(doc) {
  if (doc === null || doc === undefined) return null
  const type = String(doc.contentType ?? '')
  if (!type.includes('json')) return null
  const text = String(doc.body?.textContent ?? '')
  try {
    const parsed = JSON.parse(text)
    if (parsed?.error !== undefined && typeof parsed.error.message === 'string') {
      return {
        code: typeof parsed.error.code === 'string' ? parsed.error.code : 'form-error',
        message: parsed.error.message
      }
    }
  } catch {
    // a non-JSON body is reported with the raw text below
  }
  return { code: 'form-error', message: text }
}
