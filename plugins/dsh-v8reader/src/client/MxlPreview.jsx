/**
 * The table-document (MXL) canvas: an iframe of the Host's exported page.
 *
 * The page is served by `/v8reader/mxl` and is self-contained: one table with
 * inline styles and no scripts. The frame is sandboxed without `allow-scripts`,
 * so the page cannot execute anything; `allow-same-origin` keeps the frame's
 * document readable, which is how the panel scales it. A failed export arrives
 * as a JSON document inside the frame, which {@link frameDocumentError}
 * recognises and the panel shows as its usual banner.
 */
import React, { useCallback, useEffect, useRef, useState } from 'react'
import { frameDocumentError } from './form.js'
import { fitWidthZoom } from './mxl.js'

/**
 * The table-document canvas with its loading and failure states.
 * @param {{
 *   src: string,
 *   title: string,
 *   zoom: number,
 *   fit: boolean,
 *   nonce: number,
 *   translate: (key: string, params?: object) => string,
 *   onRetry?: () => void
 * }} props - where the page is, and how it is shown.
 * @returns {React.ReactNode} the view.
 */
export function MxlPreview({ src, title, zoom = 100, fit = false, nonce = 0, translate: t, onRetry }) {
  const frame = useRef(null)
  const [status, setStatus] = useState('loading')
  const [error, setError] = useState(null)

  /**
   * Apply the current scale to the loaded document.
   *
   * Fit width is measured on the unscaled table, because the frame's rect is
   * scaled too once a zoom is applied; the scale is reset to 1 for the
   * measurement and set afterwards. The document belongs to this origin, so
   * the measurement is a plain DOM read.
   * @returns {boolean} false when the frame's document is not readable yet.
   */
  const applyZoom = useCallback(() => {
    const element = frame.current
    let doc = null
    try {
      doc = element?.contentDocument ?? null
    } catch {
      doc = null
    }
    const root = doc?.documentElement
    if (root === undefined || root === null) return false

    let percent = zoom
    if (fit) {
      root.style.zoom = '1'
      const sheet = doc.querySelector('table.mxl')
      const tableWidth = sheet?.getBoundingClientRect().width ?? 0
      percent = fitWidthZoom(tableWidth, element?.clientWidth ?? 0)
    }
    root.style.zoom = String(percent / 100)
    return true
  }, [fit, zoom])

  /** Read the frame once it loads and switch to its own state. */
  const inspect = useCallback(() => {
    const element = frame.current
    if (element === null) return
    let doc = null
    try {
      doc = element.contentDocument
    } catch {
      doc = null
    }
    if (doc === null) {
      setStatus('error')
      setError({ code: 'mxl-error', message: '' })
      return
    }

    const documentError = frameDocumentError(doc)
    if (documentError !== null) {
      setStatus('error')
      setError(documentError)
      return
    }
    setStatus('ready')
    applyZoom()
  }, [applyZoom])

  // A new request starts from the loading state; the frame is remounted by its
  // key, so a previous document never reports the state of the next one.
  useEffect(() => {
    setStatus('loading')
    setError(null)
  }, [src, nonce])

  // Zooming and fit width never reload the page: the scale is written into the
  // rendered document, so the frame keeps its viewport and the table reflows.
  useEffect(() => {
    if (status === 'ready') applyZoom()
  }, [applyZoom, status])

  // Fit width follows the pane: a docked, floating or resized Sidebar measures
  // the same table against its new width.
  useEffect(() => {
    if (!fit || status !== 'ready') return undefined
    const element = frame.current
    if (element === null || typeof ResizeObserver !== 'function') return undefined
    const observer = new ResizeObserver(() => { applyZoom() })
    observer.observe(element)
    return () => { observer.disconnect() }
  }, [applyZoom, fit, status])

  return (
    <div className="v8r-mxl" data-status={status}>
      <iframe
        key={nonce}
        ref={frame}
        className="v8r-mxl-frame"
        src={src}
        title={title}
        sandbox="allow-same-origin"
        onLoad={inspect}
      />

      {status === 'loading' && (
        <div className="v8r-form-status" role="status">{t('template.loading')}</div>
      )}

      {status === 'error' && (
        <div className="v8r-form-error" role="alert">
          <div className="v8r-banner">
            <div className="v8r-banner-title">{t('template.error.title')}</div>
            <div>{t(`error.${error?.code ?? 'mxl-error'}`)}</div>
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
