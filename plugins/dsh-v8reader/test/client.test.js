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

import {
  StateError,
  buildBrowseUrl,
  buildFormUrl,
  buildStateUrl,
  buildTemplateUrl,
  fetchBrowse,
  fetchState
} from '../src/client/api.js'
import {
  CODE_SETTINGS_KEY,
  DEFAULT_CODE_SETTINGS,
  clampNumber,
  readCodeSettings,
  writeCodeSettings
} from '../src/client/code.js'
import { DOCUMENT_EXTENSIONS, DOCUMENT_ID, documentPath, documentVersion } from '../src/client/document.js'
import { browseErrorText } from '../src/client/errors.js'
import { frameDocumentError, sameFrameUrl } from '../src/client/form.js'
import { flattenEntries, formatBytes, sectionLabel } from '../src/client/labels.js'
import { en, ru } from '../src/client/messages.js'
import {
  DEFAULT_MXL_ZOOM,
  MXL_SETTINGS_KEY,
  MXL_ZOOM_STEPS,
  clampMxlZoom,
  fitWidthZoom,
  readMxlSettings,
  writeMxlSettings
} from '../src/client/mxl.js'
import { dirOf, extensionOf } from '../src/client/paths.js'
import { colorSchemeOf } from '../src/client/scheme.js'
import {
  MODULES_GROUP,
  countObjects,
  filterCategories,
  filterObjectGroups,
  objectGroups,
  objectKey,
  sameObject
} from '../src/client/tree.js'

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
  // The document renderer and the table-document view must be part of the
  // artifact, not only of the sources.
  assert.ok(bundle.includes('documentPreviews'))
  assert.ok(bundle.includes('v8r-mxl'))

  const { entry, exports } = loadBundle()
  assert.equal(entry.id, '@fishca/dsh-v8reader')
  assert.equal(exports.TYPE_ID, '@fishca/dsh-v8reader')
  assert.equal(exports.TYPE_KIND, 'v8reader')
  assert.equal(typeof exports.apply, 'function')
  assert.deepEqual([...exports.inject], ['slots', 'locale', 'sidebarRightTabs', 'theme'])
})

test('apply registers the tab type, copy, and body through public seats', () => {
  const { exports } = loadBundle()
  const calls = {
    locale: [], types: [], injected: [], services: [], registered: [], previews: [], effects: [], events: []
  }
  const slots = {
    inject: (name, callback) => {
      calls.injected.push(name)
      return callback()
    },
    register: (options, component) => {
      calls.registered.push({ options, component })
      return () => {}
    }
  }
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
    slots,
    theme: {
      getTheme: () => ({ active: { colorScheme: 'light' } })
    },
    on: (name, listener) => {
      calls.events.push(name)
      return () => {}
    },
    effect: (callback, label) => {
      calls.effects.push(label)
      return callback()
    },
    inject: (deps, callback) => {
      calls.services.push(deps)
      callback({
        documentPreviews: {
          register: (definition) => {
            calls.previews.push(definition)
            return () => {}
          }
        },
        slots,
        effect: ctx.effect
      })
    }
  }

  exports.apply(ctx)

  assert.equal(calls.locale.length, 1)
  assert.equal(calls.locale[0].namespace, 'v8reader')
  assert.equal(calls.locale[0].dicts.ru['type.title'], '1С: конфигурация')
  assert.equal(calls.locale[0].dicts.en['panel.open'], 'Open')
  for (const key of Object.keys(ru)) assert.ok(key in en, `dictionary misses ${key}`)
  for (const key of Object.keys(en)) assert.ok(key in ru, `dictionary misses ${key}`)

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

  // The page body and the document body share the slot registry but not a key.
  assert.deepEqual(calls.injected, ['sidebar.right.pane.tab', 'sidebar.right.tab.document'])
  assert.equal(calls.registered.length, 2)
  assert.equal(calls.registered[0].options.name, 'sidebar.right.pane.tab')
  assert.equal(calls.registered[0].options.key, exports.TYPE_ID)
  assert.equal(calls.registered[0].options.locale, 'v8reader')
  assert.equal(typeof calls.registered[0].component, 'function')
  assert.equal(calls.registered[1].options.name, 'sidebar.right.tab.document')
  assert.equal(calls.registered[1].options.key, DOCUMENT_ID)
  assert.equal(calls.registered[1].options.locale, 'v8reader')
  assert.equal(typeof calls.registered[1].component, 'function')

  // The document renderer is optional and registers only once its service exists.
  assert.deepEqual(calls.services.map((deps) => [...deps]), [['documentPreviews', 'slots']])
  assert.equal(calls.previews.length, 1)
  const preview = calls.previews[0]
  assert.equal(preview.id, DOCUMENT_ID)
  assert.deepEqual([...preview.extensions], [...DOCUMENT_EXTENSIONS])
  assert.deepEqual([...preview.binaryExtensions], [...DOCUMENT_EXTENSIONS])
  assert.equal(preview.priority, 'extension')
  assert.equal(preview.loading, 'renderer')
  assert.equal(preview.title(), 'document.title')

  assert.deepEqual(calls.events, ['theme/change'])
  assert.ok(calls.effects.includes('dsh-v8reader: tab type'))
  assert.ok(calls.effects.includes('dsh-v8reader: theme scheme'))
  assert.ok(calls.effects.includes('dsh-v8reader: document preview'))
  assert.ok(calls.effects.includes('dsh-v8reader: document body'))
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

test('dirOf keeps drive, UNC, and POSIX roots intact', () => {
  assert.equal(dirOf('E:\\work\\repo\\base.cf'), 'E:\\work\\repo')
  assert.equal(dirOf('E:\\base.cf'), 'E:\\')
  assert.equal(dirOf('E:/work/base.cf'), 'E:/work')
  assert.equal(dirOf('\\\\srv\\share\\base.cf'), '\\\\srv\\share\\')
  assert.equal(dirOf('\\\\srv\\share\\deep\\base.cf'), '\\\\srv\\share\\deep')
  assert.equal(dirOf('/work/base.cf'), '/work')
  assert.equal(dirOf('/base.cf'), '/')
  assert.equal(dirOf('base.cf'), '')
  assert.equal(dirOf('   '), '')
  assert.equal(dirOf('E:\\work\\'), 'E:\\work')
})

test('extensionOf reads the suffix the chooser filters on', () => {
  assert.equal(extensionOf('base.cf'), '.cf')
  assert.equal(extensionOf('BASE.CFE'), '.cfe')
  assert.equal(extensionOf('report.tar.gz'), '.gz')
  assert.equal(extensionOf('noext'), '')
  assert.equal(extensionOf('.hidden'), '')
  assert.equal(extensionOf(''), '')
})

test('browse URL encodes the directory once and omits an empty one', () => {
  assert.equal(buildBrowseUrl(), '/v8reader/browse')
  assert.equal(buildBrowseUrl('', false), '/v8reader/browse')
  assert.equal(buildBrowseUrl('', true), '/v8reader/browse?all=1')

  const url = buildBrowseUrl('C:\\work\\Авансовый отчет', true)
  const parsed = new URL(url, 'http://127.0.0.1:1')
  assert.equal(parsed.pathname, '/v8reader/browse')
  assert.equal(parsed.searchParams.get('path'), 'C:\\work\\Авансовый отчет')
  assert.equal(parsed.searchParams.get('all'), '1')
})

test('fetchBrowse returns the listing and maps failures to StateError', async () => {
  const listing = {
    schema: 1,
    dir: 'C:\\work',
    parent: 'C:\\',
    breadcrumbs: [{ name: 'C:\\', path: 'C:\\' }, { name: 'work', path: 'C:\\work' }],
    entries: [{ name: 'base.cf', path: 'C:\\work\\base.cf', kind: 'file', size: 10 }],
    truncated: false,
    hiddenCount: 2,
    all: false,
    supported: ['.cf']
  }
  const ok = await fetchBrowse({
    dir: 'C:\\work',
    fetchImpl: async (url) => {
      assert.equal(url, buildBrowseUrl('C:\\work'))
      return new Response(JSON.stringify(listing), {
        status: 200,
        headers: { 'content-type': 'application/json' }
      })
    }
  })
  assert.equal(ok.entries.length, 1)
  assert.equal(ok.hiddenCount, 2)

  const denied = await fetchBrowse({
    dir: 'C:\\private',
    fetchImpl: async () => new Response(JSON.stringify({
      error: { code: 'denied', message: 'Нет доступа к папке: C:\\private' }
    }), { status: 403 })
  }).catch((error) => error)
  assert.ok(denied instanceof StateError)
  assert.equal(denied.code, 'denied')

  const wrong = await fetchBrowse({
    dir: 'C:\\work',
    fetchImpl: async () => new Response(JSON.stringify({ schema: 1, dir: 'C:\\work' }), {
      status: 200
    })
  }).catch((error) => error)
  assert.ok(wrong instanceof StateError)
  assert.equal(wrong.code, 'bad-path')
})

test('chooser copy distinguishes a missing folder from a rejected file', () => {
  const t = (key, params = {}) => (ru[key] ?? key).replace(/\{(\w+)\}/g, (_, name) => params[name] ?? '')
  assert.equal(
    browseErrorText(t, new StateError('bad-path', 'x', 404, { reason: 'directory-missing' })),
    ru['picker.error.missing']
  )
  assert.equal(
    browseErrorText(t, new StateError('bad-path', 'x', 400, { reason: 'not-a-directory' })),
    ru['picker.error.notDirectory']
  )
  assert.equal(
    browseErrorText(t, new StateError('bad-path', 'x', 400)),
    ru['error.bad-path']
  )
  assert.equal(browseErrorText(t, new StateError('denied', 'x', 403)), ru['error.denied'])
  assert.equal(browseErrorText(t, new StateError('weird', 'x', 500)), ru['error.unknown'])
})
test('form page URL encodes each value once', () => {
  const url = buildFormUrl('C:\\work\\Контролы.cf', 'e3489bca-d691-53a8-8feb-890730540f20', 'Модуль формы «Кнопки»')
  assert.ok(url.startsWith('/v8reader/form?'))
  assert.ok(!url.includes(' '))
  const parsed = new URL(url, 'http://127.0.0.1:1')
  assert.equal(parsed.pathname, '/v8reader/form')
  assert.equal(parsed.searchParams.get('path'), 'C:\\work\\Контролы.cf')
  assert.equal(parsed.searchParams.get('object'), 'e3489bca-d691-53a8-8feb-890730540f20')
  assert.equal(parsed.searchParams.get('form'), 'Модуль формы «Кнопки»')
})

test('template page URL encodes each value once', () => {
  const url = buildTemplateUrl('C:\\work\\Контролы.cf', 'Контролы', 'Макет отчёта «Итоги»')
  assert.ok(url.startsWith('/v8reader/mxl?'))
  assert.ok(!url.includes(' '))
  const parsed = new URL(url, 'http://127.0.0.1:1')
  assert.equal(parsed.pathname, '/v8reader/mxl')
  assert.equal(parsed.searchParams.get('path'), 'C:\\work\\Контролы.cf')
  assert.equal(parsed.searchParams.get('object'), 'Контролы')
  assert.equal(parsed.searchParams.get('template'), 'Макет отчёта «Итоги»')
})

test('document preview resolves the file path and version from resource metadata', () => {
  assert.equal(DOCUMENT_ID, '@fishca/dsh-v8reader/config')
  assert.deepEqual([...DOCUMENT_EXTENSIONS], ['cf', 'cfe', 'epf', 'erf'])

  assert.equal(documentPath({ value: { absolutePath: 'C:\\work\\a.cf' } }), 'C:\\work\\a.cf')
  assert.equal(documentPath({ value: {} }), '')
  assert.equal(documentPath({ value: { absolutePath: 42 } }), '')
  assert.equal(documentPath(undefined), '')

  assert.equal(documentVersion({ value: { version: 'v1' } }), 'v1')
  assert.equal(documentVersion({ value: {} }), '')
  assert.equal(documentVersion(null), '')
})

test('table-document zoom clamps, fits the frame, and survives bad storage', () => {
  assert.equal(DEFAULT_MXL_ZOOM, 100)
  assert.deepEqual([...MXL_ZOOM_STEPS], [25, 50, 75, 100, 125, 150, 200, 300, 400])

  assert.equal(clampMxlZoom(150), 150)
  assert.equal(clampMxlZoom('80'), 80)
  assert.equal(clampMxlZoom(10), 25)
  assert.equal(clampMxlZoom(900), 400)
  assert.equal(clampMxlZoom('wide'), 100)
  assert.equal(clampMxlZoom(Number.NaN, 75), 75)

  // A 400 px table in a 400 px frame needs less than 100 % once the chrome is
  // removed; a zero width means the frame is not laid out yet.
  assert.equal(fitWidthZoom(0, 800), 100)
  assert.equal(fitWidthZoom(800, 0), 100)
  assert.equal(fitWidthZoom(100, 824), 400)
  assert.ok(fitWidthZoom(800, 400) < 100)
  assert.equal(fitWidthZoom(800, 32), 25)

  assert.deepEqual(readMxlSettings({ getItem: () => null }), { zoom: 100, fit: false })
  assert.deepEqual(readMxlSettings({ getItem: () => '{broken' }), { zoom: 100, fit: false })
  assert.deepEqual(
    readMxlSettings({ getItem: (key) => (key === MXL_SETTINGS_KEY ? JSON.stringify({ zoom: 150, fit: true }) : null) }),
    { zoom: 150, fit: true }
  )
  assert.deepEqual(
    readMxlSettings({ getItem: () => JSON.stringify({ zoom: 5000 }) }),
    { zoom: 400, fit: false }
  )

  let saved
  writeMxlSettings({ zoom: 200, fit: false }, {
    setItem: (key, value) => { saved = { key, value } }
  })
  assert.equal(saved.key, MXL_SETTINGS_KEY)
  assert.deepEqual(JSON.parse(saved.value), { zoom: 200, fit: false })
  // Storage failures are a convenience, not an error.
  writeMxlSettings({ zoom: 200, fit: true }, { setItem: () => { throw new Error('blocked') } })
})

test('code view settings clamp one field at a time and survive bad storage', () => {
  assert.deepEqual(readCodeSettings({ getItem: () => null }), DEFAULT_CODE_SETTINGS)
  assert.deepEqual(readCodeSettings({ getItem: () => '{broken' }), DEFAULT_CODE_SETTINGS)
  assert.deepEqual(readCodeSettings({ getItem: () => '42' }), DEFAULT_CODE_SETTINGS)
  assert.deepEqual(
    readCodeSettings({ getItem: () => JSON.stringify({ enabled: false, wrap: true, size: 99 }) }),
    { enabled: false, wrap: true, size: 20 }
  )
  assert.deepEqual(
    readCodeSettings({ getItem: () => JSON.stringify({ size: 3 }) }),
    { enabled: true, wrap: false, size: 10 }
  )
  assert.deepEqual(
    readCodeSettings({ getItem: () => { throw new Error('blocked') } }),
    DEFAULT_CODE_SETTINGS
  )

  assert.equal(clampNumber('12', 12.5, 10, 20), 12.5)
  assert.equal(clampNumber(Number.NaN, 12.5, 10, 20), 12.5)
  assert.equal(clampNumber(12.5, 12.5, 10, 20), 12.5)

  let saved
  writeCodeSettings({ enabled: true, wrap: true, size: 14 }, {
    setItem: (key, value) => { saved = { key, value } }
  })
  assert.deepEqual(saved, {
    key: CODE_SETTINGS_KEY,
    value: JSON.stringify({ enabled: true, wrap: true, size: 14 })
  })
  assert.doesNotThrow(() => writeCodeSettings(DEFAULT_CODE_SETTINGS, {
    setItem: () => { throw new Error('quota') }
  }))
})

test('metadata tree groups, filters and counts', () => {
  const categories = [
    {
      guid: 'c1',
      title: 'Документы',
      objects: [
        { guid: 'o1', name: 'АвансовыйОтчет', synonym: 'Авансовый отчёт' },
        { guid: 'o2', name: 'Заказ', synonym: '' }
      ]
    },
    { guid: 'c2', title: 'Языки', objects: [{ name: 'Русский', synonym: 'Русский' }] }
  ]
  assert.equal(objectKey({ guid: 'o1' }), 'o1')
  assert.equal(objectKey({ name: 'Русский' }), 'Русский')
  assert.equal(objectKey(null), '')
  assert.equal(countObjects(categories), 3)
  assert.equal(sameObject({ guid: 'o1' }, { guid: 'o1', name: 'АвансовыйОтчет' }), true)
  assert.equal(sameObject({ name: 'Русский' }, { name: 'Русский' }), true)
  assert.equal(sameObject({ name: 'Русский' }, { name: 'Другой' }), false)
  assert.equal(sameObject(null, { name: 'Русский' }), false)

  const filtered = filterCategories(categories, 'отчёт')
  assert.equal(filtered.length, 1)
  assert.deepEqual(filtered[0].items.map((object) => object.name), ['АвансовыйОтчет'])
  assert.equal(filterCategories(categories, 'нет такого').length, 0)
  assert.equal(filterCategories(categories, '')[0].items.length, 2)

  const object = {
    sections: [
      { kind: 'attributes', title: 'Реквизиты', entries: [{ name: 'Сумма', children: [{ name: 'Валюта' }] }] },
      { kind: 'forms', title: 'Формы', entries: [{ name: 'ФормаЭлемента' }] },
      { kind: 'templates', title: 'Макеты', entries: [] }
    ],
    modules: [{ name: 'МодульОбъекта', size: 10 }]
  }
  const groups = objectGroups(object)
  assert.deepEqual(groups.map((group) => group.kind), ['attributes', 'forms', 'templates', MODULES_GROUP])
  assert.equal(groups[1].entries[0].token, 'form')
  assert.equal(groups[2].entries.length, 0)
  assert.equal(groups[3].entries[0].token, 'module')

  const flat = filterObjectGroups(groups, '')
  assert.deepEqual(flat[0].rows.map((row) => [row.entry.name, row.depth]), [['Сумма', 0], ['Валюта', 1]])
  assert.equal(flat[0].total, 2)

  assert.deepEqual(filterObjectGroups(groups, 'формаэлемента').map((group) => group.kind), ['forms'])
  const nested = filterObjectGroups(groups, 'валют')
  assert.deepEqual(nested.map((group) => group.kind), ['attributes'])
  assert.deepEqual(nested[0].rows.map((row) => row.entry.name), ['Сумма', 'Валюта'])
})

test('form frame recognises the JSON error of a failed page', () => {
  assert.equal(frameDocumentError(null), null)
  assert.equal(frameDocumentError({ contentType: 'text/html', body: { textContent: '<html>' } }), null)

  const hostError = frameDocumentError({
    contentType: 'application/json; charset=utf-8',
    body: {
      textContent: JSON.stringify({ error: { code: 'cli-failed', message: 'форма не найдена' } })
    }
  })
  assert.deepEqual(hostError, { code: 'cli-failed', message: 'форма не найдена' })

  const raw = frameDocumentError({
    contentType: 'application/json',
    body: { textContent: 'not json' }
  })
  assert.deepEqual(raw, { code: 'form-error', message: 'not json' })
})

test('colour scheme follows the theme snapshot with a fallback', () => {
  assert.equal(colorSchemeOf({ active: { colorScheme: 'dark' } }), 'dark')
  assert.equal(colorSchemeOf({ active: { colorScheme: 'light' } }, 'dark'), 'light')
  assert.equal(colorSchemeOf({ colorScheme: 'dark' }), 'dark')
  assert.equal(colorSchemeOf(undefined, 'dark'), 'dark')
  assert.equal(colorSchemeOf({}, 'light'), 'light')
})

test('frame URL comparison tolerates query encoding but not another page', () => {
  const source = '/v8reader/form?path=C%3A%2Fwork%2Fa+b.cf&object=o1&form=%D0%A4%D0%BE%D1%80%D0%BC%D0%B0'
  assert.equal(sameFrameUrl(
    'http://127.0.0.1:1/v8reader/form?path=C%3A%2Fwork%2Fa%20b.cf&object=o1&form=%D0%A4%D0%BE%D1%80%D0%BC%D0%B0',
    source,
    'http://127.0.0.1:1/'
  ), true)
  assert.equal(sameFrameUrl(
    'http://127.0.0.1:1/v8reader/form?path=C%3A%2Fwork%2Fa+b.cf&object=o1&form=%D0%A4%D0%BE%D1%80%D0%BC%D0%B0',
    source,
    'http://127.0.0.1:1/'
  ), true)
  assert.equal(sameFrameUrl('http://127.0.0.1:1/v8reader/form?form=%D0%90', source, 'http://127.0.0.1:1/'), false)
  assert.equal(sameFrameUrl('http://127.0.0.1:1/other', source, 'http://127.0.0.1:1/'), false)
  assert.equal(sameFrameUrl('http://localhost:2/v8reader/form?x=1', '/v8reader/form?x=1', 'http://127.0.0.1:1/'), false)
  assert.equal(sameFrameUrl('about:blank', source, 'http://127.0.0.1:1/'), false)
  assert.equal(sameFrameUrl('', source, 'http://127.0.0.1:1/'), false)
})
