/**
 * BSL tokenizer for the code view.
 *
 * Ported from the application's `src/gui/BslHighlighter.cpp`, which highlights
 * one line at a time: strings are single-line (a doubled `""` is an escaped
 * quote), `//` starts a comment, `#…` is a preprocessor directive only at the
 * start of a line, `&…` is an annotation, and the keyword list is matched case
 * insensitively. Keywords inside strings and comments stay part of the string
 * or comment, exactly as the Qt highlighter paints them.
 *
 * The tokenizer is plain JavaScript with no React dependency so the Node tests
 * can check it directly.
 */

/** Token types the view styles. */
export const TOKEN_TYPES = Object.freeze([
  'plain',
  'keyword',
  'directive',
  'annotation',
  'number',
  'string',
  'comment'
])

/**
 * BSL keywords: Russian and the English spellings accepted by the platform.
 * Kept identical to the Qt highlighter so both code views agree.
 */
export const BSL_KEYWORDS = Object.freeze([
  'Процедура', 'Функция', 'КонецПроцедуры', 'КонецФункции',
  'Если', 'Тогда', 'Иначе', 'ИначеЕсли', 'КонецЕсли',
  'Для', 'Каждого', 'Из', 'По', 'Цикл', 'КонецЦикла',
  'Пока', 'Возврат', 'Перем', 'Экспорт', 'Знач', 'Новый',
  'Не', 'И', 'Или', 'Истина', 'Ложь', 'Неопределено',
  'Попытка', 'Исключение', 'КонецПопытки', 'ВызватьИсключение',
  'Перейти', 'Продолжить', 'Прервать', 'Ждать', 'Асинх', 'Выполнить',
  'Procedure', 'Function', 'EndProcedure', 'EndFunction',
  'If', 'Then', 'Else', 'ElsIf', 'EndIf', 'For', 'Each', 'In', 'To',
  'Do', 'While', 'EndDo', 'Return', 'Var', 'Export', 'Val', 'New',
  'Not', 'And', 'Or', 'True', 'False', 'Undefined',
  'Try', 'Except', 'EndTry', 'Raise', 'Goto', 'Continue', 'Break',
  'Async'
])

/** Lowercase → canonical spelling, so a keyword check is one map lookup. */
const KEYWORD_LOOKUP = new Map(BSL_KEYWORDS.map((word) => [word.toLowerCase(), word]))

/** Sticky identifier scanner; `\p{L}` covers Cyrillic and Latin. */
const WORD_PATTERN = /[\p{L}_][\p{L}\p{N}_]*/uy

/** Sticky number scanner: digits with one optional fractional part. */
const NUMBER_PATTERN = /\d+(?:\.\d+)?/y

/** Directive at the beginning of a line, after optional indentation. */
const DIRECTIVE_PATTERN = /^([ \t]*)#([\p{L}\p{N}_]+)/u

/**
 * Whether a character can be part of an identifier or a number boundary.
 * @param {string} character - one character.
 * @returns {boolean} true for a Unicode letter, digit or underscore.
 */
function isWordCharacter(character) {
  return character !== undefined && /[\p{L}\p{N}_]/u.test(character)
}

/**
 * Tokenize one line of BSL.
 * @param {string} line - the line, without its newline.
 * @returns {Array<{ type: string, text: string }>} tokens in source order.
 */
export function tokenizeLine(line) {
  const text = String(line ?? '')
  const tokens = []
  let plain = ''
  let index = 0

  const flush = () => {
    if (plain !== '') {
      tokens.push({ type: 'plain', text: plain })
      plain = ''
    }
  }
  const emit = (type, value) => {
    flush()
    tokens.push({ type, text: value })
  }

  const directive = DIRECTIVE_PATTERN.exec(text)
  if (directive !== null) {
    const start = directive[1].length
    if (start > 0) plain += text.slice(0, start)
    emit('directive', `#${directive[2]}`)
    index = start + 1 + directive[2].length
  }

  while (index < text.length) {
    const character = text[index]

    if (character === '"') {
      let end = index + 1
      while (end < text.length) {
        if (text[end] === '"') {
          if (text[end + 1] === '"') {
            end += 2
            continue
          }
          end += 1
          break
        }
        end += 1
      }
      emit('string', text.slice(index, end))
      index = end
      continue
    }

    if (character === '/' && text[index + 1] === '/') {
      emit('comment', text.slice(index))
      index = text.length
      continue
    }

    if (character === '&') {
      const probe = new RegExp(WORD_PATTERN.source, 'uy')
      probe.lastIndex = index + 1
      const match = probe.exec(text)
      if (match !== null) {
        emit('annotation', `&${match[0]}`)
        index += 1 + match[0].length
        continue
      }
    }

    if (character >= '0' && character <= '9' && !isWordCharacter(text[index - 1])) {
      NUMBER_PATTERN.lastIndex = index
      const match = NUMBER_PATTERN.exec(text)
      if (match !== null && !isWordCharacter(text[index + match[0].length])) {
        emit('number', match[0])
        index += match[0].length
        continue
      }
    }

    if (/[\p{L}_]/u.test(character)) {
      WORD_PATTERN.lastIndex = index
      const match = WORD_PATTERN.exec(text)
      const word = match === null ? character : match[0]
      const keyword = KEYWORD_LOOKUP.get(word.toLowerCase())
      if (keyword !== undefined) emit('keyword', word)
      else plain += word
      index += word.length
      continue
    }

    plain += character
    index += 1
  }

  flush()
  return tokens
}

/**
 * Tokenize a whole module into lines of tokens.
 * @param {string} text - the module text.
 * @returns {Array<Array<{ type: string, text: string }>>} one token array per line.
 */
export function tokenizeModule(text) {
  const source = String(text ?? '').replace(/\r\n?/g, '\n')
  return source.split('\n').map(tokenizeLine)
}

/**
 * Whether a token type is a keyword, for the bold setting of the view.
 * @param {string} type - a token type.
 * @returns {boolean} true for keywords and directives.
 */
export function isBoldToken(type) {
  return type === 'keyword' || type === 'directive'
}
