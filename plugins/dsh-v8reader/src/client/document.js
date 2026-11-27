/**
 * The document-preview half: constants and pure helpers of the `.cf` renderer.
 *
 * The Sidebar's document preview selects a renderer per file suffix and mounts
 * its body in the keyed `sidebar.right.tab.document` seat. The metadata tree is
 * the same panel as the `1С` page, so the body only has to resolve the file's
 * Host path and forward the owner's loading contract.
 */

/** Unique implementation id; also the document slot key of its body. */
export const DOCUMENT_ID = '@fishca/dsh-v8reader/config'

/** Container suffixes the renderer claims, without the leading dot. */
export const DOCUMENT_EXTENSIONS = Object.freeze(['cf', 'cfe', 'epf', 'erf'])

/**
 * Absolute Host path of the previewed file, if the resource provider knows it.
 *
 * The document preview addresses a file as a Session resource; its metadata
 * carries the Host path once it has been resolved. A renderer that gets an
 * address but no path yet must wait instead of guessing one.
 * @param {{ value?: { absolutePath?: unknown } }|null|undefined} meta - `useResource('file', …)` snapshot.
 * @returns {string} the absolute path, or an empty string.
 */
export function documentPath(meta) {
  const value = meta?.value
  const absolutePath = value?.absolutePath
  return typeof absolutePath === 'string' ? absolutePath : ''
}

/**
 * Version tag of the previewed file, if the resource provider reported one.
 * @param {{ value?: { version?: unknown } }|null|undefined} meta - `useResource('file', …)` snapshot.
 * @returns {string} the version, or an empty string.
 */
export function documentVersion(meta) {
  const value = meta?.value
  return typeof value?.version === 'string' ? value.version : ''
}
