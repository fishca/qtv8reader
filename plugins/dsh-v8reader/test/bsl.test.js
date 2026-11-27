/**
 * BSL tokenizer tests (`node --test`).
 *
 * The expected shapes follow `src/gui/BslHighlighter.cpp`: which spans are
 * painted, not how they are laid out.
 */
import { test } from 'node:test'
import assert from 'node:assert/strict'

import {
  BSL_KEYWORDS,
  isBoldToken,
  tokenizeLine,
  tokenizeModule
} from '../src/client/bsl.js'

/**
 * Summarize the token types of one line.
 * @param {string} line - the source line.
 * @returns {string} types separated by a space.
 */
function types(line) {
  return tokenizeLine(line).map((token) => token.type).join(' ')
}

test('keywords are matched case-insensitively in Russian and English', () => {
  assert.equal(types('Процедура Тест() Экспорт'), 'keyword plain keyword')
  assert.equal(types('ПРОЦЕДУРА Тест()'), 'keyword plain')
  assert.equal(types('procedure Test()'), 'keyword plain')
  assert.equal(types('Если Истина Тогда'), 'keyword plain keyword plain keyword')

  const words = tokenizeLine('Процедура Тест()').filter((token) => token.type === 'keyword')
  assert.deepEqual(words.map((token) => token.text), ['Процедура'])
  assert.ok(BSL_KEYWORDS.includes('КонецПроцедуры'))
  assert.ok(BSL_KEYWORDS.includes('EndFunction'))
  assert.equal(isBoldToken('keyword'), true)
  assert.equal(isBoldToken('plain'), false)
})

test('comments start at // and swallow the rest of the line', () => {
  assert.deepEqual(tokenizeLine('// комментарий'), [
    { type: 'comment', text: '// комментарий' }
  ])
  assert.equal(types('// Процедура Если'), 'comment')
  assert.equal(types('А = 1; // хвост'), 'plain number plain comment')
  assert.equal(types('А = 1 // хвост'), 'plain number plain comment')
  assert.equal(types('Путь = "a//b"'), 'plain string')
})

test('strings keep doubled quotes and protect their insides', () => {
  const tokens = tokenizeLine('А = "Строка ""кавычки"""; // хвост')
  assert.equal(tokens.map((token) => token.type).join(' '), 'plain string plain comment')
  assert.equal(tokens[1].text, '"Строка ""кавычки"""')
  assert.equal(tokens[3].text, '// хвост')

  assert.deepEqual(tokenizeLine('"незакрытая'), [{ type: 'string', text: '"незакрытая' }])
  assert.equal(types('"Процедура"'), 'string')
})

test('directives need the start of a line, annotations do not', () => {
  assert.equal(types('  #Если Сервер Тогда'), 'plain directive plain keyword')
  const directive = tokenizeLine('  #Если Сервер Тогда')[1]
  assert.deepEqual(directive, { type: 'directive', text: '#Если' })

  assert.equal(types('Сервер #Если Тогда'), 'plain keyword plain keyword')
  assert.equal(types('#КонецЕсли'), 'directive')

  assert.equal(types('&НаКлиенте'), 'annotation')
  assert.deepEqual(tokenizeLine('&НаКлиенте Процедура А()')[0], {
    type: 'annotation',
    text: '&НаКлиенте'
  })
  assert.equal(types('А & Б'), 'plain')
})

test('numbers are whole words with an optional fraction', () => {
  assert.equal(types('Число = 123.45;'), 'plain number plain')
  const number = tokenizeLine('Число = 123.45;')[1]
  assert.equal(number.text, '123.45')

  assert.equal(types('А1 = 12'), 'plain number')
  assert.equal(types('Версия 8.3.16'), 'plain number plain number')
  assert.equal(types('Цена 1.2.3'), 'plain number plain number')
  assert.equal(types('123abc'), 'plain')
})

test('module text is split into tokenized lines and CRLF is normalized', () => {
  const lines = tokenizeModule('Процедура А()\r\nВозврат;\r\nКонецПроцедуры')
  assert.equal(lines.length, 3)
  assert.deepEqual(lines[0].map((token) => token.text), ['Процедура', ' А()'])
  assert.equal(lines[1].map((token) => token.type).join(' '), 'keyword plain')

  assert.deepEqual(tokenizeLine(''), [])
  assert.deepEqual(tokenizeLine(undefined), [])
  assert.deepEqual(tokenizeLine('обычный текст').map((token) => token.type), ['plain'])
})
