/**
 * The module code view: BSL syntax highlighting, line numbers, and the few
 * reading preferences that are worth remembering.
 *
 * The text arrives already bounded by the panel; this component never asks the
 * Host for anything. Preferences live in `localStorage` under one key and are
 * copied into state on mount, so a broken or absent storage only loses them.
 */
import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { tokenizeLine } from './bsl.js'
import {
  MAX_FONT_SIZE,
  MIN_FONT_SIZE,
  clampNumber,
  readCodeSettings,
  writeCodeSettings
} from './code.js'

/** Lines rendered before the first "show more" step. */
export const LINE_STEP = 1500

/**
 * Copy text to the clipboard, with an execCommand fallback for older surfaces.
 * @param {string} text - what to copy.
 * @returns {Promise<boolean>} true when the copy was attempted successfully.
 */
export async function copyText(text) {
  try {
    if (navigator.clipboard?.writeText !== undefined) {
      await navigator.clipboard.writeText(text)
      return true
    }
  } catch {
    // fall through to the textarea path
  }
  try {
    const area = document.createElement('textarea')
    area.value = text
    area.setAttribute('readonly', '')
    area.style.position = 'fixed'
    area.style.opacity = '0'
    document.body.appendChild(area)
    area.select()
    const copied = document.execCommand('copy')
    area.remove()
    return copied
  } catch {
    return false
  }
}

/**
 * The code view.
 * @param {{
 *   text: string,
 *   scheme?: 'light' | 'dark',
 *   translate: (key: string, params?: object) => string
 * }} props - the module text and copy.
 * @returns {React.ReactNode} the view.
 */
export function CodeView({ text, scheme = 'light', translate: t }) {
  const [settings, setSettings] = useState(() => readCodeSettings())
  const [visible, setVisible] = useState(LINE_STEP)
  const [copied, setCopied] = useState(false)
  const timer = useRef(undefined)

  const source = typeof text === 'string' ? text : ''
  const lines = useMemo(() => source.replace(/\r\n?/g, '\n').split('\n'), [source])
  const gutterWidth = `${String(lines.length).length + 0.5}ch`
  const shown = Math.min(visible, lines.length)
  const hasMore = lines.length > shown

  // Only the rendered window is tokenized: a two-megabyte module is split into
  // lines cheaply, and one "show more" step tokenizes one more portion instead
  // of the whole file on every render.
  const tokens = useMemo(
    () => (settings.enabled ? lines.slice(0, shown).map(tokenizeLine) : []),
    [lines, shown, settings.enabled]
  )

  useEffect(() => {
    setVisible(LINE_STEP)
    setCopied(false)
  }, [source])

  useEffect(() => () => {
    if (timer.current !== undefined) clearTimeout(timer.current)
  }, [])

  /** Persist one preference change and apply it. */
  const change = useCallback((patch) => {
    setSettings((current) => {
      const next = { ...current, ...patch }
      writeCodeSettings(next)
      return next
    })
  }, [])

  const onCopy = useCallback(async () => {
    const done = await copyText(source)
    setCopied(done)
    if (timer.current !== undefined) clearTimeout(timer.current)
    timer.current = setTimeout(() => { setCopied(false) }, 1600)
  }, [source])

  return (
    <div className="v8r-codeview" data-scheme={scheme} data-wrap={settings.wrap ? 'true' : 'false'}>
      <div className="v8r-codeview-bar">
        <button
          type="button"
          className="v8r-button v8r-button--ghost"
          aria-pressed={settings.enabled ? 'true' : 'false'}
          title={t('code.highlight.title')}
          onClick={() => { change({ enabled: !settings.enabled }) }}
        >
          {t('code.highlight')}
        </button>
        <button
          type="button"
          className="v8r-button v8r-button--ghost"
          aria-pressed={settings.wrap ? 'true' : 'false'}
          title={t('code.wrap.title')}
          onClick={() => { change({ wrap: !settings.wrap }) }}
        >
          {t('code.wrap')}
        </button>
        <span className="v8r-codeview-size">
          <button
            type="button"
            className="v8r-button v8r-button--ghost"
            title={t('code.font.smaller')}
            aria-label={t('code.font.smaller')}
            disabled={settings.size <= MIN_FONT_SIZE}
            onClick={() => { change({ size: clampNumber(settings.size - 1, settings.size, MIN_FONT_SIZE, MAX_FONT_SIZE) }) }}
          >
            −
          </button>
          <span className="v8r-codeview-size-value">{settings.size}</span>
          <button
            type="button"
            className="v8r-button v8r-button--ghost"
            title={t('code.font.larger')}
            aria-label={t('code.font.larger')}
            disabled={settings.size >= MAX_FONT_SIZE}
            onClick={() => { change({ size: clampNumber(settings.size + 1, settings.size, MIN_FONT_SIZE, MAX_FONT_SIZE) }) }}
          >
            +
          </button>
        </span>
        <span className="v8r-codeview-spacer" />
        <span className="v8r-muted">{t('code.lines', { count: lines.length })}</span>
        <button
          type="button"
          className="v8r-button"
          onClick={() => { onCopy() }}
        >
          {copied ? t('code.copied') : t('code.copy')}
        </button>
      </div>

      <div
        className="v8r-code"
        style={{ '--v8r-code-size': `${settings.size}px`, '--v8r-gutter': gutterWidth }}
        tabIndex={0}
        role="region"
        aria-label={t('code.region')}
      >
        {Array.from({ length: shown }, (_, index) => {
          const lineTokens = tokens[index] ?? []
          return (
            <div className="v8r-code-row" key={index}>
              <span className="v8r-code-num" aria-hidden="true">{index + 1}</span>
              <span className="v8r-code-text">
                {settings.enabled
                  ? lineTokens.map((token, tokenIndex) => (
                    <span className={`v8r-tok v8r-tok-${token.type}`} key={tokenIndex}>{token.text}</span>
                  ))
                  : lines[index]}
              </span>
            </div>
          )
        })}
        {hasMore && (
          <div className="v8r-code-more">
            <button
              type="button"
              className="v8r-button"
              onClick={() => { setVisible((current) => current + LINE_STEP) }}
            >
              {t('code.showMore', { rest: lines.length - shown })}
            </button>
          </div>
        )}
      </div>
    </div>
  )
}
