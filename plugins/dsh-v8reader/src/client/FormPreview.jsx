/**
 * The exact-form view: an iframe of the Host's ready-made form page.
 *
 * The page is served by `/v8reader/form` with the `form-model` document already
 * inlined, so a failure is an HTTP error rather than a half-painted form. When
 * the page does load, the renderer reports its own progress through
 * `window.__QV8_STATE`; polling that flag keeps the panel honest about a
 * renderer that crashed after a successful response.
 */
import React, { useCallback, useEffect, useRef, useState } from 'react'
import { frameDocumentError, sameFrameUrl } from './form.js'

/** How often the frame state is polled, in milliseconds. */
const POLL_MS = 250

/** After this long without a ready state the frame is treated as broken. */
const TIMEOUT_MS = 30000

/**
 * The form canvas with its loading and failure states.
 * @param {{
 *   src: string,
 *   title: string,
 *   zoom: number,
 *   nonce: number,
 *   translate: (key: string, params?: object) => string,
 *   onRetry?: () => void
 * }} props - where the form page is, and how it is shown.
 * @returns {React.ReactNode} the view.
 */
export function FormPreview({ src, title, zoom = 100, nonce = 0, translate: t, onRetry }) {
  const frame = useRef(null)
  const [status, setStatus] = useState('loading')
  const [error, setError] = useState(null)

  /** Apply the zoom to the rendered document; the iframe itself keeps its box. */
  const applyZoom = useCallback(() => {
    try {
      const doc = frame.current?.contentDocument
      if (doc?.documentElement !== undefined && doc.documentElement !== null) {
        doc.documentElement.style.zoom = String(zoom / 100)
      }
    } catch {
      // a frame whose document is temporarily unavailable keeps the old zoom
    }
  }, [zoom])

  useEffect(() => {
    setStatus('loading')
    setError(null)
    let alive = true
    let timer
    const startedAt = Date.now()
    const baseHref = globalThis.location?.href ?? 'http://localhost/'

    /** Inspect the frame once and schedule the next look while it is not done. */
    const check = () => {
      if (!alive) return
      if (Date.now() - startedAt > TIMEOUT_MS) {
        setStatus('error')
        setError({ code: 'form-timeout', message: '' })
        return
      }
      const element = frame.current
      if (element === null) return
      let doc = null
      try {
        doc = element.contentDocument
      } catch {
        doc = null
      }
      if (doc === null) {
        schedule()
        return
      }

      // While the next page is loading the frame keeps the old document, which
      // may still say "ready" for the previous form. Only the document at the
      // requested URL is allowed to report its state.
      let currentUrl = ''
      try {
        currentUrl = doc.location?.href ?? ''
      } catch {
        currentUrl = ''
      }
      if (!sameFrameUrl(currentUrl, src, baseHref)) {
        schedule()
        return
      }

      const documentError = frameDocumentError(doc)
      if (documentError !== null) {
        setStatus('error')
        setError(documentError)
        return
      }

      const win = element.contentWindow
      const state = win?.__QV8_STATE
      if (state === 'ready') {
        applyZoom()
        setStatus('ready')
        return
      }
      if (state === 'error') {
        setStatus('error')
        setError({
          code: 'form-error',
          message: String(win?.__QV8_ERROR ?? '')
        })
        return
      }
      schedule()
    }

    /** Poll once more shortly. */
    const schedule = () => {
      timer = setTimeout(check, POLL_MS)
    }

    timer = setTimeout(check, POLL_MS)
    return () => {
      alive = false
      clearTimeout(timer)
    }
  }, [src, nonce])

  // Zooming never reloads the form: the scale is written into the rendered
  // document, so the iframe keeps its viewport and the form only reflows.
  useEffect(() => {
    if (status === 'ready') applyZoom()
  }, [applyZoom, status])

  return (
    <div className="v8r-form" data-status={status}>
      <iframe
        key={nonce}
        ref={frame}
        className="v8r-form-frame"
        src={src}
        title={title}
        sandbox="allow-scripts allow-same-origin"
        onLoad={() => { applyZoom() }}
      />

      {status === 'loading' && (
        <div className="v8r-form-status" role="status">{t('form.loading')}</div>
      )}

      {status === 'error' && (
        <div className="v8r-form-error" role="alert">
          <div className="v8r-banner">
            <div className="v8r-banner-title">{t('form.error.title')}</div>
            <div>{t(`error.${error?.code ?? 'form-error'}`)}</div>
            {error?.message !== undefined && error.message !== '' && (
              <div className="v8r-banner-detail">{error.message}</div>
            )}
            {typeof onRetry === 'function' && (
              <div className="v8r-banner-actions">
                <button type="button" className="v8r-button" onClick={() => { onRetry() }}>
                  {t('form.retry')}
                </button>
              </div>
            )}
          </div>
        </div>
      )}
    </div>
  )
}
