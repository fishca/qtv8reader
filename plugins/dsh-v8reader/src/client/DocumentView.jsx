/**
 * The Sidebar's document-preview body for `.cf`, `.cfe`, `.epf` and `.erf`.
 *
 * The preview owner addresses a file as a Session resource and hands the body
 * the owner's loading contract (`content`). The panel is the same one the `1С`
 * tab shows; here the file comes from the tab, so the path is resolved from the
 * resource's metadata (the Host path is not part of the address) and the panel
 * opens it without the path field. A file whose Host path is still being
 * resolved is explained rather than silently empty, and the owner's reload —
 * one new `revision` — refetches the screen that is open.
 */
import React from 'react'
import { PanelView } from './PanelView.jsx'
import { documentPath, documentVersion } from './document.js'
import { PANEL_CSS } from './styles.js'

/**
 * The preview body.
 * @param {{
 *   useTabInfo: () => { tab: { contentId: string } },
 *   useResource: (kind: string, address: string) => object,
 *   content?: object,
 *   t?: (key: string, params?: object) => string,
 *   scheme?: 'light'|'dark'
 * }} props - framework hooks, the owner's renderer contract, copy and scheme.
 * @returns {React.ReactNode} the panel, or the reason there is none yet.
 */
export function DocumentView({ useTabInfo, useResource, content, t: provided, scheme = 'light' }) {
  const t = typeof provided === 'function' ? provided : (key) => key
  const tab = useTabInfo().tab
  const meta = useResource('file', tab.contentId)
  const path = documentPath(meta)

  if (path === '') {
    return (
      <div className="v8r v8r--document" data-scheme={scheme}>
        <style>{PANEL_CSS}</style>
        <p className="v8r-muted v8r-document-wait">{t('document.waiting')}</p>
      </div>
    )
  }

  return (
    <PanelView
      t={t}
      scheme={scheme}
      useTabInfo={useTabInfo}
      fixedPath={path}
      documentVersion={documentVersion(meta)}
      documentContent={content}
      refreshToken={typeof content?.revision === 'number' ? content.revision : undefined}
    />
  )
}
