/**
 * The left column: the configuration tree.
 *
 * Hierarchy mirrors the 1C configuration window — configuration → category →
 * object → section → entry. The CLI returns the whole category/object list in
 * one `view` document, so categories and objects need no extra request; the
 * selected object's sections arrive with its detail and are shown under the
 * object node, exactly as the 1C tree lazily fills a branch.
 */
import React, { useEffect, useMemo, useState } from 'react'
import { sectionLabel } from './labels.js'
import {
  MODULES_GROUP,
  countObjects,
  filterCategories,
  filterObjectGroups,
  objectGroups,
  objectKey,
  sameObject
} from './tree.js'

/** Objects rendered per category before the "show more" row. */
const CATEGORY_PAGE = 200

/** Entries rendered per object branch before the "show more" row. */
const GROUP_PAGE = 100

/**
 * Key of one category node.
 * @param {object} category - a `categories[]` entry.
 * @returns {string} the node key.
 */
function categoryKey(category) {
  const guid = typeof category.guid === 'string' && category.guid !== '' ? category.guid : ''
  return `c:${guid !== '' ? guid : String(category.title ?? '')}`
}

/**
 * Key of one object node.
 * @param {object} object - a metadata object.
 * @returns {string} the node key.
 */
function objectNodeKey(object) {
  return `o:${objectKey(object)}`
}

/**
 * Key of one section branch of an object.
 * @param {object} object - the object holding the branch.
 * @param {string} kind - the section kind.
 * @returns {string} the node key.
 */
function groupNodeKey(object, kind) {
  return `g:${objectKey(object)}:${kind}`
}

/**
 * The category, object and section tree.
 * @param {{
 *   file: object|null,
 *   categories: object[],
 *   selected: { name: string, guid?: string } | null,
 *   detail: object|null,
 *   viewMode: 'object'|'module'|'form',
 *   activeSection: string,
 *   moduleName: string,
 *   formName: string,
 *   busy: boolean,
 *   onSelect: (object: object) => void,
 *   onSection: (kind: string) => void,
 *   onOpenForm: (name: string) => void,
 *   onOpenModule: (name: string) => void,
 *   translate: (key: string, params?: object) => string
 * }} props - the tree data, the current selection, and the actions.
 * @returns {React.ReactNode} the tree column.
 */
export function Tree({
  file,
  categories,
  selected,
  detail,
  viewMode,
  activeSection,
  moduleName,
  formName,
  busy,
  onSelect,
  onSection,
  onOpenForm,
  onOpenModule,
  translate: t
}) {
  const [filter, setFilter] = useState('')
  const [expanded, setExpanded] = useState(() => {
    const first = Array.isArray(categories) && categories.length > 0 ? categories[0] : undefined
    return new Set(first === undefined ? [] : [categoryKey(first)])
  })
  const [rootOpen, setRootOpen] = useState(true)
  const [limits, setLimits] = useState({})

  const query = filter.trim().toLowerCase()
  const groups = useMemo(() => filterCategories(categories, query), [categories, query])
  const total = useMemo(() => countObjects(categories), [categories])
  const matched = useMemo(
    () => groups.reduce((sum, group) => sum + group.total, 0),
    [groups]
  )

  const selectedKey = selected === null || selected === undefined ? '' : objectKey(selected)
  const detailKey = detail?.object === undefined || detail?.object === null
    ? ''
    : objectKey(detail.object)
  const detailMatches = selectedKey !== '' && detailKey === selectedKey
  const branches = useMemo(
    () => (detailMatches ? filterObjectGroups(objectGroups(detail.object), query) : []),
    [detailMatches, detail, query]
  )

  // A newly selected object opens its own branch and the category holding it.
  useEffect(() => {
    if (selectedKey === '') return
    const holder = (Array.isArray(categories) ? categories : []).find((category) => (
      (Array.isArray(category.objects) ? category.objects : []).some((object) => sameObject(object, selected))
    ))
    setExpanded((current) => {
      const next = new Set(current)
      if (holder !== undefined) next.add(categoryKey(holder))
      next.add(`o:${selectedKey}`)
      return next
    })
  }, [categories, selected, selectedKey])

  const toggle = (key) => {
    setExpanded((current) => {
      const next = new Set(current)
      if (next.has(key)) next.delete(key)
      else next.add(key)
      return next
    })
  }

  const more = (key, currentLimit, step) => {
    setLimits((current) => ({ ...current, [key]: currentLimit + step }))
  }

  const openObjectNode = (object) => {
    const key = objectNodeKey(object)
    if (sameObject(object, selected)) {
      toggle(key)
      return
    }
    setExpanded((current) => new Set(current).add(key))
    onSelect(object)
  }

  const openGroup = (group) => {
    toggle(groupNodeKey(detail?.object, group.kind))
    if (group.kind !== MODULES_GROUP) onSection(group.kind)
  }

  const leafAction = (group, entry) => {
    if (entry.token === 'form') onOpenForm(entry.name)
    else if (entry.token === 'module') onOpenModule(entry.name)
    else if (group.kind !== MODULES_GROUP) onSection(group.kind)
  }

  const leafActive = (group, entry) => {
    if (entry.token === 'form') return viewMode === 'form' && entry.name === formName
    if (entry.token === 'module') return viewMode === 'module' && entry.name === moduleName
    return false
  }

  const groupTitle = (kind, fallback) => {
    if (kind === MODULES_GROUP) return t('panel.modules')
    return sectionLabel(t, { kind, title: fallback })
  }

  return (
    <div className="v8r-tree">
      <div className="v8r-filter">
        <input
          className="v8r-input"
          type="search"
          value={filter}
          spellCheck={false}
          placeholder={t('panel.filter.placeholder')}
          aria-label={t('panel.filter.placeholder')}
          onChange={(event) => { setFilter(event.target.value) }}
        />
      </div>
      <div className="v8r-tree-list">
        <button
          type="button"
          className="v8r-node v8r-node--root"
          aria-expanded={rootOpen}
          onClick={() => { setRootOpen((current) => !current) }}
        >
          <span className="v8r-caret" aria-hidden="true">{rootOpen ? '▾' : '▸'}</span>
          <span className="v8r-node-label">{t('tree.root')}</span>
          {file?.name !== undefined && file.name !== '' && (
            <span className="v8r-node-synonym">{` «${file.name}»`}</span>
          )}
          <span className="v8r-node-count">
            {query === '' ? `(${total})` : `(${matched})`}
          </span>
        </button>

        {rootOpen && groups.length === 0 && (
          <p className="v8r-muted v8r-tree-empty">{t('panel.noObjects')}</p>
        )}

        {rootOpen && groups.map(({ category, items, total: count }) => {
          const key = categoryKey(category)
          const open = query !== '' ? count > 0 : expanded.has(key)
          const limit = limits[key] ?? CATEGORY_PAGE
          return (
            <div key={key}>
              <button
                type="button"
                className="v8r-node v8r-node--category"
                aria-expanded={open}
                onClick={() => { toggle(key) }}
              >
                <span className="v8r-caret" aria-hidden="true">{open ? '▾' : '▸'}</span>
                <span className="v8r-node-label">{category.title}</span>
                <span className="v8r-node-count">{`(${count})`}</span>
              </button>

              {open && items.slice(0, limit).map((object) => {
                const nodeKey = objectNodeKey(object)
                const isOpen = expanded.has(nodeKey)
                const selectedHere = sameObject(object, selected)
                return (
                  <div key={`${nodeKey}:${object.name}`}>
                    <button
                      type="button"
                      className="v8r-node v8r-node--object"
                      aria-current={selectedHere ? 'true' : 'false'}
                      aria-expanded={isOpen}
                      title={object.synonym === undefined || object.synonym === ''
                        ? object.name
                        : `${object.name} — ${object.synonym}`}
                      onClick={() => { openObjectNode(object) }}
                    >
                      <span className="v8r-caret" aria-hidden="true">{isOpen ? '▾' : '▸'}</span>
                      <span className="v8r-node-label v8r-node-label--mono">{object.name}</span>
                      {object.synonym !== undefined && object.synonym !== '' && (
                        <span className="v8r-node-synonym">{` — ${object.synonym}`}</span>
                      )}
                    </button>

                    {isOpen && selectedHere && busy && !detailMatches && (
                      <div className="v8r-children v8r-muted">{t('tree.loading')}</div>
                    )}

                    {isOpen && selectedHere && detailMatches && branches.map((branch) => {
                      const gkey = groupNodeKey(detail.object, branch.kind)
                      const groupOpen = query !== '' ? branch.total > 0 : expanded.has(gkey)
                      const groupLimit = limits[gkey] ?? GROUP_PAGE
                      return (
                        <div key={gkey} className="v8r-children">
                          <button
                            type="button"
                            className="v8r-node v8r-node--group"
                            aria-expanded={groupOpen}
                            data-active={viewMode === 'object' && activeSection === branch.kind ? 'true' : 'false'}
                            onClick={() => { openGroup(branch) }}
                          >
                            <span className="v8r-caret" aria-hidden="true">{groupOpen ? '▾' : '▸'}</span>
                            <span className="v8r-node-label">{groupTitle(branch.kind, branch.title)}</span>
                            <span className="v8r-node-count">{`(${branch.total})`}</span>
                          </button>

                          {groupOpen && branch.total === 0 && (
                            <div className="v8r-leaf v8r-leaf--empty v8r-muted">{t('panel.sectionEmpty')}</div>
                          )}

                          {groupOpen && branch.rows.slice(0, groupLimit).map(({ entry, depth }, index) => (
                            <button
                              key={`${entry.name}:${index}`}
                              type="button"
                              className="v8r-leaf"
                              data-token={entry.token}
                              data-active={leafActive(branch, entry) ? 'true' : 'false'}
                              style={{ paddingLeft: `${18 + depth * 12}px` }}
                              title={entry.synonym === undefined || entry.synonym === ''
                                ? entry.name
                                : `${entry.name} — ${entry.synonym}`}
                              onClick={() => { leafAction(branch, entry) }}
                            >
                              <span className="v8r-leaf-name">{entry.name}</span>
                              {entry.typeText !== undefined && entry.typeText !== '' && (
                                <span className="v8r-leaf-type">{entry.typeText}</span>
                              )}
                            </button>
                          ))}

                          {groupOpen && branch.total > groupLimit && (
                            <button
                              type="button"
                              className="v8r-more"
                              onClick={() => { more(gkey, groupLimit, GROUP_PAGE) }}
                            >
                              {t('panel.showMore')} ({branch.total - groupLimit})
                            </button>
                          )}
                        </div>
                      )
                    })}
                  </div>
                )
              })}

              {open && items.length > limit && (
                <button
                  type="button"
                  className="v8r-more"
                  onClick={() => { more(key, limit, CATEGORY_PAGE) }}
                >
                  {t('panel.showMore')} ({items.length - limit})
                </button>
              )}
            </div>
          )
        })}
      </div>
    </div>
  )
}
