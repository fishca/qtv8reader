/**
 * Client-half tests (`node --test`).
 *
 * Plain helpers and the fetch wrapper are imported directly. The bundle is
 * loaded the way the browser loads it — through `window.__ModuleLoader__` — with
 * a React stub, and `apply` is run against a recording context, so the test
 * checks the real registration shapes rather than a copy of them.
 */
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import vm from 'node:vm'

import { StateError, buildStateUrl, fetchState } from '../src/client/api.js'
import { flattenEntries, formatBytes, sectionLabel } from '../src/client/labels.js'
import { en, ru } from '../src/client/messages.js'

const here = path.dirname(fileURLToPath(import.meta.url))
const root = path.resolve(here, '..')
const bundle = readFileSync(path.join(root, 'client.js'), 'utf8')

/** Load `client.js` the way the page does and return its module exports. */
function loadBundle() {
  let captured
  const sandbox = {
    window: { __ModuleLoader__: { load(entry) { captured = entry } } },
    console
  }
  vm.runInNewContext(bundle, sandbox, { filename: 'client.js' })
  assert.ok(captured !== undefined, 'client.js did not call __ModuleLoader__.load')

  const react = {
    createElement: () => null,
    Fragment: Symbol('Fragment'),
    useState: () => [undefined, () => {}],
    useEffect: () => {},
    useMemo: (factory) => factory(),
    useCallback: (callback) => callback,
    useRef: (value) => ({ current: value })
  }
  const exports = captured.factory((name) => {
    if (name === 'react') return react
    throw new Error(`unexpected browser module: ${name}`)
  })
  return { entry: captured, exports }
}

test('bundle is a loader factory without ESM syntax', () => {
  assert.match(bundle, /__ModuleLoader__\.load/)
  assert.ok(!/^\s*import\s/m.test(bundle), 'bundle must not keep import statements')
  assert.ok(!/^\s*export\s/m.test(bundle), 'bundle must not keep export statements')

  const { entry, exports } = loadBundle()
  assert.equal(entry.id, '@fishca/dsh-v8reader')
  assert.equal(exports.TYPE_ID, '@fishca/dsh-v8reader')
  assert.equal(exports.TYPE_KIND, 'v8reader')
  assert.equal(typeof exports.apply, 'function')
  assert.deepEqual([...exports.inject], ['slots', 'locale', 'sidebarRightTabs'])
})

test('apply registers the tab type, copy, and body through public seats', () => {
  const { exports } = loadBundle()
  const calls = { locale: [], types: [], injected: [], registered: [], effects: [] }
  const ctx = {
    locale: {
      bind: (namespace) => (key) => key,
      register: (namespace, dicts) => {
        calls.locale.push({ namespace, dicts })
        return () => {}
      }
    },
    sidebarRightTabs: {
      register: (definition) => {
        calls.types.push(definition)
        return () => {}
      }
    },
    slots: {
      inject: (name, callback) => {
        calls.injected.push(name)
        return callback()
      },
      register: (options, component) => {
        calls.registered.push({ options, component })
        return () => {}
      }
    },
    effect: (callback, label) => {
      calls.effects.push(label)
      return callback()
    }
  }

  exports.apply(ctx)

  assert.equal(calls.locale.length, 1)
  assert.equal(calls.locale[0].namespace, 'v8reader')
  assert.equal(calls.locale[0].dicts.ru['type.title'], '1С: конфигурация')
  assert.equal(calls.locale[0].dicts.en['panel.open'], 'Open')
  for (const key of Object.keys(ru)) assert.ok(key in en, `dictionary misses ${key}`)

  assert.equal(calls.types.length, 1)
  const definition = calls.types[0]
  assert.equal(definition.id, exports.TYPE_ID)
  assert.equal(definition.kind, 'v8reader')
  assert.equal(definition.priority, 'extension')
  assert.equal(definition.keepMounted, true)
  assert.equal(definition.guide.length, 1)
  assert.equal(definition.guide[0].id, 'open')
  assert.equal(definition.title(), 'type.title')
  assert.equal(definition.guide[0].title(), 'guide.title')
  assert.equal(definition.guide[0].description(), 'guide.description')

  assert.deepEqual(calls.injected, ['sidebar.right.pane.tab'])
  assert.equal(calls.registered.length, 1)
  assert.equal(calls.registered[0].options.name, 'sidebar.right.pane.tab')
  assert.equal(calls.registered[0].options.key, exports.TYPE_ID)
  assert.equal(calls.registered[0].options.locale, 'v8reader')
  assert.equal(typeof calls.registered[0].component, 'function')
  assert.ok(calls.effects.includes('dsh-v8reader: tab type'))
})

test('state URL encodes Cyrillic, spaces, and plus signs once', () => {
  const file = 'C:\\work\\Авансовый отчет+1.cf'
  const url = buildStateUrl(file, 'Документ.АвансовыйОтчет', 'МодульОбъекта')
  assert.ok(url.startsWith('/v8reader/state?'))
  assert.ok(!url.includes(' '))
  // A literal plus in the path survives as %2B; spaces may become '+'.
  assert.ok(url.includes('%2B'), 'a literal plus must be percent-encoded')

  const parsed = new URL(url, 'http://127.0.0.1:1')
  assert.equal(parsed.searchParams.get('path'), file)
  assert.equal(parsed.searchParams.get('object'), 'Документ.АвансовыйОтчет')
  assert.equal(parsed.searchParams.get('module'), 'МодульОбъекта')

  const plain = new URL(buildStateUrl(file), 'http://127.0.0.1:1')
  assert.equal(plain.searchParams.has('object'), false)
  assert.equal(plain.searchParams.has('module'), false)
})

test('fetchState returns the document and maps failures to StateError', async () => {
  const ok = await fetchState({
    path: 'C:\\a.cf',
    fetchImpl: async (url) => {
      assert.equal(url, buildStateUrl('C:\\a.cf'))
      return new Response(JSON.stringify({ schema: 1, file: { kind: 'configuration' } }), {
        status: 200,
        headers: { 'content-type': 'application/json' }
      })
    }
  })
  assert.equal(ok.schema, 1)

  const refused = await fetchState({
    path: 'C:\\a.txt',
    fetchImpl: async () => new Response(JSON.stringify({
      error: { code: 'unsupported-format', message: 'nope', detail: { extension: '.txt' } }
    }), { status: 415 })
  }).catch((error) => error)
  assert.ok(refused instanceof StateError)
  assert.equal(refused.code, 'unsupported-format')
  assert.equal(refused.status, 415)
  assert.equal(refused.detail.extension, '.txt')

  const crashed = await fetchState({
    path: 'C:\\a.cf',
    fetchImpl: async () => new Response('<html>500</html>', { status: 500 })
  }).catch((error) => error)
  assert.equal(crashed.code, 'cli-failed')
  assert.equal(crashed.status, 500)

  const wrongSchema = await fetchState({
    path: 'C:\\a.cf',
    fetchImpl: async () => new Response(JSON.stringify({ schema: 2 }), { status: 200 })
  }).catch((error) => error)
  assert.ok(wrongSchema instanceof StateError)
  assert.equal(wrongSchema.code, 'cli-failed')
})

test('panel labels and nested section rows', () => {
  assert.equal(formatBytes(0), '0 Б')
  assert.equal(formatBytes(1024), '1.0 КБ')
  assert.equal(formatBytes(1536), '1.5 КБ')
  assert.equal(formatBytes(10 * 1024 * 1024), '10 МБ')
  assert.equal(formatBytes(Number.NaN), '')

  const t = (key) => ru[key] ?? key
  assert.equal(sectionLabel(t, { kind: 'attributes', title: 'Реквизиты' }), 'Реквизиты')
  assert.equal(sectionLabel(t, { kind: 'exotic', title: 'Особый раздел' }), 'Особый раздел')

  const rows = flattenEntries([
    { name: 'А' },
    { name: 'Б', children: [{ name: 'Б1', children: [{ name: 'Б1.1' }] }] }
  ])
  assert.deepEqual(rows.map((row) => [row.entry.name, row.depth]), [
    ['А', 0],
    ['Б', 0],
    ['Б1', 1],
    ['Б1.1', 2]
  ])
})
