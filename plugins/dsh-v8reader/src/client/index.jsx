/**
 * Client half of the plugin: the `v8reader` page type of the right Sidebar.
 *
 * Registration follows the Sidebar's public two-stage path: the type into
 * `ctx.sidebarRightTabs`, then the body into the keyed `sidebar.right.pane.tab`
 * seat under the type's `id`. The guide page offers the type as an entry box, so
 * `+` in the right Sidebar lists it; no resource patterns are claimed — the type
 * has no address of its own.
 */
import { PanelView } from './PanelView.jsx'
import { en, ru } from './messages.js'

/** Tab kind this plugin owns. */
export const TYPE_KIND = 'v8reader'

/** This implementation's identity in the tab system, and its body's slot key. */
export const TYPE_ID = '@fishca/dsh-v8reader'

/** Copy namespace registered with the Client locale service. */
export const LOCALE_NAMESPACE = 'v8reader'

/** Services the plugin needs before it can register anything. */
export const inject = ['slots', 'locale', 'sidebarRightTabs']

/**
 * Register the tab type, its copy, and its body.
 * @param {object} ctx - Client context with the tab registry, slots, and locale.
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

  ctx.effect(
    () => ctx.slots.inject('sidebar.right.pane.tab', () => ctx.slots.register(
      { name: 'sidebar.right.pane.tab', key: TYPE_ID, locale: LOCALE_NAMESPACE },
      PanelView
    )),
    'dsh-v8reader: tab body'
  )
}
