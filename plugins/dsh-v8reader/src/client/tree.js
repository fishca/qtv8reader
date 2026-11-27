/**
 * Pure helpers of the metadata tree.
 *
 * The tree mirrors the 1C configuration window: a configuration root, one
 * branch per metadata category, one node per object, and under the selected
 * object its sections (attributes, tabular sections, forms, commands,
 * templates) with the entries themselves. Only the selected object's detail is
 * ever loaded, so these functions work on one `view` document at a time; they
 * are plain JavaScript for the Node tests.
 */
import { flattenEntries } from './labels.js'

/** Pseudo-section holding the object's module list. */
export const MODULES_GROUP = 'modules'

/**
 * Stable identity of one metadata object.
 * @param {object} object - a `categories[].objects[]` entry or `object` of the detail.
 * @returns {string} the GUID when present, otherwise the name.
 */
export function objectKey(object) {
  if (object === null || typeof object !== 'object') return ''
  const guid = typeof object.guid === 'string' ? object.guid.trim() : ''
  if (guid !== '') return guid
  return String(object.name ?? '')
}

/**
 * Whether a detail object is the selected one.
 *
 * The GUID wins when the selection has one; an object chosen by name only is
 * compared by name, because its list entry may legitimately carry a GUID the
 * detail document repeats. Same rule as the tree's highlighted row.
 * @param {object|null|undefined} object - the `object` of a detail document.
 * @param {{ name: string, guid?: string }|null|undefined} selected - the current selection.
 * @returns {boolean} true when both denote the same metadata object.
 */
export function sameObject(object, selected) {
  if (object === null || object === undefined) return false
  if (selected === null || selected === undefined) return false
  if (typeof selected.guid === 'string' && selected.guid !== '') {
    return object.guid === selected.guid
  }
  return object.name === selected.name
}

/**
 * Whether an entry matches the filter by name or synonym.
 * @param {{ name?: string, synonym?: string }} entry - a tree entry.
 * @param {string} query - lowercase filter text, empty matches everything.
 * @returns {boolean} true when the entry stays visible.
 */
export function matchesQuery(entry, query) {
  if (query === '') return true
  const name = String(entry?.name ?? '').toLowerCase()
  const synonym = String(entry?.synonym ?? '').toLowerCase()
  return name.includes(query) || synonym.includes(query)
}

/**
 * Filter and count the categories for the current query.
 * @param {object[]} categories - `categories[]` of the `view` document.
 * @param {string} query - lowercase filter text.
 * @returns {Array<{ category: object, items: object[], total: number }>} visible categories.
 */
export function filterCategories(categories, query) {
  const source = Array.isArray(categories) ? categories : []
  const groups = source.map((category) => {
    const objects = Array.isArray(category.objects) ? category.objects : []
    const items = query === '' ? objects : objects.filter((object) => matchesQuery(object, query))
    return { category, items, total: items.length }
  })
  if (query === '') return groups
  return groups.filter((group) => group.total > 0)
}

/**
 * Build the section branches shown under an object.
 *
 * Every authored section becomes a branch, including an empty one: the 1C tree
 * also shows “Реквизиты” before there is anything in it. Modules are appended as
 * their own branch because the CLI reports them on the object, not as a section.
 * @param {object|null} object - the `object` field of a detail document.
 * @returns {Array<{ kind: string, title: string, entries: object[] }>} branches in display order.
 */
export function objectGroups(object) {
  if (object === null || typeof object !== 'object') return []
  const groups = []
  const sections = Array.isArray(object.sections) ? object.sections : []
  for (const section of sections) {
    const kind = String(section.kind ?? '')
    if (kind === '') continue
    const token = kind === 'forms' ? 'form' : kind === 'templates' ? 'template' : 'entry'
    const entries = (Array.isArray(section.entries) ? section.entries : []).map((entry) => ({
      ...entry,
      token
    }))
    groups.push({
      kind,
      title: String(section.title ?? kind),
      entries
    })
  }

  const modules = Array.isArray(object.modules) ? object.modules : []
  if (modules.length > 0) {
    groups.push({
      kind: MODULES_GROUP,
      title: '',
      entries: modules.map((module) => ({
        name: String(module.name ?? ''),
        synonym: '',
        typeText: '',
        token: 'module'
      }))
    })
  }
  return groups
}

/**
 * Filter nested entries, keeping the parents of a matching child.
 * @param {object[]} entries - entries to filter.
 * @param {string} query - lowercase filter text.
 * @returns {object[]} entries that match, or whose children do.
 */
function filterEntries(entries, query) {
  if (query === '') return entries
  const result = []
  for (const entry of entries) {
    if (matchesQuery(entry, query)) {
      result.push(entry)
      continue
    }
    const children = Array.isArray(entry.children) ? filterEntries(entry.children, query) : []
    if (children.length > 0) result.push({ ...entry, children })
  }
  return result
}

/**
 * Filter one object's branches by the query.
 * @param {Array<{ kind: string, title: string, entries: object[] }>} groups - branches from {@link objectGroups}.
 * @param {string} query - lowercase filter text.
 * @returns {Array<{ kind: string, title: string, entries: object[], rows: Array<{ entry: object, depth: number }>, total: number }>} visible branches with flattened rows.
 */
export function filterObjectGroups(groups, query) {
  const source = Array.isArray(groups) ? groups : []
  const result = []
  for (const group of source) {
    const matched = filterEntries(group.entries, query)
    if (query !== '' && matched.length === 0) continue
    const rows = flattenEntries(matched)
    result.push({ ...group, entries: matched, rows, total: rows.length })
  }
  return result
}

/**
 * Total number of metadata objects in a `categories[]` list.
 * @param {object[]} categories - `categories[]` of the `view` document.
 * @returns {number} the sum of the category counts.
 */
export function countObjects(categories) {
  const source = Array.isArray(categories) ? categories : []
  return source.reduce((total, category) => total + (Array.isArray(category.objects) ? category.objects.length : 0), 0)
}
