/**
 * The panel's file chooser.
 *
 * A browser file input never reveals the absolute path it picked, and the panel
 * needs one — the Host opens the container, not the page. So the chooser walks
 * directories through `GET /v8reader/browse` and hands the chosen absolute path
 * back to the panel, which opens it exactly as a typed path.
 *
 * The path bar above the list is editable: it is the only way to reach another
 * drive or a UNC share, and it always shows the directory being listed.
 */
import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { StateError, fetchBrowse } from './api.js'
import { browseErrorText } from './errors.js'
import { formatBytes } from './labels.js'
import { extensionOf } from './paths.js'

/** Focusable controls inside the layer; the Tab cycle stays within them. */
const FOCUSABLE = 'button:not([disabled]), input:not([disabled]), [tabindex]:not([tabindex="-1"])'

/**
 * One row of the listing.
 * @param {{
 *   entry: { name: string, path: string, kind: string, size?: number },
 *   openable: boolean,
 *   busy: boolean,
 *   reason: string,
 *   onOpen: (entry: object) => void
 * }} props - the entry and the chooser's action.
 * @returns {React.ReactNode} the row.
 */
function Row({ entry, openable, busy, reason, onOpen }) {
  return (
    <button
      type="button"
      className="v8r-picker-row"
      data-kind={entry.kind}
      data-openable={openable ? 'true' : 'false'}
      disabled={busy || !openable}
      title={openable ? entry.path : `${entry.path} — ${reason}`}
      onClick={() => { if (openable) onOpen(entry) }}
    >
      <span className="v8r-picker-mark" aria-hidden="true">{entry.kind === 'dir' ? '▸' : ''}</span>
      <span className="v8r-picker-name">{entry.name}</span>
      {entry.kind === 'file' && <span className="v8r-picker-size">{formatBytes(entry.size)}</span>}
    </button>
  )
}

/**
 * The chooser layer.
 * @param {{
 *   initialDir?: string,
 *   translate: (key: string, params?: object) => string,
 *   onPick: (path: string) => void,
 *   onClose: (dir: string) => void
 * }} props - where to start, copy, and the two outcomes.
 * @returns {React.ReactNode} the layer.
 */
export function FilePicker({ initialDir = '', translate: t, onPick, onClose }) {
  const [dir, setDir] = useState('')
  const [listing, setListing] = useState(null)
  const [pathBar, setPathBar] = useState(initialDir)
  const [filter, setFilter] = useState('')
  const [showAll, setShowAll] = useState(false)
  const [busy, setBusy] = useState(true)
  const [error, setError] = useState(null)
  const controller = useRef(null)
  const layer = useRef(null)
  const filterInput = useRef(null)
  const crumbRef = useRef(null)
  const fellBack = useRef(false)
  const lastTarget = useRef(initialDir)

  /**
   * Read one directory.
   * @param {string} target - absolute directory or file, or '' for the Host home.
   * @param {boolean} all - whether unsupported files are listed too.
   * @param {{ fallback?: boolean }} [options] - whether a gone directory falls back to the home directory.
   */
  const load = useCallback(async (target, all, { fallback = true } = {}) => {
    controller.current?.abort()
    const next = new AbortController()
    controller.current = next
    lastTarget.current = target
    setPathBar(target)
    setBusy(true)
    setError(null)
    try {
      const body = await fetchBrowse({ dir: target, all, signal: next.signal })
      if (next.signal.aborted) return
      fellBack.current = false
      setDir(body.dir)
      setPathBar(body.dir)
      setListing(body)
      setFilter('')
    } catch (failure) {
      if (next.signal.aborted || failure?.name === 'AbortError') return
      // A remembered directory may be gone or offline; the home directory is
      // the one place that always exists, so one silent retry keeps the
      // chooser usable instead of leaving it with nowhere to go.
      if (fallback && target !== '' && !fellBack.current) {
        fellBack.current = true
        controller.current?.abort()
        await load('', all, { fallback: false })
        return
      }
      setListing(null)
      setError(failure instanceof StateError
        ? failure
        : new StateError('unknown', String(failure?.message ?? failure), 0))
    } finally {
      if (!next.signal.aborted) setBusy(false)
    }
  }, [])

  useEffect(() => {
    load(initialDir, false)
    filterInput.current?.focus?.()
    return () => { controller.current?.abort() }
  }, [initialDir, load])

  useEffect(() => {
    crumbRef.current?.scrollIntoView?.({ block: 'nearest', inline: 'end' })
  }, [dir])

  const entries = Array.isArray(listing?.entries) ? listing.entries : []
  const supported = Array.isArray(listing?.supported) ? listing.supported : []
  const needle = filter.trim().toLowerCase()
  const shown = useMemo(
    () => (needle === '' ? entries : entries.filter(
      (entry) => entry.name.toLowerCase().includes(needle)
    )),
    [entries, needle]
  )

  /**
   * Whether the panel can open an entry.
   * @param {{ kind: string, name: string }} entry - a listed entry.
   * @returns {boolean} true for directories and supported container files.
   */
  const openable = useCallback((entry) => {
    if (entry.kind === 'dir') return true
    if (supported.length === 0) return true
    return supported.includes(extensionOf(entry.name))
  }, [supported])

  const close = useCallback(() => { onClose(dir) }, [dir, onClose])

  /**
   * Open a row: a directory becomes the new level, a file is the answer.
   * @param {{ kind: string, path: string }} entry - the clicked row.
   */
  const openEntry = useCallback((entry) => {
    if (entry.kind === 'dir') {
      load(entry.path, showAll, { fallback: false })
      return
    }
    onPick(entry.path)
  }, [load, onPick, showAll])

  /**
   * Keep keyboard use inside the layer: the chooser is announced as a modal,
   * so Tab must cycle here and Escape must close it from anywhere inside.
   * @param {React.KeyboardEvent} event - the key event.
   */
  const onKeyDown = useCallback((event) => {
    if (event.key === 'Escape') {
      event.stopPropagation()
      close()
      return
    }
    if (event.key !== 'Tab') return
    const nodes = Array.from(layer.current?.querySelectorAll(FOCUSABLE) ?? [])
    if (nodes.length === 0) return
    const first = nodes[0]
    const last = nodes[nodes.length - 1]
    if (event.shiftKey && document.activeElement === first) {
      event.preventDefault()
      last.focus()
    } else if (!event.shiftKey && document.activeElement === last) {
      event.preventDefault()
      first.focus()
    }
  }, [close])

  const parent = typeof listing?.parent === 'string' ? listing.parent : null
  const crumbs = Array.isArray(listing?.breadcrumbs) ? listing.breadcrumbs : []

  return (
    <div
      className="v8r-picker"
      role="dialog"
      aria-modal="true"
      aria-label={t('picker.title')}
      aria-busy={busy ? 'true' : 'false'}
      tabIndex={-1}
      ref={layer}
      onKeyDown={onKeyDown}
    >
      <div className="v8r-picker-head">
        <div className="v8r-picker-title">{t('picker.title')}</div>
        <button
          type="button"
          className="v8r-button v8r-button--ghost"
          title={t('picker.close')}
          aria-label={t('picker.close')}
          onClick={close}
        >
          ×
        </button>
      </div>

      <div className="v8r-picker-crumbs">
        <button
          type="button"
          className="v8r-button"
          title={t('picker.up')}
          aria-label={t('picker.up')}
          disabled={parent === null || busy}
          onClick={() => { load(parent, showAll, { fallback: false }) }}
        >
          ↑
        </button>
        {crumbs.map((crumb) => (
          <button
            key={crumb.path}
            type="button"
            className="v8r-crumb"
            data-active={crumb.path === dir ? 'true' : 'false'}
            title={crumb.path}
            disabled={busy}
            ref={(node) => { if (node !== null && crumb.path === dir) crumbRef.current = node }}
            onClick={() => { load(crumb.path, showAll, { fallback: false }) }}
          >
            {crumb.name}
          </button>
        ))}
      </div>

      <div className="v8r-picker-goto">
        <input
          className="v8r-input v8r-picker-path"
          type="text"
          value={pathBar}
          spellCheck={false}
          placeholder={t('picker.goto')}
          aria-label={t('picker.goto')}
          onChange={(event) => { setPathBar(event.target.value) }}
          onKeyDown={(event) => {
            if (event.key !== 'Enter') return
            event.stopPropagation()
            const value = pathBar.trim()
            if (value !== '') load(value, showAll, { fallback: false })
          }}
        />
        <button
          type="button"
          className="v8r-button"
          disabled={busy || pathBar.trim() === ''}
          onClick={() => { load(pathBar.trim(), showAll, { fallback: false }) }}
        >
          {t('picker.goto.action')}
        </button>
      </div>

      <div className="v8r-picker-controls">
        <input
          className="v8r-input"
          type="text"
          value={filter}
          spellCheck={false}
          ref={filterInput}
          placeholder={t('picker.filter')}
          aria-label={t('picker.filter')}
          onChange={(event) => { setFilter(event.target.value) }}
        />
        <label className="v8r-picker-check">
          <input
            type="checkbox"
            checked={showAll}
            disabled={busy}
            onChange={(event) => {
              const next = event.target.checked
              setShowAll(next)
              load(dir, next, { fallback: false })
            }}
          />
          {t('picker.allFiles')}
        </label>
      </div>

      {busy && <div className="v8r-loading" role="status">{t('picker.loading')}</div>}

      {error !== null && (
        <div className="v8r-banner" role="alert">
          <div className="v8r-banner-title">{t('error.title')}: {browseErrorText(t, error)}</div>
          <div className="v8r-banner-actions">
            <button
              type="button"
              className="v8r-button"
              onClick={() => { load(lastTarget.current, showAll, { fallback: false }) }}
            >
              {t('picker.retry')}
            </button>
            <button type="button" className="v8r-button" onClick={() => { load('', showAll, { fallback: false }) }}>
              {t('picker.home')}
            </button>
          </div>
        </div>
      )}

      <div className="v8r-picker-list" data-busy={busy ? 'true' : 'false'}>
        {shown.length === 0 && !busy && error === null && (
          needle === ''
            ? <p className="v8r-muted">{showAll ? t('picker.empty') : t('picker.emptyFiltered')}</p>
            : (
              <div className="v8r-picker-none">
                <span className="v8r-muted">{t('picker.notFound', { query: filter.trim() })}</span>
                <button type="button" className="v8r-button" onClick={() => { setFilter('') }}>
                  {t('picker.resetFilter')}
                </button>
              </div>
            )
        )}
        {shown.map((entry) => (
          <Row
            key={entry.path}
            entry={entry}
            openable={openable(entry)}
            busy={busy}
            reason={t('picker.unsupported')}
            onOpen={openEntry}
          />
        ))}
      </div>

      <div className="v8r-picker-foot">
        <span className="v8r-muted">{t('picker.hint')}</span>
        {typeof listing?.hiddenCount === 'number' && listing.hiddenCount > 0 && (
          <button
            type="button"
            className="v8r-link"
            disabled={busy}
            onClick={() => { setShowAll(true); load(dir, true, { fallback: false }) }}
          >
            {t('picker.hidden', { count: listing.hiddenCount })}
          </button>
        )}
        {listing?.truncated === true && (
          <span className="v8r-muted">{t('picker.truncated', { count: entries.length })}</span>
        )}
      </div>
    </div>
  )
}
