/**
 * The left column of the panel: categories of metadata objects, filtered and
 * paged so a 15 000-object configuration stays responsive.
 */
import React, { useMemo, useState } from 'react'

/** Objects rendered per category before the "show more" row. */
const PAGE = 200

/**
 * Filter one category by the query.
 * @param {object} category - one `categories[]` entry of the state document.
 * @param {string} query - lowercase filter text, empty for no filter.
 * @returns {{ category: object, items: object[], total: number }} the visible group.
 */
function filterCategory(category, query) {
  const objects = Array.isArray(category.objects) ? category.objects : []
  if (query === '') return { category, items: objects, total: objects.length }
  const items = objects.filter((object) => {
    const name = String(object.name ?? '').toLowerCase()
    const synonym = String(object.synonym ?? '').toLowerCase()
    return name.includes(query) || synonym.includes(query)
  })
  return { category, items, total: items.length }
}

/**
 * The category and object tree.
 * @param {{
 *   categories: object[],
 *   selected: string,
 *   onSelect: (name: string) => void,
 *   translate: (key: string, params?: object) => string
 * }} props - tree data and the current selection.
 * @returns {React.ReactNode} the tree column.
 */
export function Tree({ categories, selected, onSelect, translate: t }) {
  const [filter, setFilter] = useState('')
  const [expanded, setExpanded] = useState(() => {
    const first = Array.isArray(categories) && categories.length > 0 ? categories[0].guid : undefined
    return new Set(first === undefined ? [] : [first])
  })
  const [limits, setLimits] = useState({})

  const query = filter.trim().toLowerCase()
  const groups = useMemo(
    () => (Array.isArray(categories) ? categories : []).map((category) => filterCategory(category, query)),
    [categories, query]
  )

  const toggle = (guid, title) => {
    const key = guid === undefined || guid === '' ? `title:${title}` : guid
    setExpanded((current) => {
      const next = new Set(current)
      if (next.has(key)) next.delete(key)
      else next.add(key)
      return next
    })
  }

  const more = (guid, title, currentLimit) => {
    const key = guid === undefined || guid === '' ? `title:${title}` : guid
    setLimits((current) => ({ ...current, [key]: currentLimit + PAGE }))
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
        {groups.length === 0 && <p className="v8r-muted">{t('panel.noObjects')}</p>}
        {groups.map(({ category, items, total }) => {
          const key = category.guid === undefined || category.guid === ''
            ? `title:${category.title}`
            : category.guid
          const open = query !== '' ? total > 0 : expanded.has(key)
          const limit = limits[key] ?? PAGE
          return (
            <div key={key}>
              <button
                type="button"
                className="v8r-category"
                aria-expanded={open}
                onClick={() => { toggle(category.guid, category.title) }}
              >
                {category.title}{' '}
                <span className="v8r-category-count">{`(${total})`}</span>
              </button>
              {open && items.slice(0, limit).map((object) => (
                <button
                  key={`${object.guid ?? object.name}:${object.name}`}
                  type="button"
                  className="v8r-object"
                  aria-current={object.name === selected ? 'true' : 'false'}
                  title={object.synonym === '' ? object.name : `${object.name} — ${object.synonym}`}
                  onClick={() => { onSelect(object.name) }}
                >
                  {object.name}
                  {object.synonym !== undefined && object.synonym !== '' && (
                    <span className="v8r-object-synonym">{` — ${object.synonym}`}</span>
                  )}
                </button>
              ))}
              {open && items.length > limit && (
                <button
                  type="button"
                  className="v8r-more"
                  onClick={() => { more(category.guid, category.title, limit) }}
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
