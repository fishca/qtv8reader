/**
 * Host-half unit and integration tests (`node --test`).
 *
 * Unit tests drive the real modules with injected seams: a fake `stat`, a fake
 * `spawn`, and a fake CLI for the route tests. The last test talks to the real
 * `v8reader_cli` on the control fixture and skips when neither the binary nor
 * the fixture exists.
 */
import { test } from 'node:test'
import assert from 'node:assert/strict'
import path from 'node:path'
import { EventEmitter } from 'node:events'
import { fileURLToPath } from 'node:url'
import {
  existsSync,
  mkdirSync,
  mkdtempSync,
  readdirSync,
  rmSync,
  statSync,
  writeFileSync
} from 'node:fs'
import { tmpdir } from 'node:os'

import { ERROR_CODES, RouteError } from '../src/host/errors.js'
import {
  MAX_BROWSE_ENTRIES,
  breadcrumbsOf,
  listDirectory,
  resolveDirectory
} from '../src/host/browse.js'
import {
  assetMimeType,
  buildFormPage,
  inlineJson,
  resolveAssetPath
} from '../src/host/assets.js'
import {
  formCacheKey,
  inspectPathValue,
  inspectTarget,
  isAbsolutePath,
  mxlCacheKey,
  stateCacheKey,
  fileVersion
} from '../src/host/paths.js'
import { createLruCache } from '../src/host/cache.js'
import {
  MIN_CLI_VERSION,
  cliCandidates,
  compareVersions,
  createCli,
  parseCliVersion
} from '../src/host/cli.js'
import { DEFAULT_SETTINGS, createPlugin, resolveSettings } from '../src/host/plugin.js'
import { createResource } from '../src/host/routes.js'

const here = path.dirname(fileURLToPath(import.meta.url))
const packageDir = path.resolve(here, '..')
const isWindows = process.platform === 'win32'
const FILE = isWindows ? 'C:\\work\\campaign.cf' : '/work/campaign.cf'
const OTHER_FILE = isWindows ? 'C:\\work\\other.cf' : '/work/other.cf'

/** @returns {{ isFile: () => boolean, size: number, mtimeMs: number }} */
function fileStat(size = 1024, mtimeMs = 1700000000000) {
  return { isFile: () => true, size, mtimeMs }
}

/** @returns {{ isFile: () => boolean, size: number, mtimeMs: number }} */
function directoryStat() {
  return { isFile: () => false, size: 0, mtimeMs: 0 }
}

function missingError() {
  const error = new Error('ENOENT')
  error.code = 'ENOENT'
  return error
}

/** A stat seam serving the fake files above. */
function fakeStat(overrides = {}) {
  const files = {
    [FILE]: fileStat(),
    [OTHER_FILE]: fileStat(2048),
    [`${OTHER_FILE}.txt`]: fileStat(),
    ...overrides
  }
  return (candidate) => {
    const entry = files[path.normalize(candidate)]
    if (entry === undefined) throw missingError()
    return entry
  }
}

/** A child process stand-in that emits output and exits on the next tick. */
function fakeChild({ stdout = '', stderr = '', code = 0 } = {}) {
  const child = new EventEmitter()
  child.pid = 4321
  child.stdout = new EventEmitter()
  child.stderr = new EventEmitter()
  child.kill = () => {}
  setImmediate(() => {
    if (stdout !== '') child.stdout.emit('data', Buffer.from(stdout, 'utf8'))
    if (stderr !== '') child.stderr.emit('data', Buffer.from(stderr, 'utf8'))
    child.emit('close', code)
  })
  return child
}

/** A spawn seam answering by executable. */
function fakeSpawn(table) {
  return (executable) => {
    const entry = table[executable]
    if (entry === undefined) {
      const child = new EventEmitter()
      child.pid = undefined
      child.stdout = new EventEmitter()
      child.stderr = new EventEmitter()
      child.kill = () => {}
      setImmediate(() => child.emit('error', missingError()))
      return child
    }
    return fakeChild(entry)
  }
}

/** The smallest `view --json` document the route accepts. */
function viewDocument(overrides = {}) {
  return {
    schema: 1,
    appVersion: '0.14.0',
    file: { path: FILE, kind: 'configuration', name: 'Тест', objectCount: 1, size: 1024 },
    categories: [{ title: 'Справочники', typeTitle: 'Справочник', count: 1, objects: [] }],
    object: null,
    content: null,
    ...overrides
  }
}

/** A CLI stand-in for route tests that records its runs. */
function fakeCli({ stdout = JSON.stringify(viewDocument()), code = 0, resolveError } = {}) {
  const calls = []
  return {
    calls,
    async resolve() {
      if (resolveError !== undefined) throw resolveError
      return { path: 'v8reader_cli.exe', version: MIN_CLI_VERSION, searched: ['v8reader_cli.exe'] }
    },
    async run(args, options) {
      calls.push({ args, options })
      return {
        code,
        stdout: typeof stdout === 'function' ? stdout(args) : stdout,
        stderr: code === 0 ? '' : 'Ошибка: объект «Нет» не найден',
        timedOut: false,
        overflow: false,
        durationMs: 1
      }
    },
    searchedCandidates: () => ['v8reader_cli.exe']
  }
}

function request(url, init = {}) {
  return new Request(url, init)
}

function stateUrl(params = {}) {
  const search = new URLSearchParams({ path: FILE, ...params })
  return `http://127.0.0.1:1/v8reader/state?${search.toString()}`
}

test('settings merge defaults and clamp numeric bounds', () => {
  const defaults = resolveSettings()
  assert.equal(defaults.cliPath, '')
  assert.equal(defaults.timeoutMs, 20000)
  assert.equal(defaults.maxJsonBytes, DEFAULT_SETTINGS.maxJsonBytes)
  assert.equal(defaults.minCliVersion, MIN_CLI_VERSION)

  assert.deepEqual(
    resolveSettings({
      cliPath: '  C:\\tools\\v8reader_cli.exe ',
      timeoutMs: 0,
      maxJsonBytes: 1,
      maxConcurrent: 99,
      cacheEntries: 0,
      cacheBytes: 1
    }),
    {
      cliPath: 'C:\\tools\\v8reader_cli.exe',
      timeoutMs: 1000,
      maxJsonBytes: 1024,
      maxConcurrent: 8,
      cacheEntries: 1,
      cacheBytes: 1024 * 1024,
      minCliVersion: MIN_CLI_VERSION
    }
  )

  assert.equal(resolveSettings({ timeoutMs: '30000' }).timeoutMs, 20000)
})

test('path shape: absolute only, no NUL, bounded length', () => {
  assert.equal(isAbsolutePath('C:\\work\\a.cf', 'win32'), true)
  assert.equal(isAbsolutePath('\\\\server\\share\\a.cf', 'win32'), true)
  assert.equal(isAbsolutePath('\\work\\a.cf', 'win32'), false)
  assert.equal(isAbsolutePath('a.cf', 'win32'), false)
  assert.equal(isAbsolutePath('/work/a.cf', 'linux'), true)
  assert.equal(isAbsolutePath('work/a.cf', 'linux'), false)

  assert.equal(inspectPathValue('C:\\work\\a.cf', { platform: 'win32' }), 'C:\\work\\a.cf')
  for (const bad of ['', 'relative.cf', 'C:\\work\\\u0000.cf', 'x'.repeat(5000)]) {
    assert.throws(
      () => inspectPathValue(bad, { platform: 'win32' }),
      (error) => error instanceof RouteError && error.code === ERROR_CODES.badPath
    )
  }
  assert.throws(
    () => inspectPathValue(undefined, { platform: 'win32' }),
    (error) => error instanceof RouteError && error.status === 400
  )
})

test('target: regular supported file only', () => {
  const facts = inspectTarget(FILE, { stat: fakeStat(), platform: process.platform })
  assert.equal(facts.path, path.normalize(FILE))
  assert.equal(facts.size, 1024)
  assert.equal(facts.extension, '.cf')
  assert.equal(facts.mtimeMs, 1700000000000)

  assert.throws(
    () => inspectTarget(OTHER_FILE + '.x', { stat: fakeStat(), platform: process.platform }),
    (error) => error instanceof RouteError && error.status === 404
  )
  assert.throws(
    () => inspectTarget(OTHER_FILE, {
      stat: () => directoryStat(),
      platform: process.platform
    }),
    (error) => error instanceof RouteError && error.code === ERROR_CODES.badPath
  )
  assert.throws(
    () => inspectTarget(OTHER_FILE + '.txt', { stat: fakeStat(), platform: process.platform }),
    (error) => error instanceof RouteError && error.code === ERROR_CODES.unsupportedFormat
  )
  assert.throws(
    () => inspectTarget(OTHER_FILE, {
      stat: () => { throw Object.assign(new Error('EACCES'), { code: 'EACCES' }) },
      platform: process.platform
    }),
    (error) => error instanceof RouteError
      && error.code === ERROR_CODES.denied
      && error.status === 403
  )
})

test('cache key changes with the file generation and the selection', () => {
  const file = { path: FILE, size: 10, mtimeMs: 20 }
  assert.equal(stateCacheKey(file, '', ''), [FILE, 10, 20, '', ''].join('\u0000'))
  assert.notEqual(stateCacheKey(file, '', ''), stateCacheKey({ ...file, mtimeMs: 21 }, '', ''))
  assert.notEqual(stateCacheKey(file, '', ''), stateCacheKey(file, 'Документ.АвансовыйОтчет', ''))
  assert.notEqual(stateCacheKey(file, '', ''), stateCacheKey(file, '', 'МодульОбъекта'))
  assert.equal(fileVersion(file), `${(20).toString(36)}-${(10).toString(36)}`)
})

test('cache is an LRU with an entry and a byte budget', () => {
  const cache = createLruCache({ maxEntries: 2, maxBytes: 64 })
  assert.equal(cache.set('a', 'aaaa'), true)
  assert.equal(cache.set('b', 'bbbb'), true)
  assert.equal(cache.get('a'), 'aaaa')
  cache.set('c', 'cccc') // evicts b, the least recently used
  assert.equal(cache.get('b'), undefined)
  assert.equal(cache.get('a'), 'aaaa')
  assert.equal(cache.get('c'), 'cccc')
  assert.equal(cache.size, 2)

  assert.equal(cache.set('huge', 'x'.repeat(65)), false)
  assert.equal(cache.get('huge'), undefined)

  const bytes = createLruCache({ maxEntries: 10, maxBytes: 10 })
  bytes.set('a', 'x'.repeat(6))
  bytes.set('b', 'y'.repeat(6))
  assert.equal(bytes.size, 1)
  assert.equal(bytes.get('a'), undefined)
  assert.equal(bytes.bytes, 6)
  bytes.clear()
  assert.equal(bytes.size, 0)
  assert.equal(bytes.bytes, 0)
})

test('version parsing and ordering', () => {
  assert.equal(parseCliVersion('v8reader_cli 0.14.0'), '0.14.0')
  assert.equal(parseCliVersion('v8reader_cli 1.2.3-rc.1'), '1.2.3')
  assert.equal(parseCliVersion('1.2.3\n'), '1.2.3')
  assert.equal(parseCliVersion('another program 1.0.0'), '')
  assert.equal(parseCliVersion('nonsense'), '')
  assert.equal(compareVersions('0.14.0', '0.14.0'), 0)
  assert.ok(compareVersions('0.13.3', '0.14.0') < 0)
  assert.ok(compareVersions('0.14.1', '0.14.0') > 0)
})

test('executable search order: config, environment, build outputs, PATH', () => {
  const candidates = cliCandidates({
    cliPath: 'C:\\custom\\v8reader_cli.exe',
    env: {
      V8READER_CLI: 'C:\\env\\v8reader_cli.exe',
      PATH: 'C:\\a;C:\\b'
    },
    platform: 'win32',
    packageDir: 'C:\\repo\\plugins\\dsh-v8reader'
  })
  assert.deepEqual(candidates, [
    'C:\\custom\\v8reader_cli.exe',
    'C:\\env\\v8reader_cli.exe',
    path.resolve('C:\\repo\\plugins\\dsh-v8reader', '..', '..', 'out', 'cli-deploy', 'v8reader_cli.exe'),
    path.resolve('C:\\repo\\plugins\\dsh-v8reader', '..', '..', 'out', 'mingw', 'v8reader_cli.exe'),
    path.resolve('C:\\repo\\plugins\\dsh-v8reader', '..', '..', 'out', 'vs', 'v8reader_cli.exe'),
    path.resolve('C:\\repo\\plugins\\dsh-v8reader', '..', '..', 'out', 'build', 'v8reader_cli.exe'),
    path.join('C:\\a', 'v8reader_cli.exe'),
    path.join('C:\\b', 'v8reader_cli.exe')
  ])
})

test('resolver skips a file that is not our CLI and refuses an old one', async () => {
  const stat = (candidate) => {
    if (candidate === 'C:\\one\\v8reader_cli.exe' || candidate === 'C:\\two\\v8reader_cli.exe') {
      return fileStat()
    }
    throw missingError()
  }
  const spawn = fakeSpawn({
    'C:\\one\\v8reader_cli.exe': { stdout: 'another program 1.0.0', code: 0 },
    'C:\\two\\v8reader_cli.exe': { stdout: 'v8reader_cli 0.14.0', code: 0 }
  })
  const cli = createCli({
    settings: { cliPath: 'C:\\one\\v8reader_cli.exe', maxConcurrent: 1 },
    env: { V8READER_CLI: 'C:\\two\\v8reader_cli.exe' },
    platform: 'win32',
    packageDir: '',
    spawn,
    stat
  })
  const resolved = await cli.resolve()
  assert.equal(resolved.path, 'C:\\two\\v8reader_cli.exe')
  assert.equal(resolved.version, '0.14.0')
  assert.deepEqual(resolved.searched, ['C:\\one\\v8reader_cli.exe', 'C:\\two\\v8reader_cli.exe'])

  const oldSpawn = fakeSpawn({ 'C:\\one\\v8reader_cli.exe': { stdout: 'v8reader_cli 0.13.3' } })
  const oldCli = createCli({
    settings: { cliPath: 'C:\\one\\v8reader_cli.exe' },
    env: {},
    platform: 'win32',
    spawn: oldSpawn,
    stat: () => fileStat()
  })
  await assert.rejects(
    () => oldCli.resolve(),
    (error) => error instanceof RouteError && error.code === ERROR_CODES.cliOld
  )

  const missing = createCli({
    settings: {},
    env: { PATH: 'C:\\empty' },
    platform: 'win32',
    spawn: fakeSpawn({}),
    stat: () => { throw missingError() }
  })
  await assert.rejects(
    () => missing.resolve(),
    (error) => error instanceof RouteError
      && error.code === ERROR_CODES.cliMissing
      && Array.isArray(error.detail.searched)
  )
})

test('resolver retries after a failure instead of caching it', async () => {
  const executable = 'C:\\bin\\v8reader_cli.exe'
  let deployed = false
  const cli = createCli({
    settings: { cliPath: executable },
    env: {},
    platform: 'win32',
    spawn: fakeSpawn({ [executable]: { stdout: 'v8reader_cli 0.14.0' } }),
    stat: () => {
      if (!deployed) throw missingError()
      return fileStat()
    }
  })
  await assert.rejects(
    () => cli.resolve(),
    (error) => error instanceof RouteError && error.code === ERROR_CODES.cliMissing
  )
  deployed = true
  const resolved = await cli.resolve()
  assert.equal(resolved.path, executable)
  assert.equal(resolved.version, '0.14.0')
})

test('state route returns the CLI document and caches by file generation', async () => {
  const cli = fakeCli()
  const resource = createResource({ cli, cache: createLruCache(), settings: DEFAULT_SETTINGS, stat: fakeStat() })
  const response = await resource(request(stateUrl()))
  assert.equal(response.status, 200)
  assert.match(response.headers.get('content-type'), /application\/json/)
  assert.equal(response.headers.get('x-v8reader-version'), fileVersion({ size: 1024, mtimeMs: 1700000000000 }))
  assert.deepEqual(await response.json(), viewDocument())
  assert.equal(cli.calls.length, 1)
  assert.deepEqual(cli.calls[0].args, ['view', path.normalize(FILE), '--json'])
  assert.ok(cli.calls[0].options.signal !== undefined, 'the CLI run must follow the request signal')
  assert.equal(typeof cli.calls[0].options.signal.aborted, 'boolean')

  const again = await resource(request(stateUrl()))
  assert.equal(again.status, 200)
  assert.equal(cli.calls.length, 1) // served from the cache

  const withObject = await resource(request(stateUrl({ object: 'Документ.АвансовыйОтчет' })))
  assert.equal(withObject.status, 200)
  assert.equal(cli.calls.length, 2)
  assert.deepEqual(cli.calls[1].args, [
    'view', path.normalize(FILE), 'Документ.АвансовыйОтчет', '--json'
  ])

  await resource(request(stateUrl({ object: 'Документ.АвансовыйОтчет', module: 'МодульОбъекта' })))
  assert.deepEqual(cli.calls[2].args, [
    'view', path.normalize(FILE), 'Документ.АвансовыйОтчет', '--json', '--module', 'МодульОбъекта'
  ])
})

test('state route answers 304 for a matching ETag and HEAD without a body', async () => {
  const cli = fakeCli()
  const resource = createResource({ cli, cache: createLruCache(), settings: DEFAULT_SETTINGS, stat: fakeStat() })
  const first = await resource(request(stateUrl()))
  const etag = first.headers.get('etag')
  assert.ok(etag !== null)

  const cached = await resource(request(stateUrl(), { headers: { 'if-none-match': etag } }))
  assert.equal(cached.status, 304)
  assert.equal(await cached.text(), '')

  // The ETag names the answer, not just the file: revalidating one object's
  // answer with another selection's tag must return a body.
  const withObject = await resource(request(stateUrl({ object: 'Документ.АвансовыйОтчет' })))
  assert.notEqual(withObject.headers.get('etag'), etag)
  const mismatched = await resource(request(
    stateUrl({ object: 'Документ.АвансовыйОтчет' }),
    { headers: { 'if-none-match': etag } }
  ))
  assert.equal(mismatched.status, 200)
  assert.equal((await mismatched.json()).schema, 1)

  const head = await resource(request(stateUrl(), { method: 'HEAD' }))
  assert.equal(head.status, 200)
  assert.equal(await head.text(), '')
})

test('state route maps failures to stable codes', async () => {
  const stat = fakeStat()
  const mk = (cli) => createResource({ cli, cache: createLruCache(), settings: DEFAULT_SETTINGS, stat })

  const badPath = await mk(fakeCli())(request('http://127.0.0.1:1/v8reader/state?path=relative.cf'))
  assert.equal(badPath.status, 400)
  assert.equal((await badPath.json()).error.code, ERROR_CODES.badPath)

  const unsupported = await mk(fakeCli())(request(
    `http://127.0.0.1:1/v8reader/state?path=${encodeURIComponent(OTHER_FILE + '.txt')}`
  ))
  assert.equal(unsupported.status, 415)
  assert.equal((await unsupported.json()).error.code, ERROR_CODES.unsupportedFormat)

  const missingFile = await mk(fakeCli())(request(
    `http://127.0.0.1:1/v8reader/state?path=${encodeURIComponent(FILE + '.missing.cf')}`
  ))
  assert.equal(missingFile.status, 404)
  assert.equal((await missingFile.json()).error.code, ERROR_CODES.badPath)

  const failed = await mk(fakeCli({ code: 1 }))(request(stateUrl()))
  assert.equal(failed.status, 500)
  const failedBody = await failed.json()
  assert.equal(failedBody.error.code, ERROR_CODES.cliFailed)
  assert.match(failedBody.error.message, /не найден/)

  const badSchema = await mk(fakeCli({ stdout: JSON.stringify({ schema: 2 }) }))(request(stateUrl()))
  assert.equal(badSchema.status, 502)
  assert.equal((await badSchema.json()).error.code, ERROR_CODES.cliFailed)

  const notJson = await mk(fakeCli({ stdout: 'not json' }))(request(stateUrl()))
  assert.equal(notJson.status, 502)

  const post = await mk(fakeCli())(request(stateUrl(), { method: 'POST' }))
  assert.equal(post.status, 405)
  assert.equal(post.headers.get('allow'), 'GET, HEAD')

  const crossSite = await mk(fakeCli())(request(stateUrl(), {
    headers: { 'sec-fetch-site': 'cross-site' }
  }))
  assert.equal(crossSite.status, 403)
  assert.equal((await crossSite.json()).error.code, ERROR_CODES.forbidden)

  const unknown = await mk(fakeCli())(request('http://127.0.0.1:1/v8reader/nope'))
  assert.equal(unknown.status, 404)
  assert.equal((await unknown.json()).error.code, ERROR_CODES.notFound)
})

test('health reports a missing CLI without failing the request', async () => {
  const missing = new RouteError(ERROR_CODES.cliMissing, 'v8reader_cli не найден', 503, {
    searched: ['C:\\a\\v8reader_cli.exe'],
    min: MIN_CLI_VERSION
  })
  const resource = createResource({
    cli: fakeCli({ resolveError: missing }),
    cache: createLruCache(),
    settings: DEFAULT_SETTINGS,
    stat: fakeStat()
  })
  const response = await resource(request('http://127.0.0.1:1/v8reader/health'))
  assert.equal(response.status, 200)
  const body = await response.json()
  assert.equal(body.ok, false)
  assert.equal(body.schema, 1)
  assert.equal(body.cli.found, false)
  assert.equal(body.cli.code, ERROR_CODES.cliMissing)
  assert.deepEqual(body.cli.searched, ['C:\\a\\v8reader_cli.exe'])

  const healthy = createResource({
    cli: fakeCli(),
    cache: createLruCache(),
    settings: DEFAULT_SETTINGS,
    stat: fakeStat()
  })
  const ok = await (await healthy(request('http://127.0.0.1:1/v8reader/health'))).json()
  assert.equal(ok.ok, true)
  assert.equal(ok.cli.version, MIN_CLI_VERSION)
})

test('browse lists one directory level: folders first, supported files only unless all', () => {
  const dir = mkdtempSync(path.join(tmpdir(), 'v8r-browse-'))
  try {
    mkdirSync(path.join(dir, 'forms'))
    mkdirSync(path.join(dir, 'Alpha'))
    writeFileSync(path.join(dir, 'a.cf'), 'x'.repeat(64))
    writeFileSync(path.join(dir, 'b.txt'), 'y')
    writeFileSync(path.join(dir, '.hidden.cf'), 'z')
    writeFileSync(path.join(dir, 'c.EPF'), 'w')

    const listing = listDirectory(dir, { platform: process.platform })
    assert.equal(listing.schema, 1)
    assert.deepEqual(listing.entries.map((entry) => entry.name), ['Alpha', 'forms', 'a.cf', 'c.EPF'])
    assert.deepEqual(listing.entries.map((entry) => entry.kind), ['dir', 'dir', 'file', 'file'])
    assert.equal(listing.hiddenCount, 1)
    assert.equal(listing.all, false)
    assert.equal(listing.truncated, false)
    assert.equal(listing.entries[2].size, 64)
    assert.equal(listing.entries[2].path, path.join(dir, 'a.cf'))

    const all = listDirectory(dir, { all: true, platform: process.platform })
    assert.deepEqual(
      all.entries.map((entry) => entry.name),
      ['Alpha', 'forms', 'a.cf', 'b.txt', 'c.EPF']
    )
    assert.equal(all.hiddenCount, 0)
    assert.equal(all.all, true)
    assert.ok(all.supported.includes('.cf'))
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
})

test('browse resolves a file path to its directory and walks to the root', () => {
  const dir = mkdtempSync(path.join(tmpdir(), 'v8r-browse-'))
  try {
    const file = path.join(dir, 'base.cf')
    writeFileSync(file, 'x')
    assert.equal(resolveDirectory(file, { platform: process.platform }), dir)
    assert.equal(resolveDirectory(dir, { platform: process.platform }), dir)

    const crumbs = breadcrumbsOf(dir, process.platform)
    assert.ok(crumbs.length >= 2)
    assert.equal(crumbs[crumbs.length - 1].path, path.resolve(dir))
    assert.equal(crumbs[0].path, path.parse(path.resolve(dir)).root)
    assert.deepEqual(
      listDirectory(file, { platform: process.platform }).dir,
      dir
    )
    assert.equal(listDirectory(file, { platform: process.platform }).parent, path.dirname(dir))
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
})

test('browse stops at the entry limit and keeps the filter usable', () => {
  const many = MAX_BROWSE_ENTRIES + 5
  const listing = listDirectory('C:\\work', {
    platform: 'win32',
    stat: () => ({ isFile: () => false, isDirectory: () => true, size: 0 }),
    readdir: () => Array.from({ length: many }, (_, index) => ({
      name: `file-${index}.cf`,
      isDirectory: () => false,
      isFile: () => true
    }))
  })
  assert.equal(listing.truncated, true)
  assert.equal(listing.entries.length, MAX_BROWSE_ENTRIES)
  assert.equal(listing.entries[0].kind, 'file')
})

test('browse route answers a listing, HEAD without a body, and stable failures', async () => {
  const dir = mkdtempSync(path.join(tmpdir(), 'v8r-browse-'))
  try {
    writeFileSync(path.join(dir, 'base.cf'), 'x')
    mkdirSync(path.join(dir, 'sub'))
    const resource = createResource({
      cli: fakeCli(),
      cache: createLruCache(),
      settings: DEFAULT_SETTINGS,
      stat: statSync,
      readdir: readdirSync,
      home: dir,
      platform: process.platform
    })

    const home = await resource(request('http://127.0.0.1:1/v8reader/browse'))
    assert.equal(home.status, 200)
    const body = await home.json()
    assert.equal(body.schema, 1)
    assert.equal(body.dir, dir)
    assert.deepEqual(body.entries.map((entry) => entry.name), ['sub', 'base.cf'])

    const head = await resource(request('http://127.0.0.1:1/v8reader/browse', { method: 'HEAD' }))
    assert.equal(head.status, 200)
    assert.equal(await head.text(), '')

    const seeded = await resource(request(
      `http://127.0.0.1:1/v8reader/browse?path=${encodeURIComponent(path.join(dir, 'base.cf'))}`
    ))
    assert.equal((await seeded.json()).dir, dir)

    const all = await resource(request(
      `http://127.0.0.1:1/v8reader/browse?path=${encodeURIComponent(dir)}&all=1`
    ))
    assert.equal((await all.json()).all, true)

    const missing = await resource(request(
      `http://127.0.0.1:1/v8reader/browse?path=${encodeURIComponent(path.join(dir, 'nope'))}`
    ))
    assert.equal(missing.status, 404)
    const missingBody = await missing.json()
    assert.equal(missingBody.error.code, ERROR_CODES.badPath)
    assert.equal(missingBody.error.detail.reason, 'directory-missing')

    const relative = await resource(request('http://127.0.0.1:1/v8reader/browse?path=work'))
    assert.equal(relative.status, 400)

    const crossSite = await resource(request('http://127.0.0.1:1/v8reader/browse', {
      headers: { 'sec-fetch-site': 'cross-site' }
    }))
    assert.equal(crossSite.status, 403)
    assert.equal((await crossSite.json()).error.code, ERROR_CODES.forbidden)
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
})

test('browse reports a denied directory with its own code', () => {
  assert.throws(
    () => listDirectory('C:\\private', {
      platform: 'win32',
      readdir: () => { throw Object.assign(new Error('EACCES'), { code: 'EACCES' }) },
      stat: () => ({ isFile: () => false, isDirectory: () => true, size: 0 })
    }),
    (error) => error instanceof RouteError
      && error.code === ERROR_CODES.denied
      && error.status === 403
  )
})

test('plugin wires one prefix route and disposes the runner', async () => {
  const plugin = createPlugin({ config: {}, packageDir, env: {}, platform: process.platform })
  assert.equal(plugin.route.kind, 'prefix')
  assert.equal(plugin.route.path, '/v8reader')
  assert.equal(typeof plugin.route.resource, 'function')
  plugin.dispose()
  assert.equal(plugin.cache.size, 0)
})

test('form cache key separates the generation, the object and the form', () => {
  const file = { path: FILE, size: 10, mtimeMs: 20 }
  assert.notEqual(formCacheKey(file, 'A', 'Форма1'), formCacheKey({ ...file, mtimeMs: 21 }, 'A', 'Форма1'))
  assert.notEqual(formCacheKey(file, 'A', 'Форма1'), formCacheKey(file, 'B', 'Форма1'))
  assert.notEqual(formCacheKey(file, 'A', 'Форма1'), formCacheKey(file, 'A', 'Форма2'))
  assert.equal(formCacheKey(file, 'A', 'Форма1'), formCacheKey({ ...file }, 'A', 'Форма1'))
})

test('mxl cache key separates the generation, the object and the template', () => {
  const file = { path: FILE, size: 10, mtimeMs: 20 }
  assert.notEqual(mxlCacheKey(file, 'A', 'Макет1'), mxlCacheKey({ ...file, size: 11 }, 'A', 'Макет1'))
  assert.notEqual(mxlCacheKey(file, 'A', 'Макет1'), mxlCacheKey(file, 'B', 'Макет1'))
  assert.notEqual(mxlCacheKey(file, 'A', 'Макет1'), mxlCacheKey(file, 'A', 'Макет2'))
  assert.equal(mxlCacheKey(file, 'A', 'Макет1'), mxlCacheKey({ ...file }, 'A', 'Макет1'))
  // The form and table-document keys never collide, even for one selection.
  assert.notEqual(mxlCacheKey(file, 'A', 'Форма1'), formCacheKey(file, 'A', 'Форма1'))
})

test('model page injects the base and the model before the renderer script', () => {
  const template = '<!doctype html><html><head><meta charset="utf-8">'
    + '<link rel="stylesheet" href="css/viewer.css"></head><body>'
    + '<div id="host"></div><script src="js/model-app.js"></script></body></html>'
  const model = { tag: 'Form', name: 'Кнопки', caption: '<script>alert(1)</script>' }
  const page = buildFormPage(template, model, { baseHref: '/v8reader/assets/' })

  assert.match(page, /<base href="\/v8reader\/assets\/">/)
  // The template's stylesheet links are relative; a <base> placed after them
  // does not apply retroactively and the form would load without CSS.
  const baseAt = page.indexOf('<base')
  const stylesheetAt = page.indexOf('rel="stylesheet"')
  assert.ok(baseAt > 0, 'the base element is missing')
  assert.ok(baseAt < stylesheetAt, 'the base must precede the stylesheet links')

  const modelAt = page.indexOf('window.__QV8_MODEL__')
  const scriptAt = page.indexOf('<script src="js/model-app.js">')
  assert.ok(modelAt > 0, 'the model script is missing')
  assert.ok(modelAt < scriptAt, 'the model must be defined before model-app.js runs')
  assert.ok(!inlineJson(model).includes('</script>'), 'inline JSON must not close the script element')
  assert.match(page, /\\u003cscript>alert/)

  assert.throws(
    () => buildFormPage('<html><body></body></html>', model, { baseHref: '/x/' }),
    (error) => error instanceof RouteError && error.code === ERROR_CODES.assetsMissing
  )
  assert.throws(
    () => buildFormPage('<html><head></head><body></body></html>', model, { baseHref: '/x/' }),
    (error) => error instanceof RouteError && error.code === ERROR_CODES.assetsMissing
  )
})

test('asset paths stay inside the renderer directory', () => {
  const root = process.platform === 'win32'
    ? 'C:\\plugin\\assets'
    : '/plugin/assets'
  assert.equal(
    resolveAssetPath('css/viewer.css', root),
    path.join(path.resolve(root), 'css', 'viewer.css')
  )
  assert.equal(assetMimeType('js/model-app.js'), 'text/javascript; charset=utf-8')
  assert.equal(assetMimeType('icons.svg'), 'image/svg+xml')
  assert.equal(assetMimeType('std-pictures/a.PNG'), 'image/png')
  assert.equal(assetMimeType('unknown.bin'), 'application/octet-stream')

  for (const bad of ['', '..%2Fpackage.json', '../package.json', '%00x', 'C:%5Cwindows', 'css%5Cviewer.css']) {
    assert.throws(
      () => resolveAssetPath(bad, root),
      (error) => error instanceof RouteError && error.code === ERROR_CODES.badRequest,
      `expected ${bad} to be refused`
    )
  }
})

test('assets route serves files with revalidating cache headers and guards traversal', async () => {
  const dir = mkdtempSync(path.join(tmpdir(), 'v8r-assets-'))
  try {
    mkdirSync(path.join(dir, 'css'))
    writeFileSync(path.join(dir, 'css', 'viewer.css'), '.a{color:red}')
    writeFileSync(path.join(dir, 'icons.svg'), '<svg/>')
    const resource = createResource({
      cli: fakeCli(),
      cache: createLruCache(),
      settings: DEFAULT_SETTINGS,
      stat: statSync,
      assetsDir: dir,
      platform: process.platform
    })

    const css = await resource(request('http://127.0.0.1:1/v8reader/assets/css/viewer.css'))
    assert.equal(css.status, 200)
    assert.equal(css.headers.get('content-type'), 'text/css; charset=utf-8')
    assert.equal(css.headers.get('cache-control'), 'no-cache')
    assert.equal(await css.text(), '.a{color:red}')
    const etag = css.headers.get('etag')
    assert.ok(etag !== null)

    const cached = await resource(request(
      'http://127.0.0.1:1/v8reader/assets/css/viewer.css',
      { headers: { 'if-none-match': etag } }
    ))
    assert.equal(cached.status, 304)
    assert.equal(await cached.text(), '')

    const head = await resource(request('http://127.0.0.1:1/v8reader/assets/icons.svg', { method: 'HEAD' }))
    assert.equal(head.status, 200)
    assert.equal(head.headers.get('content-type'), 'image/svg+xml')
    assert.equal(await head.text(), '')

    const missing = await resource(request('http://127.0.0.1:1/v8reader/assets/nope.js'))
    assert.equal(missing.status, 404)
    assert.equal((await missing.json()).error.code, ERROR_CODES.notFound)

    const traversal = await resource(request('http://127.0.0.1:1/v8reader/assets/..%2fpackage.json'))
    assert.equal(traversal.status, 400)
    assert.equal((await traversal.json()).error.code, ERROR_CODES.badRequest)
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
})

test('assets route explains a missing renderer copy', async () => {
  const resource = createResource({
    cli: fakeCli(),
    cache: createLruCache(),
    settings: DEFAULT_SETTINGS,
    stat: statSync,
    assetsDir: path.join(tmpdir(), `v8r-no-assets-${Date.now()}`),
    platform: process.platform
  })
  const response = await resource(request('http://127.0.0.1:1/v8reader/assets/model.html'))
  assert.equal(response.status, 503)
  assert.equal((await response.json()).error.code, ERROR_CODES.assetsMissing)
})

test('form route inlines the model, caches it, and maps failures', async () => {
  const dir = mkdtempSync(path.join(tmpdir(), 'v8r-form-'))
  try {
    writeFileSync(
      path.join(dir, 'model.html'),
      '<!doctype html><html><head><meta charset="utf-8"></head><body>'
      + '<script src="js/model-app.js"></script></body></html>'
    )
    const formModel = {
      tag: 'Form',
      version: '2.20',
      name: 'Кнопки',
      childItemsRoot: [],
      properties: {}
    }
    const cli = fakeCli({
      stdout: (args) => (args[0] === 'form-model'
        ? JSON.stringify(formModel)
        : JSON.stringify(viewDocument()))
    })
    const resource = createResource({
      cli,
      cache: createLruCache(),
      settings: DEFAULT_SETTINGS,
      stat: fakeStat(),
      assetsDir: dir,
      platform: process.platform
    })
    const object = 'e3489bca-d691-53a8-8feb-890730540f20'
    const url = `http://127.0.0.1:1/v8reader/form?path=${encodeURIComponent(FILE)}`
      + `&object=${encodeURIComponent(object)}&form=${encodeURIComponent('Кнопки')}`

    const page = await resource(request(url))
    assert.equal(page.status, 200)
    assert.match(page.headers.get('content-type'), /text\/html/)
    const html = await page.text()
    assert.ok(html.includes('window.__QV8_MODEL__ = {"tag":"Form"'))
    assert.ok(html.includes('<base href="/v8reader/assets/">'))
    assert.equal(cli.calls.length, 1)
    assert.deepEqual(cli.calls[0].args, [
      'form-model', path.normalize(FILE), object, '--form', 'Кнопки'
    ])
    assert.equal(typeof cli.calls[0].options.cwd, 'string')

    const again = await (await resource(request(url))).text()
    assert.equal(cli.calls.length, 1, 'the model must be served from the cache')
    assert.equal(again, html)

    const head = await resource(request(url, { method: 'HEAD' }))
    assert.equal(head.status, 200)
    assert.equal(await head.text(), '')

    const noForm = await resource(request(
      `http://127.0.0.1:1/v8reader/form?path=${encodeURIComponent(FILE)}&object=${encodeURIComponent(object)}`
    ))
    assert.equal(noForm.status, 400)
    assert.equal((await noForm.json()).error.code, ERROR_CODES.badRequest)

    const noObject = await resource(request(
      `http://127.0.0.1:1/v8reader/form?path=${encodeURIComponent(FILE)}&form=${encodeURIComponent('Кнопки')}`
    ))
    assert.equal(noObject.status, 400)

    const failed = createResource({
      cli: fakeCli({ code: 1 }),
      cache: createLruCache(),
      settings: DEFAULT_SETTINGS,
      stat: fakeStat(),
      assetsDir: dir,
      platform: process.platform
    })
    const failedPage = await failed(request(url))
    assert.equal(failedPage.status, 500)
    assert.equal((await failedPage.json()).error.code, ERROR_CODES.cliFailed)

    const malformed = createResource({
      cli: fakeCli({ stdout: () => '{"not":"a model"}' }),
      cache: createLruCache(),
      settings: DEFAULT_SETTINGS,
      stat: fakeStat(),
      assetsDir: dir,
      platform: process.platform
    })
    const malformedPage = await malformed(request(url))
    assert.equal(malformedPage.status, 502)
    assert.equal((await malformedPage.json()).error.code, ERROR_CODES.cliFailed)
  } finally {
    rmSync(dir, { recursive: true, force: true })
  }
})

test('mxl route serves the exported page, caches it, and maps failures', async () => {
  const page = '<!DOCTYPE html>\n<html lang="ru"><head><meta charset="utf-8">'
    + '<style>.mxl{width:200px}</style></head><body><div class="sheet">'
    + '<table class="mxl"><tr><td>Итого</td></tr></table></div></body></html>'
  const cli = fakeCli({
    stdout: (args) => (args[0] === 'mxl' ? page : JSON.stringify(viewDocument()))
  })
  const resource = createResource({
    cli,
    cache: createLruCache(),
    settings: DEFAULT_SETTINGS,
    stat: fakeStat(),
    platform: process.platform
  })
  const object = 'e3489bca-d691-53a8-8feb-890730540f20'
  const url = `http://127.0.0.1:1/v8reader/mxl?path=${encodeURIComponent(FILE)}`
    + `&object=${encodeURIComponent(object)}&template=${encodeURIComponent('Макет1')}`

  const response = await resource(request(url))
  assert.equal(response.status, 200)
  assert.match(response.headers.get('content-type'), /text\/html/)
  assert.equal(await response.text(), page)
  assert.match(response.headers.get('content-security-policy'), /default-src 'none'/)
  assert.equal(response.headers.get('x-content-type-options'), 'nosniff')
  assert.equal(cli.calls.length, 1)
  assert.deepEqual(cli.calls[0].args, [
    'mxl', path.normalize(FILE), object, '--template', 'Макет1', '--html', '-'
  ])
  assert.equal(typeof cli.calls[0].options.cwd, 'string')
  assert.equal(cli.calls[0].options.maxBytes, DEFAULT_SETTINGS.maxJsonBytes)

  const again = await resource(request(url))
  assert.equal(await again.text(), page)
  assert.equal(cli.calls.length, 1, 'the page must be served from the cache')

  const head = await resource(request(url, { method: 'HEAD' }))
  assert.equal(head.status, 200)
  assert.equal(await head.text(), '')

  const noTemplate = await resource(request(
    `http://127.0.0.1:1/v8reader/mxl?path=${encodeURIComponent(FILE)}&object=${encodeURIComponent(object)}`
  ))
  assert.equal(noTemplate.status, 400)
  assert.equal((await noTemplate.json()).error.code, ERROR_CODES.badRequest)

  const noObject = await resource(request(
    `http://127.0.0.1:1/v8reader/mxl?path=${encodeURIComponent(FILE)}&template=${encodeURIComponent('Макет1')}`
  ))
  assert.equal(noObject.status, 400)

  const failed = createResource({
    cli: fakeCli({ code: 1 }),
    cache: createLruCache(),
    settings: DEFAULT_SETTINGS,
    stat: fakeStat(),
    platform: process.platform
  })
  const failedPage = await failed(request(url))
  assert.equal(failedPage.status, 500)
  assert.equal((await failedPage.json()).error.code, ERROR_CODES.cliFailed)

  const malformed = createResource({
    cli: fakeCli({ stdout: () => '{"not":"a page"}' }),
    cache: createLruCache(),
    settings: DEFAULT_SETTINGS,
    stat: fakeStat(),
    platform: process.platform
  })
  const malformedPage = await malformed(request(url))
  assert.equal(malformedPage.status, 502)
  assert.equal((await malformedPage.json()).error.code, ERROR_CODES.cliFailed)
})

test('real CLI: mxl route reports a container without templates', async (t) => {
  const fixture = path.resolve(packageDir, '..', '..', 'tests', 'fixtures', 'controlbase', '1Cv8_controls.cf')
  const plugin = createPlugin({ config: {}, packageDir })
  const resolved = await plugin.cli.resolve().catch((error) => error)
  if (resolved instanceof RouteError || !existsSync(fixture)) {
    plugin.dispose()
    t.skip(`нет v8reader_cli или фикстуры: ${fixture}`)
    return
  }

  try {
    const response = await plugin.resource(request(
      `http://127.0.0.1:1/v8reader/mxl?path=${encodeURIComponent(fixture)}`
      + `&object=${encodeURIComponent('Контролы')}&template=${encodeURIComponent('НетТакого')}`
    ))
    assert.equal(response.status, 500)
    const body = await response.json()
    assert.equal(body.error.code, ERROR_CODES.cliFailed)
    assert.match(body.error.message, /макет/i)
  } finally {
    plugin.dispose()
  }
})

test('real CLI: form page and renderer assets of the control fixture', async (t) => {
  const fixture = path.resolve(packageDir, '..', '..', 'tests', 'fixtures', 'controlbase', '1Cv8_controls.cf')
  const assets = path.join(packageDir, 'assets', 'formpreview', 'model.html')
  const plugin = createPlugin({ config: {}, packageDir })
  const resolved = await plugin.cli.resolve().catch((error) => error)
  if (resolved instanceof RouteError || !existsSync(fixture) || !existsSync(assets)) {
    plugin.dispose()
    t.skip(`нет v8reader_cli, фикстуры или ресурсов рендерера: ${fixture}`)
    return
  }

  try {
    const url = `http://127.0.0.1:1/v8reader/form?path=${encodeURIComponent(fixture)}`
      + `&object=${encodeURIComponent('Контролы')}&form=${encodeURIComponent('Кнопки')}`
    const page = await plugin.resource(request(url))
    assert.equal(page.status, 200)
    const html = await page.text()
    assert.match(html, /window\.__QV8_MODEL__ = \{/)
    assert.match(html, /"tag":"Form"/)
    assert.match(html, /<base href="\/v8reader\/assets\/">/)

    const css = await plugin.resource(request('http://127.0.0.1:1/v8reader/assets/css/viewer.css'))
    assert.equal(css.status, 200)
    assert.match(css.headers.get('content-type'), /text\/css/)
    assert.ok((await css.text()).length > 1000)
  } finally {
    plugin.dispose()
  }
})

test('real CLI: state of the control fixture', async (t) => {
  const fixture = path.resolve(packageDir, '..', '..', 'tests', 'fixtures', 'controlbase', '1Cv8_controls.cf')
  const plugin = createPlugin({ config: {}, packageDir })
  const resolved = await plugin.cli.resolve().catch((error) => error)
  if (resolved instanceof RouteError || !existsSync(fixture)) {
    plugin.dispose()
    t.skip(`нет v8reader_cli или фикстуры: ${fixture}`)
    return
  }

  try {
    const response = await plugin.resource(request(
      `http://127.0.0.1:1/v8reader/state?path=${encodeURIComponent(fixture)}`
    ))
    assert.equal(response.status, 200)
    const body = await response.json()
    assert.equal(body.schema, 1)
    assert.ok(body.categories.length > 0)
    assert.equal(typeof body.file.kind, 'string')
    assert.ok(response.headers.get('etag') !== null)
  } finally {
    plugin.dispose()
  }
})
