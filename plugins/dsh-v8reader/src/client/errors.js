/**
 * Localization of the panel's failures.
 *
 * The Host answers one error vocabulary for both routes; the chooser needs
 * `bad-path` split further, because the same code covers a rejected file and a
 * folder that is gone.
 */

/**
 * Localize one chooser failure.
 * @param {(key: string, params?: object) => string} t - translate.
 * @param {Error & { code?: string, detail?: object }} error - the failure.
 * @returns {string} the message for the banner.
 */
export function browseErrorText(t, error) {
  const code = typeof error?.code === 'string' ? error.code : 'unknown'
  const reason = error?.detail?.reason
  if (code === 'bad-path') {
    if (reason === 'directory-missing') return t('picker.error.missing')
    if (reason === 'not-a-directory') return t('picker.error.notDirectory')
    return t('error.bad-path')
  }
  const key = `error.${code}`
  const localized = t(key, error?.detail ?? {})
  return localized === key ? t('error.unknown') : localized
}
