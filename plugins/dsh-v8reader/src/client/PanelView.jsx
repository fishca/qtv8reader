/**
 * The right-Sidebar page that reads one 1C:Enterprise container.
 *
 * The panel owns no data of its own: every object screen is one `state` request
 * the Host serves from its cache, modules are code with BSL highlighting, and a
 * form is the application's own exact-view renderer served as a page and shown
 * in an iframe. The metadata tree is hierarchical: configuration → category →
 * object → section → entry, filled from the same documents.
 */
import React, { useCallback, useEffect, useRef, useState } from 'react'
import { StateError, buildFormUrl, fetchState } from './api.js'
import { CodeView } from './CodeView.jsx'
import { FilePicker } from './FilePicker.jsx'
import { FormPreview } from './FormPreview.jsx'
import { fileKindLabel, flattenEntries, formatBytes, sectionLabel } from './labels.js'
import { dirOf } from './paths.js'
import { PANEL_CSS } from './styles.js'
import { MODULES_GROUP, sameObject } from './tree.js'
import { Tree } from './Tree.jsx'

/** Key of the last opened path; only the path is remembered, never file contents. */
export const LAST_PATH_KEY = 'dsh.v8reader.path.v1'

/** Key of the directory the file chooser starts in. */
export const LAST_DIR_KEY = 'dsh.v8reader.dir.v1'

/** Key of the form-view preferences (zoom). */
export const FORM_SETTINGS_KEY = 'dsh.v8reader.form.v1'


/** Zoom steps of the form view, in percent. */
export const ZOOM_STEPS = Object.freeze([50, 75, 100, 125, 150])

/** Zoom used when nothing was stored. */
export const DEFAULT_ZOOM = 100

/**
 * Read a remembered value.
 * @param {string} key - the storage key.
 * @returns {string} the stored string, or an empty one.
 */
function readStored(key) {
  try {
    return globalThis.localStorage?.getItem(key) ?? ''
  } catch {
    return ''
  }
}

/**
 * Remember a value, ignoring storage failures (private windows, quota).
 * @param {string} key - the storage key.
 * @param {string} value - what to remember.
 */
function writeStored(key, value) {
  try {
    globalThis.localStorage?.setItem(key, value)
  } catch {
    // remembering a path is a convenience
  }
}

/**
 * Read the remembered path.
 * @returns {string} the last opened absolute path, or an empty string.
 */
function readLastPath() {
  return readStored(LAST_PATH_KEY)
}

/**
 * Read the remembered form zoom, clamped to a known step.
 * @returns {number} the zoom in percent.
 */
function readZoom() {
  const raw = readStored(FORM_SETTINGS_KEY)
  if (raw === '') return DEFAULT_ZOOM
  try {
    const parsed = JSON.parse(raw)
    const zoom = Number(parsed?.zoom)
    if (!Number.isFinite(zoom) || !ZOOM_STEPS.includes(zoom)) return DEFAULT_ZOOM
    return zoom
  } catch {
    return DEFAULT_ZOOM
  }
}

/**
 * Remember the form zoom.
 * @param {number} zoom - zoom in percent.
 */
function writeZoom(zoom) {
  writeStored(FORM_SETTINGS_KEY, JSON.stringify({ zoom }))
}

/**
 * The module view: object context, the module switcher, and the highlighted code.
 * @param {{
 *   object: object,
 *   modules: object[],
 *   moduleName: string,
 *   text: string,
 *   loaded: boolean,
 *   busy: boolean,
 *   scheme: 'light'|'dark',
 *   translate: (key: string, params?: object) => string,
 *   onBack: () => void,
 *   onOpenModule: (name: string) => void
 * }} props - the module and its context.
 * @returns {React.ReactNode} the module screen.
 */
function ModuleView({
  object,
  modules,
  moduleName,
  text,
  loaded,
  busy,
  scheme,
  translate: t,
  onBack,
  onOpenModule
}) {
  return (
    <>
      <div className="v8r-viewhead">
        <button type="button" className="v8r-button" onClick={onBack}>{`← ${t('panel.back')}`}</button>
        <div className="v8r-viewhead-text">
          <h3 className="v8r-viewhead-title">{moduleName}</h3>
          <div className="v8r-viewhead-sub">{object.name}</div>
        </div>
      </div>

      <div className="v8r-modules">
        <span className="v8r-muted">{t('panel.modules')}:</span>
        {modules.map((module) => (
          <button
            key={module.name}
            type="button"
            className="v8r-button"
            aria-pressed={module.name === moduleName ? 'true' : 'false'}
            disabled={busy}
            onClick={() => { onOpenModule(module.name) }}
          >
            {module.name}
          </button>
        ))}
      </div>

      {!loaded && !busy && <p className="v8r-muted">{t('panel.objectUnavailable')}</p>}
      {loaded && text === '' && !busy && <p className="v8r-muted">{t('panel.module.empty')}</p>}
      {loaded && text !== '' && <CodeView text={text} scheme={scheme} translate={t} />}
    </>
  )
}

/**
 * The exact-form screen: header with zoom, then the renderer page.
 * @param {{
 *   object: object,
 *   formName: string,
 *   src: string,
 *   zoom: number,
 *   nonce: number,
 *   translate: (key: string, params?: object) => string,
 *   onBack: () => void,
 *   onZoom: (zoom: number) => void,
 *   onReload: () => void
 * }} props - the form page and its controls.
 * @returns {React.ReactNode} the form screen.
 */
function FormView({
  object,
  formName,
  src,
  zoom,
  nonce,
  translate: t,
  onBack,
  onZoom,
  onReload
}) {
  return (
    <>
      <div className="v8r-viewhead">
        <button type="button" className="v8r-button" onClick={onBack}>{`← ${t('panel.back')}`}</button>
        <div className="v8r-viewhead-text">
          <h3 className="v8r-viewhead-title">{formName}</h3>
          <div className="v8r-viewhead-sub">{object.name}</div>
        </div>
        <span className="v8r-zoom">
          <span>{t('form.zoom')}</span>
          <select
            className="v8r-select"
            aria-label={t('form.zoom')}
            value={String(zoom)}
            onChange={(event) => { onZoom(Number(event.target.value)) }}
          >
            {ZOOM_STEPS.map((step) => (
              <option key={step} value={String(step)}>{`${step}%`}</option>
            ))}
          </select>
          <button type="button" className="v8r-button" onClick={onReload}>{t('panel.reload')}</button>
        </span>
      </div>

      <FormPreview
        key={`${src}#${nonce}`}
        src={src}
        title={formName}
        zoom={zoom}
        nonce={nonce}
        translate={t}
        onRetry={onReload}
      />
    </>
  )
}

/**
 * One section as a table of name, synonym, and type.
 *
 * The Forms branch is the one place where a row is actionable: a form name
 * opens the exact-view screen, which is where the composition and the renderer
 * meet. Other rows stay read-only.
 * @param {{
 *   section: object,
 *   translate: (key: string, params?: object) => string,
 *   onOpenForm?: (name: string) => void
 * }} props - the section.
 * @returns {React.ReactNode} the table.
 */
function SectionTable({ section, translate: t, onOpenForm }) {
  const rows = flattenEntries(Array.isArray(section.entries) ? section.entries : [])
  if (rows.length === 0) return <p className="v8r-muted">{t('panel.sectionEmpty')}</p>
  const forms = section.kind === 'forms' && typeof onOpenForm === 'function'

  return (
    <table className="v8r-table">
      <thead>
        <tr>
          <th>{t('panel.column.name')}</th>
          <th>{t('panel.column.synonym')}</th>
          <th>{t('panel.column.type')}</th>
          {forms && <th>{t('panel.openForm')}</th>}
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
            {forms && (
              <td>
                <button
                  type="button"
                  className="v8r-button"
                  title={t('panel.openForm')}
                  onClick={() => { onOpenForm(entry.name) }}
                >
                  {t('panel.openForm')}
                </button>
              </td>
            )}
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
 * @param {{
 *   useTabInfo?: () => { tab?: object },
 *   t?: (key: string, params?: object) => string,
 *   scheme?: 'light'|'dark'
 * }} props - slot props and the current theme scheme.
 * @returns {React.ReactNode} the page.
 */
export function PanelView({ useTabInfo, t: provided, scheme = 'light' }) {
  const t = typeof provided === 'function' ? provided : (key) => key
  const tab = typeof useTabInfo === 'function' ? useTabInfo().tab : undefined

  const [pathInput, setPathInput] = useState(readLastPath)
  const [lastDir, setLastDir] = useState(() => readStored(LAST_DIR_KEY))
  const [pickerOpen, setPickerOpen] = useState(false)
  const [document, setDocument] = useState(null)
  const [detail, setDetail] = useState(null)
  const [selected, setSelected] = useState(null)
  const [viewMode, setViewMode] = useState('object')
  const [moduleName, setModuleName] = useState('')
  const [loadedModule, setLoadedModule] = useState('')
  const [formName, setFormName] = useState('')
  const [formNonce, setFormNonce] = useState(0)
  const [zoom, setZoom] = useState(readZoom)
  const [activeSection, setActiveSection] = useState('')
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState(null)
  const controller = useRef(null)
  const browseButton = useRef(null)

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
    // The field shows what was asked for even when the answer is an error, so
    // the user can fix the path instead of wondering which file was attempted.
    setPathInput(value)
    run({ path: value }, (body) => {
      setDocument(body)
      setDetail(null)
      setSelected(null)
      setModuleName('')
      setLoadedModule('')
      setFormName('')
      setViewMode('object')
      setActiveSection('')
      writeStored(LAST_PATH_KEY, value)
      const dir = dirOf(value)
      if (dir !== '') {
        setLastDir(dir)
        writeStored(LAST_DIR_KEY, dir)
      }
    })
  }, [run])

  /**
   * The toolbar's open action: an empty field is not a dead end, it opens the
   * chooser, because a typed path is only one of the two ways to pick a file.
   */
  const openFromToolbar = useCallback(() => {
    if (String(pathInput ?? '').trim() === '') {
      setPickerOpen(true)
      return
    }
    openPath(pathInput)
  }, [openPath, pathInput])

  const openChooser = useCallback(() => { setPickerOpen(true) }, [])

  /**
   * Close the chooser, remembering the directory it was left in and returning
   * focus to the control that opened it.
   * @param {string} [finalDir] - the directory the chooser ended in.
   */
  const closeChooser = useCallback((finalDir = '') => {
    setPickerOpen(false)
    if (finalDir !== '') {
      setLastDir(finalDir)
      writeStored(LAST_DIR_KEY, finalDir)
    }
    browseButton.current?.focus?.()
  }, [])

  const pickFromChooser = useCallback((picked) => {
    setPickerOpen(false)
    openPath(picked)
  }, [openPath])

  const openObject = useCallback((object) => {
    if (document === null || object === undefined || object === null) return
    // GUID is the only unambiguous query: the same name may exist in two
    // categories of a large configuration.
    const query = typeof object.guid === 'string' && object.guid !== '' ? object.guid : object.name
    setSelected({ name: object.name, guid: object.guid ?? '' })
    setModuleName('')
    setLoadedModule('')
    setFormName('')
    setViewMode('object')
    run({ path: document.file.path, object: query }, (body) => {
      setDetail(body)
      const first = Array.isArray(body.object?.sections) && body.object.sections.length > 0
        ? body.object.sections[0].kind
        : ''
      setActiveSection(first)
    })
  }, [document, run])

  const openModule = useCallback((name) => {
    if (document === null || selected === null) return
    const query = selected.guid !== '' ? selected.guid : selected.name
    setModuleName(name)
    setLoadedModule('')
    setFormName('')
    setViewMode('module')
    run({ path: document.file.path, object: query, module: name }, (body) => {
      setDetail(body)
      setLoadedModule(name)
    })
  }, [document, selected, run])

  const openForm = useCallback((name) => {
    if (document === null || detail === null || detail.object === null) return
    if (!sameObject(detail.object, selected)) return
    setError(null)
    setFormName(name)
    setModuleName('')
    setLoadedModule('')
    setViewMode('form')
  }, [document, detail, selected])

  const showObject = useCallback((kind) => {
    if (kind !== undefined && kind !== '') setActiveSection(kind)
    setError(null)
    setModuleName('')
    setLoadedModule('')
    setFormName('')
    setViewMode('object')
  }, [])

  const reload = useCallback(() => {
    if (document === null) {
      if (String(pathInput ?? '').trim() === '') return
      openPath(pathInput)
      return
    }
    if (viewMode === 'form') {
      setFormNonce((current) => current + 1)
      return
    }
    if (viewMode === 'module') {
      if (moduleName !== '') openModule(moduleName)
      return
    }
    if (selected === null) {
      openPath(document.file.path)
      return
    }
    openObject(selected)
  }, [document, moduleName, openModule, openObject, openPath, pathInput, selected, viewMode])

  useEffect(() => {
    const actions = tab?.actions
    if (actions === undefined || typeof actions.bindCommands !== 'function') return undefined
    return actions.bindCommands({ refresh: reload })
  }, [tab, reload])

  useEffect(() => () => { controller.current?.abort() }, [])

  const onZoom = useCallback((value) => {
    setZoom(value)
    writeZoom(value)
  }, [])

  const object = detail?.object ?? null
  const content = detail?.content ?? null
  const modules = Array.isArray(object?.modules) ? object.modules : []
  const sections = Array.isArray(object?.sections) ? object.sections : []
  const active = sections.find((section) => section.kind === activeSection) ?? sections[0]

  // `detail` can still describe the previous object while the request for the
  // new one is in flight or failed. Rendering it under the new selection would
  // show A's sections for B, so every detail view is gated by this check.
  const detailMatches = sameObject(object, selected)
  const moduleText = detailMatches && loadedModule === moduleName && typeof content?.text === 'string'
    ? content.text
    : ''
  const hasPath = String(pathInput ?? '').trim() !== ''
  const selectedQuery = selected === null ? '' : (selected.guid !== '' ? selected.guid : selected.name)
  const modulesTab = activeSection === MODULES_GROUP && modules.length > 0

  return (
    <div className="v8r" data-scheme={scheme}>
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
            if (event.key === 'Enter') openFromToolbar()
          }}
        />
        <button
          type="button"
          className={hasPath ? 'v8r-button v8r-button--primary' : 'v8r-button'}
          disabled={busy}
          title={t('panel.open.title')}
          onClick={openFromToolbar}
        >
          {t('panel.open')}
        </button>
        <button
          type="button"
          className={hasPath ? 'v8r-button' : 'v8r-button v8r-button--primary'}
          title={t('panel.browse.title')}
          ref={browseButton}
          onClick={openChooser}
        >
          {t('panel.browse')}
        </button>
        {document !== null && (
          <button
            type="button"
            className="v8r-button"
            disabled={busy}
            onClick={reload}
          >
            {t('panel.reload')}
          </button>
        )}
      </div>

      {busy && (
        <div className="v8r-loading" role="status">
          {document !== null ? t('panel.loading.object') : t('panel.loading')}
        </div>
      )}
      {error !== null && <ErrorBanner error={error} translate={t} />}

      {document === null && !busy && error === null && !pickerOpen && (
        <div className="v8r-empty">
          <div className="v8r-empty-title">{t('panel.empty.title')}</div>
          <div>{t('panel.empty.hint')}</div>
          <button
            type="button"
            className="v8r-button v8r-button--primary"
            onClick={openChooser}
          >
            {t('panel.chooseFile')}
          </button>
        </div>
      )}

      {document !== null && (
        <div className="v8r-body">
          <Tree
            key={document.file?.path ?? 'tree'}
            file={document.file}
            categories={document.categories}
            selected={selected}
            detail={detail}
            viewMode={viewMode}
            activeSection={activeSection}
            moduleName={moduleName}
            formName={formName}
            busy={busy}
            onSelect={openObject}
            onSection={showObject}
            onOpenForm={openForm}
            onOpenModule={openModule}
            translate={t}
          />
          <div className="v8r-main" data-view={viewMode}>
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

            {viewMode === 'object' && selected === null && (
              <p className="v8r-muted">{t('panel.selectObject')}</p>
            )}

            {viewMode === 'object' && selected !== null && !detailMatches && (
              <>
                <h3 className="v8r-title">{selected.name}</h3>
                <p className="v8r-muted">
                  {busy ? t('panel.loading.object') : t('panel.objectUnavailable')}
                </p>
              </>
            )}

            {viewMode === 'object' && detailMatches && object !== null && (
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
                      aria-selected={!modulesTab && active !== undefined && section.kind === active.kind ? 'true' : 'false'}
                      onClick={() => { setActiveSection(section.kind) }}
                    >
                      {sectionLabel(t, section)} ({Array.isArray(section.entries) ? section.entries.length : 0})
                    </button>
                  ))}
                  {modules.length > 0 && (
                    <button
                      type="button"
                      role="tab"
                      className="v8r-tab"
                      aria-selected={modulesTab ? 'true' : 'false'}
                      onClick={() => { setActiveSection(MODULES_GROUP) }}
                    >
                      {t('panel.modules')} ({modules.length})
                    </button>
                  )}
                </div>

                {modulesTab ? (
                  <div className="v8r-modules">
                    {modules.map((module) => (
                      <button
                        key={module.name}
                        type="button"
                        className="v8r-button"
                        disabled={busy}
                        onClick={() => { openModule(module.name) }}
                      >
                        {module.name}
                      </button>
                    ))}
                  </div>
                ) : active !== undefined ? (
                  <SectionTable
                    section={active}
                    translate={t}
                    onOpenForm={openForm}
                  />
                ) : null}
              </>
            )}

            {viewMode === 'module' && detailMatches && object !== null && (
              <ModuleView
                object={object}
                modules={modules}
                moduleName={moduleName}
                text={moduleText}
                loaded={detailMatches && loadedModule === moduleName}
                busy={busy}
                scheme={scheme}
                translate={t}
                onBack={() => { showObject() }}
                onOpenModule={openModule}
              />
            )}

            {viewMode === 'module' && !detailMatches && (
              <p className="v8r-muted">
                {busy ? t('panel.loading.object') : t('panel.objectUnavailable')}
              </p>
            )}

            {viewMode === 'form' && detailMatches && object !== null && (
              <FormView
                object={object}
                formName={formName}
                src={buildFormUrl(document.file.path, selectedQuery, formName)}
                zoom={zoom}
                nonce={formNonce}
                translate={t}
                onBack={() => { showObject() }}
                onZoom={onZoom}
                onReload={reload}
              />
            )}

            {viewMode === 'form' && !detailMatches && (
              <p className="v8r-muted">
                {busy ? t('panel.loading.object') : t('panel.objectUnavailable')}
              </p>
            )}
          </div>
        </div>
      )}

      {pickerOpen && (
        <FilePicker
          initialDir={lastDir}
          translate={t}
          onPick={pickFromChooser}
          onClose={closeChooser}
        />
      )}
    </div>
  )
}
