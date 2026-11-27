/**
 * Reading the active colour scheme from a theme snapshot.
 *
 * The panel only needs `light` or `dark` to pick a BSL palette; everything else
 * about the theme comes from CSS tokens. Kept plain so the Node tests can check
 * the shape without loading the JSX entry.
 */

/**
 * Read the colour scheme of one theme snapshot.
 * @param {{ active?: { colorScheme?: string }, colorScheme?: string }|undefined} snapshot - a `ThemeSnapshot` or a bare theme definition.
 * @param {'light'|'dark'} [fallback] - scheme used for an unknown snapshot.
 * @returns {'light'|'dark'} the active scheme.
 */
export function colorSchemeOf(snapshot, fallback = 'light') {
  const scheme = snapshot?.active?.colorScheme ?? snapshot?.colorScheme
  if (scheme === 'dark') return 'dark'
  if (scheme === 'light') return 'light'
  return fallback
}
