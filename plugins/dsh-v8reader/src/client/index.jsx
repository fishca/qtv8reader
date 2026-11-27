/**
 * Client half of the plugin: the `v8reader` page type of the right Sidebar.
 *
 * Registration follows the Sidebar's public two-stage path: the type into
 * `ctx.sidebarRightTabs`, then the body into the keyed `sidebar.right.pane.tab`
 * seat under the type's `id`. The guide page offers the type as an entry box, so
 * `+` in the right Sidebar lists it; no resource patterns are claimed — the type
 * has no address of its own.
 *
 * The theme service is injected for one reason: the code view keeps the BSL
 * palette of the application, which has a light and a dark variant. The active
 * scheme is tracked through `theme/change` and passed to the panel as a prop, so
 * the palette follows the application theme without reading the DOM.
 */
import React, { useEffect, useState } from 'react'
import { PanelView } from './PanelView.jsx'
import { en, ru } from './messages.js'
import { colorSchemeOf } from './scheme.js'

/** Tab kind this plugin owns. */
export const TYPE_KIND = 'v8reader'

/** This implementation's identity in the tab system, and its body's slot key. */
export const TYPE_ID = '@fishca/dsh-v8reader'

/** Copy namespace registered with the Client locale service. */
export const LOCALE_NAMESPACE = 'v8reader'

/** Services the plugin needs before it can register anything. */
export const inject = ['slots', 'locale', 'sidebarRightTabs', 'theme']

/**
 * Register the tab type, its copy, and its body.
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

  // One small store keeps the wrapper component free of Cordis specifics: the
  // event updates the module value and wakes the mounted panel.
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
   * Slot body: the panel with the current colour scheme.
   * @param {object} props - slot props forwarded to the panel.
   * @returns {React.ReactNode} the panel.
   */
  function PanelBody(props) {
    const [current, setCurrent] = useState(scheme)
    useEffect(() => {
      listeners.add(setCurrent)
      setCurrent(scheme)
      return () => { listeners.delete(setCurrent) }
    }, [])
    return React.createElement(PanelView, { ...props, scheme: current })
  }

  ctx.effect(
    () => ctx.slots.inject('sidebar.right.pane.tab', () => ctx.slots.register(
      { name: 'sidebar.right.pane.tab', key: TYPE_ID, locale: LOCALE_NAMESPACE },
      PanelBody
    )),
    'dsh-v8reader: tab body'
  )
}
