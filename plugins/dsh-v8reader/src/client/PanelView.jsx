/**
 * The right-Sidebar page that reads one 1C:Enterprise container.
 *
 * The panel owns no data of its own: every screen is one `state` request whose
 * answer the Host serves from its cache. Modules are shown as plain text here;
 * the syntax-aware CodeView arrives with its own stage.
 */
import React, { useCallback, useEffect, useRef, useState } from 'react'
import { StateError, fetchState } from './api.js'
import { fileKindLabel, flattenEntries, formatBytes, sectionLabel } from './labels.js'
import { PANEL_CSS } from './styles.js'
import { Tree } from './Tree.jsx'

/** Key of the last opened path; only the path is remembered, never file contents. */
export const LAST_PATH_KEY = 'dsh.v8reader.path.v1'

/** Module characters rendered before the "truncated" note. */
const MODULE_LIMIT = 300000

/**
 * Read the remembered path.
 * @returns {string} the last opened absolute path, or an empty string.
 */
function readLastPath() {
  try {
    return globalThis.localStorage?.getItem(LAST_PATH_KEY) ?? ''
  } catch {
    return ''
  }
}

/**
 * Remember the opened path, ignoring storage failures (private windows, quota).
 * @param {string} value - the absolute path.
 */
function writeLastPath(value) {
  try {
    globalThis.localStorage?.setItem(LAST_PATH_KEY, value)
  } catch {
    // remembering the path is a convenience
  }
}

/**
 * One section as a table of name, synonym, and type.
 * @param {{ section: object, translate: (key: string) => string }} props - the section.
 * @returns {React.ReactNode} the table.
 */
function SectionTable({ section, translate: t }) {
  const rows = flattenEntries(Array.isArray(section.entries) ? section.entries : [])
  if (rows.length === 0) return <p className="v8r-muted">{t('panel.sectionEmpty')}</p>

  return (
    <table className="v8r-table">
      <thead>
        <tr>
          <th>{t('panel.column.name')}</th>
          <th>{t('panel.column.synonym')}</th>
          <th>{t('panel.column.type')}</th>
        </tr>
      </thead>
      <tbody>
        {rows.map(({ entry, depth }, index) => (
          <tr key={`${entry.name}:${index}`}>
            <td>
              <span style={{ paddingLeft: `${depth * 14}px` }} className="v8r-name">{entry.name}</span>
              {entry.comment !== undefined && entry.comment !== '' && (
                <div className="v8r-muted">{entry.comment}</div>
              )}
            </td>
            <td>{entry.synonym}</td>
            <td className="v8r-muted">{entry.typeText}</td>
          </tr>
        ))}
      </tbody>
    </table>
  )
}

/**
 * The failure banner.
 * @param {{ error: Error & { code?: string, detail?: object }, translate: (key: string, params?: object) => string }} props - the failure.
 * @returns {React.ReactNode} the banner.
 */
function ErrorBanner({ error, translate: t }) {
  const code = typeof error?.code === 'string' ? error.code : 'unknown'
  const key = `error.${code}`
  const localized = t(key, error?.detail ?? {})
  const title = localized === key ? t('error.unknown') : localized
  const hint = code === 'cli-missing' || code === 'cli-old' ? t('error.hint.cli-missing') : ''
  const searched = error?.detail?.searched

  return (
    <div className="v8r-banner" role="alert">
      <div className="v8r-banner-title">{t('error.title')}: {title}</div>
      {hint !== '' && <div>{hint}</div>}
      {Array.isArray(searched) && searched.length > 0 && (
        <div className="v8r-banner-detail">{searched.join('\n')}</div>
      )}
      {typeof error?.message === 'string' && error.message !== '' && (
        <div className="v8r-banner-detail">{t('error.details')}: {error.message}</div>
      )}
    </div>
  )
}

/**
 * The panel page.
 * @param {{ useTabInfo?: () => { tab?: object }, t?: (key: string, params?: object) => string }} props - slot props.
 * @returns {React.ReactNode} the page.
 */
export function PanelView({ useTabInfo, t: provided }) {
  const t = typeof provided === 'function' ? provided : (key) => key
  const tab = typeof useTabInfo === 'function' ? useTabInfo().tab : undefined

  const [pathInput, setPathInput] = useState(readLastPath)
  const [document, setDocument] = useState(null)
  const [detail, setDetail] = useState(null)
  const [objectName, setObjectName] = useState('')
  const [moduleName, setModuleName] = useState('')
  const [activeSection, setActiveSection] = useState('')
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState(null)
  const controller = useRef(null)

  /**
   * Run one request and hand its document to the caller's setter.
   * @param {{ path: string, object?: string, module?: string }} params - what to read.
   * @param {(body: object) => void} onDone - applies the answer.
   * @param {boolean} [noStore] - bypass the browser cache.
   */
  const run = useCallback(async (params, onDone, noStore = false) => {
    controller.current?.abort()
    const next = new AbortController()
    controller.current = next
    setBusy(true)
    setError(null)
    try {
      const body = await fetchState({ ...params, signal: next.signal, noStore })
      if (next.signal.aborted) return
      onDone(body)
    } catch (failure) {
      if (next.signal.aborted || failure?.name === 'AbortError') return
      setError(failure instanceof StateError
        ? failure
        : new StateError('unknown', String(failure?.message ?? failure), 0))
    } finally {
      if (!next.signal.aborted) setBusy(false)
    }
  }, [])

  const openPath = useCallback((raw) => {
    const value = String(raw ?? '').trim()
    if (value === '') return
    run({ path: value }, (body) => {
      setDocument(body)
      setDetail(null)
      setObjectName('')
      setModuleName('')
      setActiveSection('')
      setPathInput(value)
      writeLastPath(value)
    })
  }, [run])

  const openObject = useCallback((name) => {
    if (document === null) return
    run({ path: document.file.path, object: name }, (body) => {
      setDetail(body)
      setObjectName(name)
      setModuleName('')
      const first = Array.isArray(body.object?.sections) && body.object.sections.length > 0
        ? body.object.sections[0].kind
        : ''
      setActiveSection(first)
    })
  }, [document, run])

  const openModule = useCallback((name) => {
    if (document === null || objectName === '') return
    run({ path: document.file.path, object: objectName, module: name }, (body) => {
      setDetail(body)
      setModuleName(name)
    })
  }, [document, objectName, run])

  const reload = useCallback(() => {
    if (document === null) {
      openPath(pathInput)
      return
    }
    if (objectName === '') {
      openPath(document.file.path)
      return
    }
    if (moduleName !== '') {
      openModule(moduleName)
      return
    }
    openObject(objectName)
  }, [document, moduleName, objectName, openModule, openObject, openPath, pathInput])

  useEffect(() => {
    const actions = tab?.actions
    if (actions === undefined || typeof actions.bindCommands !== 'function') return undefined
    return actions.bindCommands({ refresh: reload })
  }, [tab, reload])

  useEffect(() => () => { controller.current?.abort() }, [])

  const object = detail?.object ?? null
  const content = detail?.content ?? null
  const modules = Array.isArray(object?.modules) ? object.modules : []
  const sections = Array.isArray(object?.sections) ? object.sections : []
  const active = sections.find((section) => section.kind === activeSection) ?? sections[0]

  const moduleText = typeof content?.text === 'string' ? content.text : ''
  const moduleShown = moduleText.length > MODULE_LIMIT ? moduleText.slice(0, MODULE_LIMIT) : moduleText

  return (
    <div className="v8r">
      <style>{PANEL_CSS}</style>

      <div className="v8r-toolbar">
        <input
          className="v8r-input"
          type="text"
          value={pathInput}
          spellCheck={false}
          placeholder={t('panel.path.placeholder')}
          aria-label={t('panel.path.placeholder')}
          onChange={(event) => { setPathInput(event.target.value) }}
          onKeyDown={(event) => {
            if (event.key === 'Enter') openPath(pathInput)
          }}
        />
        <button
          type="button"
          className="v8r-button v8r-button--primary"
          disabled={busy || pathInput.trim() === ''}
          onClick={() => { openPath(pathInput) }}
        >
          {t('panel.open')}
        </button>
        <button
          type="button"
          className="v8r-button"
          disabled={busy || document === null}
          onClick={reload}
        >
          {t('panel.reload')}
        </button>
      </div>

      {busy && <div className="v8r-loading">{t('panel.loading')}</div>}
      {error !== null && <ErrorBanner error={error} translate={t} />}

      {document === null && !busy && error === null && (
        <div className="v8r-empty">
          <div className="v8r-empty-title">{t('panel.empty.title')}</div>
          <div>{t('panel.empty.hint')}</div>
        </div>
      )}

      {document !== null && (
        <div className="v8r-body">
          <Tree
            categories={document.categories}
            selected={objectName}
            onSelect={openObject}
            translate={t}
          />
          <div className="v8r-main">
            <div className="v8r-subtitle">
              {t('panel.file.summary', {
                kind: fileKindLabel(t, document.file?.kind),
                size: formatBytes(document.file?.size),
                count: document.file?.objectCount ?? 0
              })}
              {document.file?.formatVersion !== undefined && (
                <> · {t('panel.file.format', { version: document.file.formatVersion })}</>
              )}
              {Array.isArray(document.file?.warnings) && document.file.warnings.length > 0 && (
                <> · {t('panel.warnings', { count: document.file.warnings.length })}</>
              )}
            </div>

            {object === null && <p className="v8r-muted">{t('panel.selectObject')}</p>}

            {object !== null && (
              <>
                <h3 className="v8r-title">{object.name}</h3>
                <div className="v8r-subtitle">
                  {object.typeTitle}
                  {object.synonym !== undefined && object.synonym !== '' && <> · {object.synonym}</>}
                </div>

                <div className="v8r-tabs" role="tablist">
                  {sections.map((section) => (
                    <button
                      key={section.kind}
                      type="button"
                      role="tab"
                      className="v8r-tab"
                      aria-selected={active !== undefined && section.kind === active.kind ? 'true' : 'false'}
                      onClick={() => { setActiveSection(section.kind) }}
                    >
                      {sectionLabel(t, section)} ({Array.isArray(section.entries) ? section.entries.length : 0})
                    </button>
                  ))}
                </div>

                {active !== undefined && <SectionTable section={active} translate={t} />}

                {modules.length > 0 && (
                  <>
                    <div className="v8r-modules">
                      <span className="v8r-muted">{t('panel.modules')}:</span>
                      {modules.map((module) => (
                        <button
                          key={module.name}
                          type="button"
                          className="v8r-button"
                          aria-pressed={module.name === moduleName ? 'true' : 'false'}
                          disabled={busy}
                          onClick={() => { openModule(module.name) }}
                        >
                          {module.name}
                        </button>
                      ))}
                    </div>
                    {moduleName !== '' && moduleText !== '' && (
                      <>
                        <pre className="v8r-code">{moduleShown}</pre>
                        {moduleText.length > MODULE_LIMIT && (
                          <p className="v8r-muted">
                            {t('panel.module.truncated', {
                              size: MODULE_LIMIT,
                              total: moduleText.length
                            })}
                          </p>
                        )}
                      </>
                    )}
                  </>
                )}
              </>
            )}
          </div>
        </div>
      )}
    </div>
  )
}
