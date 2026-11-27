/**
 * Client half of the plugin: the `v8reader` page type of the right Sidebar and
 * the `.cf` renderer of the Sidebar's document previews.
 *
 * Registration of the page follows the Sidebar's public two-stage path: the
 * type into `ctx.sidebarRightTabs`, then the body into the keyed
 * `sidebar.right.pane.tab` seat under the type's `id`. The guide page offers
 * the type as an entry box, so `+` in the right Sidebar lists it; no resource
 * patterns are claimed — the type has no address of its own.
 *
 * The document renderer is optional: it is registered through
 * `ctx.inject(['documentPreviews'], …)`, so a profile without the Sidebar's
 * document preview keeps the page tab and simply has no Files-tree integration.
 *
 * The theme service is injected for one reason: the code view keeps the BSL
 * palette of the application, which has a light and a dark variant. The active
 * scheme is tracked through `theme/change` and passed to the panel as a prop, so
 * the palette follows the application theme without reading the DOM.
 */
import React, { useEffect, useState } from 'react'
import { DocumentView } from './DocumentView.jsx'
import { PanelView } from './PanelView.jsx'
import { DOCUMENT_EXTENSIONS, DOCUMENT_ID } from './document.js'
import { en, ru } from './messages.js'
import { colorSchemeOf } from './scheme.js'

/** Tab kind this plugin owns. */
export const TYPE_KIND = 'v8reader'

/** This implementation's identity in the tab system, and its body's slot key. */
export const TYPE_ID = '@fishca/dsh-v8reader'

/** Document-preview renderer id; also its body's key in the document seat. */
export { DOCUMENT_EXTENSIONS, DOCUMENT_ID }

/** Copy namespace registered with the Client locale service. */
export const LOCALE_NAMESPACE = 'v8reader'

/** Services the plugin needs before it can register anything. */
export const inject = ['slots', 'locale', 'sidebarRightTabs', 'theme']

/**
 * Register the tab type, its copy, its body, and the document renderer.
 * @param {object} ctx - Client context with the tab registry, slots, locale, and theme.
 */
export function apply(ctx) {
  const t = ctx.locale.bind(LOCALE_NAMESPACE)

  ctx.effect(
    () => ctx.locale.register(LOCALE_NAMESPACE, { ru, en }),
    'dsh-v8reader: dictionaries'
  )

  ctx.effect(
    () => ctx.sidebarRightTabs.register({
      id: TYPE_ID,
      kind: TYPE_KIND,
      priority: 'extension',
      keepMounted: true,
      title: () => t('type.title'),
      guide: [{
        id: 'open',
        order: 10,
        title: () => t('guide.title'),
        description: () => t('guide.description')
      }]
    }),
    'dsh-v8reader: tab type'
  )

  // One small store keeps the wrapper components free of Cordis specifics: the
  // event updates the module value and wakes the mounted panels.
  const listeners = new Set()
  let scheme = 'light'
  try {
    scheme = colorSchemeOf(ctx.theme.getTheme(), scheme)
  } catch {
    // a theme service without a snapshot leaves the light palette
  }

  ctx.effect(
    () => ctx.on('theme/change', (snapshot) => {
      scheme = colorSchemeOf(snapshot, scheme)
      for (const listener of listeners) listener(scheme)
    }),
    'dsh-v8reader: theme scheme'
  )

  /**
   * Follow the application colour scheme from a mounted slot body.
   * @returns {'light'|'dark'} the current scheme.
   */
  function useScheme() {
    const [current, setCurrent] = useState(scheme)
    useEffect(() => {
      listeners.add(setCurrent)
      setCurrent(scheme)
      return () => { listeners.delete(setCurrent) }
    }, [])
    return current
  }

  /**
   * Slot body of the page tab: the panel with the current colour scheme.
   * @param {object} props - slot props forwarded to the panel.
   * @returns {React.ReactNode} the panel.
   */
  function PanelBody(props) {
    return React.createElement(PanelView, { ...props, scheme: useScheme() })
  }

  ctx.effect(
    () => ctx.slots.inject('sidebar.right.pane.tab', () => ctx.slots.register(
      { name: 'sidebar.right.pane.tab', key: TYPE_ID, locale: LOCALE_NAMESPACE },
      PanelBody
    )),
    'dsh-v8reader: tab body'
  )

  /**
   * Slot body of the document preview: the same panel, addressed by the tab.
   * @param {object} props - document body props from the preview owner.
   * @returns {React.ReactNode} the panel.
   */
  function DocumentBody(props) {
    return React.createElement(DocumentView, { ...props, scheme: useScheme() })
  }

  // The document preview is a separate, optional package. Registering through
  // `ctx.inject` keeps this plugin active without it, and disposes the
  // renderer together with the page when it unloads.
  ctx.inject(['documentPreviews', 'slots'], (documentCtx) => {
    documentCtx.effect(
      () => documentCtx.documentPreviews.register({
        id: DOCUMENT_ID,
        extensions: [...DOCUMENT_EXTENSIONS],
        // Every claimed suffix is a container, never text: without this the
        // preview would also offer its plain-text fallback for the file.
        binaryExtensions: [...DOCUMENT_EXTENSIONS],
        priority: 'extension',
        title: () => t('document.title'),
        loading: 'renderer'
      }),
      'dsh-v8reader: document preview'
    )

    documentCtx.effect(
      () => documentCtx.slots.inject('sidebar.right.tab.document', () => documentCtx.slots.register(
        { name: 'sidebar.right.tab.document', key: DOCUMENT_ID, locale: LOCALE_NAMESPACE },
        DocumentBody
      )),
      'dsh-v8reader: document body'
    )
  })
}
