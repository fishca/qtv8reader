/* Сгенерировано build.mjs из src/client; правьте исходники. */
window.__ModuleLoader__.load({
  id: '@fishca/dsh-v8reader',
  factory(require) {
    const module = { exports: {} };
    const exports = module.exports;
    var __create = Object.create;
    var __defProp = Object.defineProperty;
    var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
    var __getOwnPropNames = Object.getOwnPropertyNames;
    var __getProtoOf = Object.getPrototypeOf;
    var __hasOwnProp = Object.prototype.hasOwnProperty;
    var __export = (target, all) => {
      for (var name in all)
        __defProp(target, name, { get: all[name], enumerable: true });
    };
    var __copyProps = (to, from, except, desc) => {
      if (from && typeof from === "object" || typeof from === "function") {
        for (let key of __getOwnPropNames(from))
          if (!__hasOwnProp.call(to, key) && key !== except)
            __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
      }
      return to;
    };
    var __toESM = (mod, isNodeMode, target) => (target = mod != null ? __create(__getProtoOf(mod)) : {}, __copyProps(
      // If the importer is in node compatibility mode or this is not an ESM
      // file that has been converted to a CommonJS file using a Babel-
      // compatible transform (i.e. "__esModule" has not been set), then set
      // "default" to the CommonJS "module.exports" for node compatibility.
      isNodeMode || !mod || !mod.__esModule ? __defProp(target, "default", { value: mod, enumerable: true }) : target,
      mod
    ));
    var __toCommonJS = (mod) => __copyProps(__defProp({}, "__esModule", { value: true }), mod);
    
    // src/client/index.jsx
    var index_exports = {};
    __export(index_exports, {
      LOCALE_NAMESPACE: () => LOCALE_NAMESPACE,
      TYPE_ID: () => TYPE_ID,
      TYPE_KIND: () => TYPE_KIND,
      apply: () => apply,
      inject: () => inject
    });
    module.exports = __toCommonJS(index_exports);
    var import_react6 = __toESM(require("react"), 1);
    
    // src/client/PanelView.jsx
    var import_react5 = __toESM(require("react"), 1);
    
    // src/client/api.js
    var StateError = class extends Error {
      /**
       * @param {string} code - the Host error code.
       * @param {string} message - the Host message.
       * @param {number} status - the HTTP status.
       * @param {object} [detail] - extra facts, for example the searched CLI paths.
       */
      constructor(code, message, status, detail) {
        super(message);
        this.name = "StateError";
        this.code = code;
        this.status = status;
        this.detail = detail;
      }
    };
    async function readJson(response, fallbackCode) {
      let body = null;
      try {
        body = await response.json();
      } catch {
        body = null;
      }
      if (!response.ok || body === null) {
        const failure = body?.error ?? {};
        throw new StateError(
          typeof failure.code === "string" ? failure.code : fallbackCode,
          typeof failure.message === "string" ? failure.message : `HTTP ${response.status}`,
          response.status,
          failure.detail
        );
      }
      return body;
    }
    function buildStateUrl(path, object = "", module2 = "") {
      const params = new URLSearchParams({ path });
      if (object !== "") params.set("object", object);
      if (module2 !== "") params.set("module", module2);
      return `/v8reader/state?${params.toString()}`;
    }
    function buildBrowseUrl(dir = "", all = false) {
      const params = new URLSearchParams();
      if (dir !== "") params.set("path", dir);
      if (all) params.set("all", "1");
      const search = params.toString();
      return search === "" ? "/v8reader/browse" : `/v8reader/browse?${search}`;
    }
    function buildFormUrl(path, object, form) {
      const params = new URLSearchParams({ path, object, form });
      return `/v8reader/form?${params.toString()}`;
    }
    async function fetchState({
      path,
      object = "",
      module: module2 = "",
      signal,
      noStore = false,
      fetchImpl = globalThis.fetch
    }) {
      const response = await fetchImpl(buildStateUrl(path, object, module2), {
        signal,
        cache: noStore ? "no-store" : "default",
        headers: { accept: "application/json" }
      });
      const body = await readJson(response, "cli-failed");
      if (body.schema !== 1) {
        throw new StateError("cli-failed", `unsupported schema: ${String(body.schema)}`, response.status);
      }
      return body;
    }
    async function fetchBrowse({
      dir = "",
      all = false,
      signal,
      fetchImpl = globalThis.fetch
    }) {
      const response = await fetchImpl(buildBrowseUrl(dir, all), {
        signal,
        cache: "no-store",
        headers: { accept: "application/json" }
      });
      const body = await readJson(response, "bad-path");
      if (body.schema !== 1 || typeof body.dir !== "string" || !Array.isArray(body.entries)) {
        throw new StateError("bad-path", "browse answer is not the contract", response.status);
      }
      return body;
    }
    
    // src/client/CodeView.jsx
    var import_react = __toESM(require("react"), 1);
    
    // src/client/bsl.js
    var TOKEN_TYPES = Object.freeze([
      "plain",
      "keyword",
      "directive",
      "annotation",
      "number",
      "string",
      "comment"
    ]);
    var BSL_KEYWORDS = Object.freeze([
      "\u041F\u0440\u043E\u0446\u0435\u0434\u0443\u0440\u0430",
      "\u0424\u0443\u043D\u043A\u0446\u0438\u044F",
      "\u041A\u043E\u043D\u0435\u0446\u041F\u0440\u043E\u0446\u0435\u0434\u0443\u0440\u044B",
      "\u041A\u043E\u043D\u0435\u0446\u0424\u0443\u043D\u043A\u0446\u0438\u0438",
      "\u0415\u0441\u043B\u0438",
      "\u0422\u043E\u0433\u0434\u0430",
      "\u0418\u043D\u0430\u0447\u0435",
      "\u0418\u043D\u0430\u0447\u0435\u0415\u0441\u043B\u0438",
      "\u041A\u043E\u043D\u0435\u0446\u0415\u0441\u043B\u0438",
      "\u0414\u043B\u044F",
      "\u041A\u0430\u0436\u0434\u043E\u0433\u043E",
      "\u0418\u0437",
      "\u041F\u043E",
      "\u0426\u0438\u043A\u043B",
      "\u041A\u043E\u043D\u0435\u0446\u0426\u0438\u043A\u043B\u0430",
      "\u041F\u043E\u043A\u0430",
      "\u0412\u043E\u0437\u0432\u0440\u0430\u0442",
      "\u041F\u0435\u0440\u0435\u043C",
      "\u042D\u043A\u0441\u043F\u043E\u0440\u0442",
      "\u0417\u043D\u0430\u0447",
      "\u041D\u043E\u0432\u044B\u0439",
      "\u041D\u0435",
      "\u0418",
      "\u0418\u043B\u0438",
      "\u0418\u0441\u0442\u0438\u043D\u0430",
      "\u041B\u043E\u0436\u044C",
      "\u041D\u0435\u043E\u043F\u0440\u0435\u0434\u0435\u043B\u0435\u043D\u043E",
      "\u041F\u043E\u043F\u044B\u0442\u043A\u0430",
      "\u0418\u0441\u043A\u043B\u044E\u0447\u0435\u043D\u0438\u0435",
      "\u041A\u043E\u043D\u0435\u0446\u041F\u043E\u043F\u044B\u0442\u043A\u0438",
      "\u0412\u044B\u0437\u0432\u0430\u0442\u044C\u0418\u0441\u043A\u043B\u044E\u0447\u0435\u043D\u0438\u0435",
      "\u041F\u0435\u0440\u0435\u0439\u0442\u0438",
      "\u041F\u0440\u043E\u0434\u043E\u043B\u0436\u0438\u0442\u044C",
      "\u041F\u0440\u0435\u0440\u0432\u0430\u0442\u044C",
      "\u0416\u0434\u0430\u0442\u044C",
      "\u0410\u0441\u0438\u043D\u0445",
      "\u0412\u044B\u043F\u043E\u043B\u043D\u0438\u0442\u044C",
      "Procedure",
      "Function",
      "EndProcedure",
      "EndFunction",
      "If",
      "Then",
      "Else",
      "ElsIf",
      "EndIf",
      "For",
      "Each",
      "In",
      "To",
      "Do",
      "While",
      "EndDo",
      "Return",
      "Var",
      "Export",
      "Val",
      "New",
      "Not",
      "And",
      "Or",
      "True",
      "False",
      "Undefined",
      "Try",
      "Except",
      "EndTry",
      "Raise",
      "Goto",
      "Continue",
      "Break",
      "Async"
    ]);
    var KEYWORD_LOOKUP = new Map(BSL_KEYWORDS.map((word) => [word.toLowerCase(), word]));
    var WORD_PATTERN = /[\p{L}_][\p{L}\p{N}_]*/uy;
    var NUMBER_PATTERN = /\d+(?:\.\d+)?/y;
    var DIRECTIVE_PATTERN = /^([ \t]*)#([\p{L}\p{N}_]+)/u;
    function isWordCharacter(character) {
      return character !== void 0 && /[\p{L}\p{N}_]/u.test(character);
    }
    function tokenizeLine(line) {
      const text = String(line ?? "");
      const tokens = [];
      let plain = "";
      let index = 0;
      const flush = () => {
        if (plain !== "") {
          tokens.push({ type: "plain", text: plain });
          plain = "";
        }
      };
      const emit = (type, value) => {
        flush();
        tokens.push({ type, text: value });
      };
      const directive = DIRECTIVE_PATTERN.exec(text);
      if (directive !== null) {
        const start = directive[1].length;
        if (start > 0) plain += text.slice(0, start);
        emit("directive", `#${directive[2]}`);
        index = start + 1 + directive[2].length;
      }
      while (index < text.length) {
        const character = text[index];
        if (character === '"') {
          let end = index + 1;
          while (end < text.length) {
            if (text[end] === '"') {
              if (text[end + 1] === '"') {
                end += 2;
                continue;
              }
              end += 1;
              break;
            }
            end += 1;
          }
          emit("string", text.slice(index, end));
          index = end;
          continue;
        }
        if (character === "/" && text[index + 1] === "/") {
          emit("comment", text.slice(index));
          index = text.length;
          continue;
        }
        if (character === "&") {
          const probe = new RegExp(WORD_PATTERN.source, "uy");
          probe.lastIndex = index + 1;
          const match = probe.exec(text);
          if (match !== null) {
            emit("annotation", `&${match[0]}`);
            index += 1 + match[0].length;
            continue;
          }
        }
        if (character >= "0" && character <= "9" && !isWordCharacter(text[index - 1])) {
          NUMBER_PATTERN.lastIndex = index;
          const match = NUMBER_PATTERN.exec(text);
          if (match !== null && !isWordCharacter(text[index + match[0].length])) {
            emit("number", match[0]);
            index += match[0].length;
            continue;
          }
        }
        if (/[\p{L}_]/u.test(character)) {
          WORD_PATTERN.lastIndex = index;
          const match = WORD_PATTERN.exec(text);
          const word = match === null ? character : match[0];
          const keyword = KEYWORD_LOOKUP.get(word.toLowerCase());
          if (keyword !== void 0) emit("keyword", word);
          else plain += word;
          index += word.length;
          continue;
        }
        plain += character;
        index += 1;
      }
      flush();
      return tokens;
    }
    
    // src/client/code.js
    var CODE_SETTINGS_KEY = "dsh.v8reader.code.v1";
    var MIN_FONT_SIZE = 10;
    var MAX_FONT_SIZE = 20;
    var DEFAULT_CODE_SETTINGS = Object.freeze({
      enabled: true,
      wrap: false,
      size: 12.5
    });
    function clampNumber(value, fallback, min, max) {
      if (typeof value !== "number" || !Number.isFinite(value)) return fallback;
      return Math.min(max, Math.max(min, value));
    }
    function readCodeSettings(storage = globalThis.localStorage) {
      let raw = null;
      try {
        raw = storage?.getItem(CODE_SETTINGS_KEY) ?? null;
      } catch {
        raw = null;
      }
      if (raw === null) return { ...DEFAULT_CODE_SETTINGS };
      let parsed;
      try {
        parsed = JSON.parse(raw);
      } catch {
        return { ...DEFAULT_CODE_SETTINGS };
      }
      if (parsed === null || typeof parsed !== "object") return { ...DEFAULT_CODE_SETTINGS };
      return {
        enabled: typeof parsed.enabled === "boolean" ? parsed.enabled : DEFAULT_CODE_SETTINGS.enabled,
        wrap: typeof parsed.wrap === "boolean" ? parsed.wrap : DEFAULT_CODE_SETTINGS.wrap,
        size: clampNumber(parsed.size, DEFAULT_CODE_SETTINGS.size, MIN_FONT_SIZE, MAX_FONT_SIZE)
      };
    }
    function writeCodeSettings(settings, storage = globalThis.localStorage) {
      try {
        storage?.setItem(CODE_SETTINGS_KEY, JSON.stringify(settings));
      } catch {
      }
    }
    
    // src/client/CodeView.jsx
    var LINE_STEP = 1500;
    async function copyText(text) {
      try {
        if (navigator.clipboard?.writeText !== void 0) {
          await navigator.clipboard.writeText(text);
          return true;
        }
      } catch {
      }
      try {
        const area = document.createElement("textarea");
        area.value = text;
        area.setAttribute("readonly", "");
        area.style.position = "fixed";
        area.style.opacity = "0";
        document.body.appendChild(area);
        area.select();
        const copied = document.execCommand("copy");
        area.remove();
        return copied;
      } catch {
        return false;
      }
    }
    function CodeView({ text, scheme = "light", translate: t }) {
      const [settings, setSettings] = (0, import_react.useState)(() => readCodeSettings());
      const [visible, setVisible] = (0, import_react.useState)(LINE_STEP);
      const [copied, setCopied] = (0, import_react.useState)(false);
      const timer = (0, import_react.useRef)(void 0);
      const source = typeof text === "string" ? text : "";
      const lines = (0, import_react.useMemo)(() => source.replace(/\r\n?/g, "\n").split("\n"), [source]);
      const gutterWidth = `${String(lines.length).length + 0.5}ch`;
      const shown = Math.min(visible, lines.length);
      const hasMore = lines.length > shown;
      const tokens = (0, import_react.useMemo)(
        () => settings.enabled ? lines.slice(0, shown).map(tokenizeLine) : [],
        [lines, shown, settings.enabled]
      );
      (0, import_react.useEffect)(() => {
        setVisible(LINE_STEP);
        setCopied(false);
      }, [source]);
      (0, import_react.useEffect)(() => () => {
        if (timer.current !== void 0) clearTimeout(timer.current);
      }, []);
      const change = (0, import_react.useCallback)((patch) => {
        setSettings((current) => {
          const next = { ...current, ...patch };
          writeCodeSettings(next);
          return next;
        });
      }, []);
      const onCopy = (0, import_react.useCallback)(async () => {
        const done = await copyText(source);
        setCopied(done);
        if (timer.current !== void 0) clearTimeout(timer.current);
        timer.current = setTimeout(() => {
          setCopied(false);
        }, 1600);
      }, [source]);
      return /* @__PURE__ */ import_react.default.createElement("div", { className: "v8r-codeview", "data-scheme": scheme, "data-wrap": settings.wrap ? "true" : "false" }, /* @__PURE__ */ import_react.default.createElement("div", { className: "v8r-codeview-bar" }, /* @__PURE__ */ import_react.default.createElement(
        "button",
        {
          type: "button",
          className: "v8r-button v8r-button--ghost",
          "aria-pressed": settings.enabled ? "true" : "false",
          title: t("code.highlight.title"),
          onClick: () => {
            change({ enabled: !settings.enabled });
          }
        },
        t("code.highlight")
      ), /* @__PURE__ */ import_react.default.createElement(
        "button",
        {
          type: "button",
          className: "v8r-button v8r-button--ghost",
          "aria-pressed": settings.wrap ? "true" : "false",
          title: t("code.wrap.title"),
          onClick: () => {
            change({ wrap: !settings.wrap });
          }
        },
        t("code.wrap")
      ), /* @__PURE__ */ import_react.default.createElement("span", { className: "v8r-codeview-size" }, /* @__PURE__ */ import_react.default.createElement(
        "button",
        {
          type: "button",
          className: "v8r-button v8r-button--ghost",
          title: t("code.font.smaller"),
          "aria-label": t("code.font.smaller"),
          disabled: settings.size <= MIN_FONT_SIZE,
          onClick: () => {
            change({ size: clampNumber(settings.size - 1, settings.size, MIN_FONT_SIZE, MAX_FONT_SIZE) });
          }
        },
        "\u2212"
      ), /* @__PURE__ */ import_react.default.createElement("span", { className: "v8r-codeview-size-value" }, settings.size), /* @__PURE__ */ import_react.default.createElement(
        "button",
        {
          type: "button",
          className: "v8r-button v8r-button--ghost",
          title: t("code.font.larger"),
          "aria-label": t("code.font.larger"),
          disabled: settings.size >= MAX_FONT_SIZE,
          onClick: () => {
            change({ size: clampNumber(settings.size + 1, settings.size, MIN_FONT_SIZE, MAX_FONT_SIZE) });
          }
        },
        "+"
      )), /* @__PURE__ */ import_react.default.createElement("span", { className: "v8r-codeview-spacer" }), /* @__PURE__ */ import_react.default.createElement("span", { className: "v8r-muted" }, t("code.lines", { count: lines.length })), /* @__PURE__ */ import_react.default.createElement(
        "button",
        {
          type: "button",
          className: "v8r-button",
          onClick: () => {
            onCopy();
          }
        },
        copied ? t("code.copied") : t("code.copy")
      )), /* @__PURE__ */ import_react.default.createElement(
        "div",
        {
          className: "v8r-code",
          style: { "--v8r-code-size": `${settings.size}px`, "--v8r-gutter": gutterWidth },
          tabIndex: 0,
          role: "region",
          "aria-label": t("code.region")
        },
        Array.from({ length: shown }, (_, index) => {
          const lineTokens = tokens[index] ?? [];
          return /* @__PURE__ */ import_react.default.createElement("div", { className: "v8r-code-row", key: index }, /* @__PURE__ */ import_react.default.createElement("span", { className: "v8r-code-num", "aria-hidden": "true" }, index + 1), /* @__PURE__ */ import_react.default.createElement("span", { className: "v8r-code-text" }, settings.enabled ? lineTokens.map((token, tokenIndex) => /* @__PURE__ */ import_react.default.createElement("span", { className: `v8r-tok v8r-tok-${token.type}`, key: tokenIndex }, token.text)) : lines[index]));
        }),
        hasMore && /* @__PURE__ */ import_react.default.createElement("div", { className: "v8r-code-more" }, /* @__PURE__ */ import_react.default.createElement(
          "button",
          {
            type: "button",
            className: "v8r-button",
            onClick: () => {
              setVisible((current) => current + LINE_STEP);
            }
          },
          t("code.showMore", { rest: lines.length - shown })
        ))
      ));
    }
    
    // src/client/FilePicker.jsx
    var import_react2 = __toESM(require("react"), 1);
    
    // src/client/errors.js
    function browseErrorText(t, error) {
      const code = typeof error?.code === "string" ? error.code : "unknown";
      const reason = error?.detail?.reason;
      if (code === "bad-path") {
        if (reason === "directory-missing") return t("picker.error.missing");
        if (reason === "not-a-directory") return t("picker.error.notDirectory");
        return t("error.bad-path");
      }
      const key = `error.${code}`;
      const localized = t(key, error?.detail ?? {});
      return localized === key ? t("error.unknown") : localized;
    }
    
    // src/client/labels.js
    function formatBytes(bytes) {
      if (typeof bytes !== "number" || !Number.isFinite(bytes) || bytes < 0) return "";
      const units = ["\u0411", "\u041A\u0411", "\u041C\u0411", "\u0413\u0411"];
      let value = bytes;
      let index = 0;
      while (value >= 1024 && index < units.length - 1) {
        value /= 1024;
        index += 1;
      }
      const digits = index === 0 ? 0 : value < 10 ? 1 : 0;
      return `${value.toFixed(digits)} ${units[index]}`;
    }
    function fileKindLabel(t, kind) {
      const key = `kind.${String(kind ?? "")}`;
      const label = t(key);
      return label === key ? String(kind ?? "") : label;
    }
    function sectionLabel(t, section) {
      const key = `section.${String(section.kind ?? "")}`;
      const label = t(key);
      return label === key ? String(section.title ?? section.kind ?? "") : label;
    }
    function flattenEntries(entries, depth = 0) {
      const rows = [];
      for (const entry of entries) {
        rows.push({ entry, depth });
        if (Array.isArray(entry.children) && entry.children.length > 0) {
          rows.push(...flattenEntries(entry.children, depth + 1));
        }
      }
      return rows;
    }
    
    // src/client/paths.js
    function extensionOf(name) {
      const value = String(name ?? "");
      const dot = value.lastIndexOf(".");
      if (dot <= 0) return "";
      return value.slice(dot).toLowerCase();
    }
    function dirOf(filePath) {
      const raw = String(filePath ?? "").trim();
      if (raw === "") return "";
      const trailing = /[\\/]$/.test(raw);
      const value = raw.replace(/[\\/]+$/, "");
      if (value === "") return raw.includes("\\") ? "\\" : "/";
      if (/^[a-zA-Z]:$/.test(value)) return `${value}\\`;
      if (trailing) return value;
      const cut = Math.max(value.lastIndexOf("\\"), value.lastIndexOf("/"));
      if (cut < 0) return "";
      if (/^[a-zA-Z]:$/.test(value.slice(0, cut))) return value.slice(0, cut + 1);
      const head = value.slice(0, cut);
      if (/^\\\\[^\\/]+[\\/][^\\/]+$/.test(head)) return `${head}\\`;
      if (head === "") return "/";
      return head;
    }
    
    // src/client/FilePicker.jsx
    var FOCUSABLE = 'button:not([disabled]), input:not([disabled]), [tabindex]:not([tabindex="-1"])';
    function Row({ entry, openable, busy, reason, onOpen }) {
      return /* @__PURE__ */ import_react2.default.createElement(
        "button",
        {
          type: "button",
          className: "v8r-picker-row",
          "data-kind": entry.kind,
          "data-openable": openable ? "true" : "false",
          disabled: busy || !openable,
          title: openable ? entry.path : `${entry.path} \u2014 ${reason}`,
          onClick: () => {
            if (openable) onOpen(entry);
          }
        },
        /* @__PURE__ */ import_react2.default.createElement("span", { className: "v8r-picker-mark", "aria-hidden": "true" }, entry.kind === "dir" ? "\u25B8" : ""),
        /* @__PURE__ */ import_react2.default.createElement("span", { className: "v8r-picker-name" }, entry.name),
        entry.kind === "file" && /* @__PURE__ */ import_react2.default.createElement("span", { className: "v8r-picker-size" }, formatBytes(entry.size))
      );
    }
    function FilePicker({ initialDir = "", translate: t, onPick, onClose }) {
      const [dir, setDir] = (0, import_react2.useState)("");
      const [listing, setListing] = (0, import_react2.useState)(null);
      const [pathBar, setPathBar] = (0, import_react2.useState)(initialDir);
      const [filter, setFilter] = (0, import_react2.useState)("");
      const [showAll, setShowAll] = (0, import_react2.useState)(false);
      const [busy, setBusy] = (0, import_react2.useState)(true);
      const [error, setError] = (0, import_react2.useState)(null);
      const controller = (0, import_react2.useRef)(null);
      const layer = (0, import_react2.useRef)(null);
      const filterInput = (0, import_react2.useRef)(null);
      const crumbRef = (0, import_react2.useRef)(null);
      const fellBack = (0, import_react2.useRef)(false);
      const lastTarget = (0, import_react2.useRef)(initialDir);
      const load = (0, import_react2.useCallback)(async (target, all, { fallback = true } = {}) => {
        controller.current?.abort();
        const next = new AbortController();
        controller.current = next;
        lastTarget.current = target;
        setPathBar(target);
        setBusy(true);
        setError(null);
        try {
          const body = await fetchBrowse({ dir: target, all, signal: next.signal });
          if (next.signal.aborted) return;
          fellBack.current = false;
          setDir(body.dir);
          setPathBar(body.dir);
          setListing(body);
          setFilter("");
        } catch (failure) {
          if (next.signal.aborted || failure?.name === "AbortError") return;
          if (fallback && target !== "" && !fellBack.current) {
            fellBack.current = true;
            controller.current?.abort();
            await load("", all, { fallback: false });
            return;
          }
          setListing(null);
          setError(failure instanceof StateError ? failure : new StateError("unknown", String(failure?.message ?? failure), 0));
        } finally {
          if (!next.signal.aborted) setBusy(false);
        }
      }, []);
      (0, import_react2.useEffect)(() => {
        load(initialDir, false);
        filterInput.current?.focus?.();
        return () => {
          controller.current?.abort();
        };
      }, [initialDir, load]);
      (0, import_react2.useEffect)(() => {
        crumbRef.current?.scrollIntoView?.({ block: "nearest", inline: "end" });
      }, [dir]);
      const entries = Array.isArray(listing?.entries) ? listing.entries : [];
      const supported = Array.isArray(listing?.supported) ? listing.supported : [];
      const needle = filter.trim().toLowerCase();
      const shown = (0, import_react2.useMemo)(
        () => needle === "" ? entries : entries.filter(
          (entry) => entry.name.toLowerCase().includes(needle)
        ),
        [entries, needle]
      );
      const openable = (0, import_react2.useCallback)((entry) => {
        if (entry.kind === "dir") return true;
        if (supported.length === 0) return true;
        return supported.includes(extensionOf(entry.name));
      }, [supported]);
      const close = (0, import_react2.useCallback)(() => {
        onClose(dir);
      }, [dir, onClose]);
      const openEntry = (0, import_react2.useCallback)((entry) => {
        if (entry.kind === "dir") {
          load(entry.path, showAll, { fallback: false });
          return;
        }
        onPick(entry.path);
      }, [load, onPick, showAll]);
      const onKeyDown = (0, import_react2.useCallback)((event) => {
        if (event.key === "Escape") {
          event.stopPropagation();
          close();
          return;
        }
        if (event.key !== "Tab") return;
        const nodes = Array.from(layer.current?.querySelectorAll(FOCUSABLE) ?? []);
        if (nodes.length === 0) return;
        const first = nodes[0];
        const last = nodes[nodes.length - 1];
        if (event.shiftKey && document.activeElement === first) {
          event.preventDefault();
          last.focus();
        } else if (!event.shiftKey && document.activeElement === last) {
          event.preventDefault();
          first.focus();
        }
      }, [close]);
      const parent = typeof listing?.parent === "string" ? listing.parent : null;
      const crumbs = Array.isArray(listing?.breadcrumbs) ? listing.breadcrumbs : [];
      return /* @__PURE__ */ import_react2.default.createElement(
        "div",
        {
          className: "v8r-picker",
          role: "dialog",
          "aria-modal": "true",
          "aria-label": t("picker.title"),
          "aria-busy": busy ? "true" : "false",
          tabIndex: -1,
          ref: layer,
          onKeyDown
        },
        /* @__PURE__ */ import_react2.default.createElement("div", { className: "v8r-picker-head" }, /* @__PURE__ */ import_react2.default.createElement("div", { className: "v8r-picker-title" }, t("picker.title")), /* @__PURE__ */ import_react2.default.createElement(
          "button",
          {
            type: "button",
            className: "v8r-button v8r-button--ghost",
            title: t("picker.close"),
            "aria-label": t("picker.close"),
            onClick: close
          },
          "\xD7"
        )),
        /* @__PURE__ */ import_react2.default.createElement("div", { className: "v8r-picker-crumbs" }, /* @__PURE__ */ import_react2.default.createElement(
          "button",
          {
            type: "button",
            className: "v8r-button",
            title: t("picker.up"),
            "aria-label": t("picker.up"),
            disabled: parent === null || busy,
            onClick: () => {
              load(parent, showAll, { fallback: false });
            }
          },
          "\u2191"
        ), crumbs.map((crumb) => /* @__PURE__ */ import_react2.default.createElement(
          "button",
          {
            key: crumb.path,
            type: "button",
            className: "v8r-crumb",
            "data-active": crumb.path === dir ? "true" : "false",
            title: crumb.path,
            disabled: busy,
            ref: (node) => {
              if (node !== null && crumb.path === dir) crumbRef.current = node;
            },
            onClick: () => {
              load(crumb.path, showAll, { fallback: false });
            }
          },
          crumb.name
        ))),
        /* @__PURE__ */ import_react2.default.createElement("div", { className: "v8r-picker-goto" }, /* @__PURE__ */ import_react2.default.createElement(
          "input",
          {
            className: "v8r-input v8r-picker-path",
            type: "text",
            value: pathBar,
            spellCheck: false,
            placeholder: t("picker.goto"),
            "aria-label": t("picker.goto"),
            onChange: (event) => {
              setPathBar(event.target.value);
            },
            onKeyDown: (event) => {
              if (event.key !== "Enter") return;
              event.stopPropagation();
              const value = pathBar.trim();
              if (value !== "") load(value, showAll, { fallback: false });
            }
          }
        ), /* @__PURE__ */ import_react2.default.createElement(
          "button",
          {
            type: "button",
            className: "v8r-button",
            disabled: busy || pathBar.trim() === "",
            onClick: () => {
              load(pathBar.trim(), showAll, { fallback: false });
            }
          },
          t("picker.goto.action")
        )),
        /* @__PURE__ */ import_react2.default.createElement("div", { className: "v8r-picker-controls" }, /* @__PURE__ */ import_react2.default.createElement(
          "input",
          {
            className: "v8r-input",
            type: "text",
            value: filter,
            spellCheck: false,
            ref: filterInput,
            placeholder: t("picker.filter"),
            "aria-label": t("picker.filter"),
            onChange: (event) => {
              setFilter(event.target.value);
            }
          }
        ), /* @__PURE__ */ import_react2.default.createElement("label", { className: "v8r-picker-check" }, /* @__PURE__ */ import_react2.default.createElement(
          "input",
          {
            type: "checkbox",
            checked: showAll,
            disabled: busy,
            onChange: (event) => {
              const next = event.target.checked;
              setShowAll(next);
              load(dir, next, { fallback: false });
            }
          }
        ), t("picker.allFiles"))),
        busy && /* @__PURE__ */ import_react2.default.createElement("div", { className: "v8r-loading", role: "status" }, t("picker.loading")),
        error !== null && /* @__PURE__ */ import_react2.default.createElement("div", { className: "v8r-banner", role: "alert" }, /* @__PURE__ */ import_react2.default.createElement("div", { className: "v8r-banner-title" }, t("error.title"), ": ", browseErrorText(t, error)), /* @__PURE__ */ import_react2.default.createElement("div", { className: "v8r-banner-actions" }, /* @__PURE__ */ import_react2.default.createElement(
          "button",
          {
            type: "button",
            className: "v8r-button",
            onClick: () => {
              load(lastTarget.current, showAll, { fallback: false });
            }
          },
          t("picker.retry")
        ), /* @__PURE__ */ import_react2.default.createElement("button", { type: "button", className: "v8r-button", onClick: () => {
          load("", showAll, { fallback: false });
        } }, t("picker.home")))),
        /* @__PURE__ */ import_react2.default.createElement("div", { className: "v8r-picker-list", "data-busy": busy ? "true" : "false" }, shown.length === 0 && !busy && error === null && (needle === "" ? /* @__PURE__ */ import_react2.default.createElement("p", { className: "v8r-muted" }, showAll ? t("picker.empty") : t("picker.emptyFiltered")) : /* @__PURE__ */ import_react2.default.createElement("div", { className: "v8r-picker-none" }, /* @__PURE__ */ import_react2.default.createElement("span", { className: "v8r-muted" }, t("picker.notFound", { query: filter.trim() })), /* @__PURE__ */ import_react2.default.createElement("button", { type: "button", className: "v8r-button", onClick: () => {
          setFilter("");
        } }, t("picker.resetFilter")))), shown.map((entry) => /* @__PURE__ */ import_react2.default.createElement(
          Row,
          {
            key: entry.path,
            entry,
            openable: openable(entry),
            busy,
            reason: t("picker.unsupported"),
            onOpen: openEntry
          }
        ))),
        /* @__PURE__ */ import_react2.default.createElement("div", { className: "v8r-picker-foot" }, /* @__PURE__ */ import_react2.default.createElement("span", { className: "v8r-muted" }, t("picker.hint")), typeof listing?.hiddenCount === "number" && listing.hiddenCount > 0 && /* @__PURE__ */ import_react2.default.createElement(
          "button",
          {
            type: "button",
            className: "v8r-link",
            disabled: busy,
            onClick: () => {
              setShowAll(true);
              load(dir, true, { fallback: false });
            }
          },
          t("picker.hidden", { count: listing.hiddenCount })
        ), listing?.truncated === true && /* @__PURE__ */ import_react2.default.createElement("span", { className: "v8r-muted" }, t("picker.truncated", { count: entries.length })))
      );
    }
    
    // src/client/FormPreview.jsx
    var import_react3 = __toESM(require("react"), 1);
    
    // src/client/form.js
    function frameDocumentError(doc) {
      if (doc === null || doc === void 0) return null;
      const type = String(doc.contentType ?? "");
      if (!type.includes("json")) return null;
      const text = String(doc.body?.textContent ?? "");
      try {
        const parsed = JSON.parse(text);
        if (parsed?.error !== void 0 && typeof parsed.error.message === "string") {
          return {
            code: typeof parsed.error.code === "string" ? parsed.error.code : "form-error",
            message: parsed.error.message
          };
        }
      } catch {
      }
      return { code: "form-error", message: text };
    }
    
    // src/client/FormPreview.jsx
    var POLL_MS = 250;
    var TIMEOUT_MS = 3e4;
    function FormPreview({ src, title, zoom = 100, nonce = 0, translate: t, onRetry }) {
      const frame = (0, import_react3.useRef)(null);
      const [status, setStatus] = (0, import_react3.useState)("loading");
      const [error, setError] = (0, import_react3.useState)(null);
      const applyZoom = (0, import_react3.useCallback)(() => {
        try {
          const doc = frame.current?.contentDocument;
          if (doc?.documentElement !== void 0 && doc.documentElement !== null) {
            doc.documentElement.style.zoom = String(zoom / 100);
          }
        } catch {
        }
      }, [zoom]);
      (0, import_react3.useEffect)(() => {
        setStatus("loading");
        setError(null);
        let alive = true;
        let timer;
        const startedAt = Date.now();
        let expectedUrl = src;
        try {
          expectedUrl = new URL(src, globalThis.location?.href ?? "http://localhost/").href;
        } catch {
        }
        const check = () => {
          if (!alive) return;
          if (Date.now() - startedAt > TIMEOUT_MS) {
            setStatus("error");
            setError({ code: "form-timeout", message: "" });
            return;
          }
          const element = frame.current;
          if (element === null) return;
          let doc = null;
          try {
            doc = element.contentDocument;
          } catch {
            doc = null;
          }
          if (doc === null) {
            schedule();
            return;
          }
          let currentUrl = "";
          try {
            currentUrl = doc.location?.href ?? "";
          } catch {
            currentUrl = "";
          }
          if (currentUrl !== expectedUrl) {
            schedule();
            return;
          }
          const documentError = frameDocumentError(doc);
          if (documentError !== null) {
            setStatus("error");
            setError(documentError);
            return;
          }
          const win = element.contentWindow;
          const state = win?.__QV8_STATE;
          if (state === "ready") {
            applyZoom();
            setStatus("ready");
            return;
          }
          if (state === "error") {
            setStatus("error");
            setError({
              code: "form-error",
              message: String(win?.__QV8_ERROR ?? "")
            });
            return;
          }
          schedule();
        };
        const schedule = () => {
          timer = setTimeout(check, POLL_MS);
        };
        timer = setTimeout(check, POLL_MS);
        return () => {
          alive = false;
          clearTimeout(timer);
        };
      }, [src, nonce]);
      (0, import_react3.useEffect)(() => {
        if (status === "ready") applyZoom();
      }, [applyZoom, status]);
      return /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-form", "data-status": status }, /* @__PURE__ */ import_react3.default.createElement(
        "iframe",
        {
          key: nonce,
          ref: frame,
          className: "v8r-form-frame",
          src,
          title,
          sandbox: "allow-scripts allow-same-origin",
          onLoad: () => {
            applyZoom();
          }
        }
      ), status === "loading" && /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-form-status", role: "status" }, t("form.loading")), status === "error" && /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-form-error", role: "alert" }, /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-banner" }, /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-banner-title" }, t("form.error.title")), /* @__PURE__ */ import_react3.default.createElement("div", null, t(`error.${error?.code ?? "form-error"}`)), error?.message !== void 0 && error.message !== "" && /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-banner-detail" }, error.message), typeof onRetry === "function" && /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-banner-actions" }, /* @__PURE__ */ import_react3.default.createElement("button", { type: "button", className: "v8r-button", onClick: () => {
        onRetry();
      } }, t("form.retry"))))));
    }
    
    // src/client/styles.js
    var PANEL_CSS = `
    .v8r{position:relative;display:flex;flex-direction:column;height:100%;min-height:0;container-type:inline-size;color:var(--dsw-alias-label-primary);background:var(--dsw-alias-bg-base);font-size:13px;line-height:1.45}
    .v8r-toolbar{display:flex;flex-wrap:wrap;gap:8px;align-items:center;padding:8px 12px;border-bottom:1px solid var(--dsw-alias-border-l1);flex:0 0 auto}
    .v8r-input{flex:1 1 140px;min-width:0;height:28px;padding:0 8px;border-radius:6px;border:1px solid var(--dsw-alias-border-l2);background:var(--dsw-alias-bg-layer-1);color:inherit;font:inherit;outline:none}
    .v8r-input:focus{border-color:var(--dsw-alias-brand-primary)}
    .v8r-button{flex:0 0 auto;height:28px;padding:0 12px;border-radius:6px;border:1px solid var(--dsw-alias-border-l2);background:var(--dsw-alias-bg-layer-1);color:var(--dsw-alias-label-primary);font:inherit;cursor:pointer}
    .v8r-button:hover{background:var(--dsw-alias-bg-layer-2)}
    .v8r-button:disabled{color:var(--dsw-alias-label-secondary);cursor:default}
    .v8r-button--primary{border-color:var(--dsw-alias-brand-primary);color:var(--dsw-alias-brand-primary)}
    .v8r-button--ghost{border-color:transparent;background:transparent}
    .v8r-body{display:flex;flex:1 1 auto;min-height:0}
    .v8r-tree{display:flex;flex-direction:column;flex:0 0 min(300px,45%);min-width:0;border-right:1px solid var(--dsw-alias-border-l1);min-height:0}
    .v8r-filter{padding:8px;border-bottom:1px solid var(--dsw-alias-border-l1)}
    .v8r-tree-list{flex:1 1 auto;overflow:auto;padding:4px 4px 12px}
    .v8r-category{display:block;width:100%;text-align:left;padding:5px 8px;border:0;border-radius:5px;background:transparent;color:inherit;font:inherit;cursor:pointer}
    .v8r-category:hover{background:var(--dsw-alias-bg-layer-2)}
    .v8r-category-count{color:var(--dsw-alias-label-secondary);font-size:12px}
    .v8r-node{display:flex;align-items:center;gap:6px;width:100%;text-align:left;padding:3px 6px;border:0;border-radius:5px;background:transparent;color:inherit;font:inherit;cursor:pointer;overflow:hidden;white-space:nowrap}
    .v8r-node:hover{background:var(--dsw-alias-bg-layer-2)}
    .v8r-node--root{font-weight:600;padding-top:6px}
    .v8r-node--category{font-weight:600;margin-top:2px}
    .v8r-node--object[aria-current="true"]{background:var(--dsw-alias-bg-layer-2);font-weight:600}
    .v8r-caret{flex:0 0 12px;color:var(--dsw-alias-label-secondary);font-size:10px;text-align:center}
    .v8r-node-label{min-width:0;overflow:hidden;text-overflow:ellipsis}
    .v8r-node-label--mono{font-family:ui-monospace,SFMono-Regular,Consolas,monospace;font-size:12.5px}
    .v8r-node-synonym{color:var(--dsw-alias-label-secondary);font-size:12px;font-weight:400;overflow:hidden;text-overflow:ellipsis}
    .v8r-node-count{flex:0 0 auto;color:var(--dsw-alias-label-secondary);font-size:11.5px;margin-left:auto;padding-left:6px}
    .v8r-children{margin-left:10px;padding-left:10px;border-left:1px solid var(--dsw-alias-border-l1)}
    @container (max-width: 520px){.v8r-body{flex-direction:column}.v8r-tree{flex:0 0 auto;width:100%;max-height:42%;border-right:0;border-bottom:1px solid var(--dsw-alias-border-l1)}.v8r-main{flex:1 1 auto;min-height:0}}
    .v8r-node--group{font-size:12.5px;font-weight:600}
    .v8r-node--group[data-active="true"]{background:var(--dsw-alias-bg-layer-2)}
    .v8r-leaf{display:flex;align-items:baseline;gap:6px;width:100%;text-align:left;padding:2px 6px 2px 0;border:0;border-radius:5px;background:transparent;color:inherit;font:inherit;font-size:12.5px;cursor:pointer;white-space:nowrap;overflow:hidden}
    .v8r-leaf:hover{background:var(--dsw-alias-bg-layer-2)}
    .v8r-leaf[data-active="true"]{background:var(--dsw-alias-bg-layer-2);font-weight:600}
    .v8r-leaf--empty{cursor:default;padding-left:18px;font-size:12px}
    .v8r-leaf--empty:hover{background:transparent}
    .v8r-leaf-name{min-width:0;overflow:hidden;text-overflow:ellipsis}
    .v8r-leaf-type{flex:0 0 auto;margin-left:auto;padding-left:8px;max-width:48%;color:var(--dsw-alias-label-secondary);font-size:11.5px;overflow:hidden;text-overflow:ellipsis}
    .v8r-tree-empty{padding:8px 10px}
    .v8r-more{display:block;width:100%;text-align:left;padding:4px 8px 4px 20px;border:0;background:transparent;color:var(--dsw-alias-brand-primary);font:inherit;cursor:pointer}
    .v8r-main{flex:1 1 auto;min-width:0;overflow:auto;padding:12px 16px 24px}
    .v8r-title{display:flex;flex-wrap:wrap;gap:8px;align-items:baseline;margin:0 0 2px;font-size:16px;font-weight:600}
    .v8r-subtitle{color:var(--dsw-alias-label-secondary);font-size:12px;margin-bottom:10px}
    .v8r-tabs{display:flex;flex-wrap:wrap;gap:4px;margin:10px 0 12px;border-bottom:1px solid var(--dsw-alias-border-l1);padding-bottom:8px}
    .v8r-tab{height:26px;padding:0 10px;border-radius:5px;border:1px solid transparent;background:transparent;color:var(--dsw-alias-label-secondary);font:inherit;cursor:pointer}
    .v8r-tab:hover{background:var(--dsw-alias-bg-layer-2)}
    .v8r-tab[aria-selected="true"]{background:var(--dsw-alias-bg-layer-2);color:var(--dsw-alias-label-primary);border-color:var(--dsw-alias-border-l1)}
    .v8r-table{width:100%;border-collapse:collapse;font-size:12.5px}
    .v8r-table th{position:sticky;top:0;text-align:left;font-weight:600;color:var(--dsw-alias-label-secondary);background:var(--dsw-alias-bg-base);border-bottom:1px solid var(--dsw-alias-border-l1);padding:5px 8px}
    .v8r-table td{border-bottom:1px solid var(--dsw-alias-border-l1);padding:4px 8px;vertical-align:top}
    .v8r-table tr:hover td{background:var(--dsw-alias-bg-layer-2)}
    .v8r-name{font-family:ui-monospace,SFMono-Regular,Consolas,monospace;white-space:nowrap}
    .v8r-muted{color:var(--dsw-alias-label-secondary)}
    .v8r-modules{display:flex;flex-wrap:wrap;gap:6px;align-items:center;margin:12px 0 8px}
    .v8r-viewhead{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin:0 0 10px}
    .v8r-viewhead-text{min-width:0;flex:1 1 160px}
    .v8r-viewhead-title{margin:0;font-size:15px;font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
    .v8r-viewhead-sub{color:var(--dsw-alias-label-secondary);font-size:12px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
    .v8r-zoom{display:flex;align-items:center;gap:4px;margin-left:auto;font-size:12px;color:var(--dsw-alias-label-secondary);white-space:nowrap}
    .v8r-select{height:26px;padding:0 6px;border-radius:6px;border:1px solid var(--dsw-alias-border-l2);background:var(--dsw-alias-bg-layer-1);color:var(--dsw-alias-label-primary);font:inherit;font-size:12px}
    .v8r-main[data-view="module"],.v8r-main[data-view="form"]{display:flex;flex-direction:column;overflow:hidden}
    .v8r-codeview{display:flex;flex-direction:column;flex:1 1 auto;min-height:0;border:1px solid var(--dsw-alias-border-l1);border-radius:8px;overflow:hidden;background:var(--dsw-alias-bg-layer-1)}
    .v8r-codeview-bar{display:flex;flex-wrap:wrap;gap:4px;align-items:center;flex:0 0 auto;padding:5px 6px;border-bottom:1px solid var(--dsw-alias-border-l1)}
    .v8r-codeview-bar .v8r-button{height:24px;padding:0 8px;font-size:12px}
    .v8r-codeview-size{display:inline-flex;align-items:center;gap:2px}
    .v8r-codeview-size-value{min-width:30px;text-align:center;font-size:12px;color:var(--dsw-alias-label-secondary)}
    .v8r-codeview-spacer{flex:1 1 auto}
    .v8r-code{flex:1 1 auto;min-height:0;overflow:auto;padding:6px 0;font-family:ui-monospace,SFMono-Regular,Consolas,monospace;font-size:var(--v8r-code-size,12.5px);line-height:1.45;tab-size:4}
    .v8r-code:focus-visible{outline:2px solid var(--dsw-alias-brand-primary);outline-offset:-2px}
    .v8r-code-row{display:flex;align-items:flex-start;min-height:1.45em}
    .v8r-code-num{flex:0 0 var(--v8r-gutter,3ch);position:sticky;left:0;z-index:1;padding:0 8px 0 6px;text-align:right;color:var(--dsw-alias-label-secondary);background:var(--dsw-alias-bg-layer-1);font-size:.92em;user-select:none}
    .v8r-code-text{flex:1 1 auto;min-width:max-content;padding-right:12px;white-space:pre}
    .v8r-codeview[data-wrap="true"] .v8r-code-text{min-width:0;white-space:pre-wrap;word-break:break-word}
    .v8r-codeview[data-wrap="true"] .v8r-code-num{position:static}
    .v8r-code-more{padding:8px 12px}
    .v8r-tok-keyword{font-weight:600}
    .v8r-tok-directive{font-weight:600}
    .v8r-tok-comment{font-style:italic}
    .v8r-codeview[data-scheme="light"] .v8r-tok-keyword{color:#0000a0}
    .v8r-codeview[data-scheme="light"] .v8r-tok-directive{color:#800080}
    .v8r-codeview[data-scheme="light"] .v8r-tok-annotation{color:#008080}
    .v8r-codeview[data-scheme="light"] .v8r-tok-number{color:#993333}
    .v8r-codeview[data-scheme="light"] .v8r-tok-string{color:#a31515}
    .v8r-codeview[data-scheme="light"] .v8r-tok-comment{color:#008000}
    .v8r-codeview[data-scheme="dark"] .v8r-tok-keyword{color:#569cd6}
    .v8r-codeview[data-scheme="dark"] .v8r-tok-directive{color:#c586c0}
    .v8r-codeview[data-scheme="dark"] .v8r-tok-annotation{color:#4ec9b0}
    .v8r-codeview[data-scheme="dark"] .v8r-tok-number{color:#b5cea8}
    .v8r-codeview[data-scheme="dark"] .v8r-tok-string{color:#ce9178}
    .v8r-codeview[data-scheme="dark"] .v8r-tok-comment{color:#6a9955}
    .v8r-form{position:relative;flex:1 1 auto;min-height:280px;border:1px solid var(--dsw-alias-border-l1);border-radius:8px;overflow:hidden;background:var(--dsw-alias-bg-layer-1)}
    .v8r-form-frame{display:block;width:100%;height:100%;border:0;background:var(--dsw-alias-bg-base)}
    .v8r-form[data-status="error"] .v8r-form-frame{visibility:hidden}
    .v8r-form-status{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;color:var(--dsw-alias-label-secondary);font-size:12.5px;pointer-events:none}
    .v8r-form-error{position:absolute;inset:0;padding:12px;overflow:auto;background:var(--dsw-alias-bg-base)}
    .v8r-form-error .v8r-banner{margin:0}
    .v8r-empty{display:flex;flex-direction:column;gap:10px;align-items:flex-start;color:var(--dsw-alias-label-secondary);padding:16px 4px;max-width:520px}
    .v8r-empty-title{color:var(--dsw-alias-label-primary);font-weight:600}
    .v8r-picker{position:absolute;inset:0;z-index:10;display:flex;flex-direction:column;min-height:0;background:var(--dsw-alias-bg-base);border:1px solid var(--dsw-alias-border-l1);outline:none}
    .v8r-picker-head{display:flex;align-items:center;gap:8px;flex:0 0 auto;padding:8px 12px;border-bottom:1px solid var(--dsw-alias-border-l1)}
    .v8r-picker-title{flex:1 1 auto;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-weight:600}
    .v8r-picker-crumbs{display:flex;align-items:center;gap:4px;flex:0 0 auto;padding:6px 12px;border-bottom:1px solid var(--dsw-alias-border-l1);overflow-x:auto;white-space:nowrap}
    .v8r-crumb{flex:0 0 auto;max-width:180px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;height:24px;padding:0 6px;border:0;border-radius:5px;background:transparent;color:var(--dsw-alias-brand-primary);font:inherit;font-size:12.5px;cursor:pointer}
    .v8r-crumb:hover{background:var(--dsw-alias-bg-layer-2)}
    .v8r-crumb[data-active="true"]{color:var(--dsw-alias-label-primary);font-weight:600}
    .v8r-crumb:disabled{cursor:default;color:var(--dsw-alias-label-secondary)}
    .v8r-picker-controls{display:flex;gap:8px;align-items:center;flex:0 0 auto;padding:8px 12px;border-bottom:1px solid var(--dsw-alias-border-l1)}
    .v8r-picker-check{display:flex;gap:6px;align-items:center;flex:0 0 auto;white-space:nowrap;color:var(--dsw-alias-label-secondary);font-size:12.5px;cursor:pointer}
    .v8r-picker-goto{display:flex;gap:8px;align-items:center;flex:0 0 auto;padding:6px 12px;border-bottom:1px solid var(--dsw-alias-border-l1)}
    .v8r-picker-path{flex:1 1 auto;font-family:ui-monospace,SFMono-Regular,Consolas,monospace;font-size:12px}
    .v8r-picker-list{flex:1 1 auto;min-height:0;overflow:auto;padding:4px 6px}
    .v8r-picker-list[data-busy="true"]{opacity:.55}
    .v8r-picker-row{display:flex;align-items:center;gap:8px;width:100%;text-align:left;padding:4px 8px;border:0;border-radius:5px;background:transparent;color:inherit;font:inherit;cursor:pointer}
    .v8r-picker-row:hover{background:var(--dsw-alias-bg-layer-2)}
    .v8r-picker-row[data-kind="file"] .v8r-picker-name{font-family:ui-monospace,SFMono-Regular,Consolas,monospace}
    .v8r-picker-row[data-openable="false"]{cursor:default;color:var(--dsw-alias-label-secondary)}
    .v8r-picker-row[data-openable="false"]:hover{background:transparent}
    .v8r-picker-row:disabled{cursor:default}
    .v8r-picker-mark{flex:0 0 12px;color:var(--dsw-alias-label-secondary);font-size:11px}
    .v8r-picker-name{flex:1 1 auto;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
    .v8r-picker-size{flex:0 0 auto;color:var(--dsw-alias-label-secondary);font-size:12px}
    .v8r-picker-none{display:flex;flex-direction:column;gap:8px;align-items:flex-start;padding:8px}
    .v8r-picker-foot{display:flex;gap:12px;align-items:center;flex-wrap:wrap;flex:0 0 auto;padding:8px 12px;border-top:1px solid var(--dsw-alias-border-l1);font-size:12px}
    .v8r-link{padding:0;border:0;background:transparent;color:var(--dsw-alias-brand-primary);font:inherit;font-size:12px;cursor:pointer;text-decoration:underline}
    .v8r-banner-actions{display:flex;gap:8px;margin-top:8px}
    .v8r-button:focus-visible,.v8r-crumb:focus-visible,.v8r-picker-row:focus-visible,.v8r-link:focus-visible,.v8r-node:focus-visible,.v8r-leaf:focus-visible,.v8r-select:focus-visible{outline:2px solid var(--dsw-alias-brand-primary);outline-offset:1px}
    .v8r-banner{margin:8px 12px;padding:8px 10px;border-radius:8px;border:1px solid var(--dsw-alias-state-error-primary);color:var(--dsw-alias-state-error-primary);background:var(--dsw-alias-bg-layer-1);font-size:12.5px;flex:0 0 auto}
    .v8r-banner-title{font-weight:600;margin-bottom:2px}
    .v8r-banner-detail{margin-top:6px;white-space:pre-wrap;word-break:break-word;font-family:ui-monospace,SFMono-Regular,Consolas,monospace;font-size:11.5px;color:var(--dsw-alias-label-secondary)}
    .v8r-loading{padding:8px 12px;color:var(--dsw-alias-label-secondary);font-size:12.5px;flex:0 0 auto}
    .v8r-children{padding-left:14px}
    `;
    
    // src/client/tree.js
    var MODULES_GROUP = "modules";
    function objectKey(object) {
      if (object === null || typeof object !== "object") return "";
      const guid = typeof object.guid === "string" ? object.guid.trim() : "";
      if (guid !== "") return guid;
      return String(object.name ?? "");
    }
    function sameObject(object, selected) {
      if (object === null || object === void 0) return false;
      if (selected === null || selected === void 0) return false;
      if (typeof selected.guid === "string" && selected.guid !== "") {
        return object.guid === selected.guid;
      }
      return object.name === selected.name;
    }
    function matchesQuery(entry, query) {
      if (query === "") return true;
      const name = String(entry?.name ?? "").toLowerCase();
      const synonym = String(entry?.synonym ?? "").toLowerCase();
      return name.includes(query) || synonym.includes(query);
    }
    function filterCategories(categories, query) {
      const source = Array.isArray(categories) ? categories : [];
      const groups = source.map((category) => {
        const objects = Array.isArray(category.objects) ? category.objects : [];
        const items = query === "" ? objects : objects.filter((object) => matchesQuery(object, query));
        return { category, items, total: items.length };
      });
      if (query === "") return groups;
      return groups.filter((group) => group.total > 0);
    }
    function objectGroups(object) {
      if (object === null || typeof object !== "object") return [];
      const groups = [];
      const sections = Array.isArray(object.sections) ? object.sections : [];
      for (const section of sections) {
        const kind = String(section.kind ?? "");
        if (kind === "") continue;
        const token = kind === "forms" ? "form" : kind === "templates" ? "template" : "entry";
        const entries = (Array.isArray(section.entries) ? section.entries : []).map((entry) => ({
          ...entry,
          token
        }));
        groups.push({
          kind,
          title: String(section.title ?? kind),
          entries
        });
      }
      const modules = Array.isArray(object.modules) ? object.modules : [];
      if (modules.length > 0) {
        groups.push({
          kind: MODULES_GROUP,
          title: "",
          entries: modules.map((module2) => ({
            name: String(module2.name ?? ""),
            synonym: "",
            typeText: "",
            token: "module"
          }))
        });
      }
      return groups;
    }
    function filterEntries(entries, query) {
      if (query === "") return entries;
      const result = [];
      for (const entry of entries) {
        if (matchesQuery(entry, query)) {
          result.push(entry);
          continue;
        }
        const children = Array.isArray(entry.children) ? filterEntries(entry.children, query) : [];
        if (children.length > 0) result.push({ ...entry, children });
      }
      return result;
    }
    function filterObjectGroups(groups, query) {
      const source = Array.isArray(groups) ? groups : [];
      const result = [];
      for (const group of source) {
        const matched = filterEntries(group.entries, query);
        if (query !== "" && matched.length === 0) continue;
        const rows = flattenEntries(matched);
        result.push({ ...group, entries: matched, rows, total: rows.length });
      }
      return result;
    }
    function countObjects(categories) {
      const source = Array.isArray(categories) ? categories : [];
      return source.reduce((total, category) => total + (Array.isArray(category.objects) ? category.objects.length : 0), 0);
    }
    
    // src/client/Tree.jsx
    var import_react4 = __toESM(require("react"), 1);
    var CATEGORY_PAGE = 200;
    var GROUP_PAGE = 100;
    function categoryKey(category) {
      const guid = typeof category.guid === "string" && category.guid !== "" ? category.guid : "";
      return `c:${guid !== "" ? guid : String(category.title ?? "")}`;
    }
    function objectNodeKey(object) {
      return `o:${objectKey(object)}`;
    }
    function groupNodeKey(object, kind) {
      return `g:${objectKey(object)}:${kind}`;
    }
    function Tree({
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
      const [filter, setFilter] = (0, import_react4.useState)("");
      const [expanded, setExpanded] = (0, import_react4.useState)(() => {
        const first = Array.isArray(categories) && categories.length > 0 ? categories[0] : void 0;
        return new Set(first === void 0 ? [] : [categoryKey(first)]);
      });
      const [rootOpen, setRootOpen] = (0, import_react4.useState)(true);
      const [limits, setLimits] = (0, import_react4.useState)({});
      const query = filter.trim().toLowerCase();
      const groups = (0, import_react4.useMemo)(() => filterCategories(categories, query), [categories, query]);
      const total = (0, import_react4.useMemo)(() => countObjects(categories), [categories]);
      const matched = (0, import_react4.useMemo)(
        () => groups.reduce((sum, group) => sum + group.total, 0),
        [groups]
      );
      const selectedKey = selected === null || selected === void 0 ? "" : objectKey(selected);
      const detailKey = detail?.object === void 0 || detail?.object === null ? "" : objectKey(detail.object);
      const detailMatches = selectedKey !== "" && detailKey === selectedKey;
      const branches = (0, import_react4.useMemo)(
        () => detailMatches ? filterObjectGroups(objectGroups(detail.object), query) : [],
        [detailMatches, detail, query]
      );
      (0, import_react4.useEffect)(() => {
        if (selectedKey === "") return;
        const holder = (Array.isArray(categories) ? categories : []).find((category) => (Array.isArray(category.objects) ? category.objects : []).some((object) => sameObject(object, selected)));
        setExpanded((current) => {
          const next = new Set(current);
          if (holder !== void 0) next.add(categoryKey(holder));
          next.add(`o:${selectedKey}`);
          return next;
        });
      }, [categories, selected, selectedKey]);
      const toggle = (key) => {
        setExpanded((current) => {
          const next = new Set(current);
          if (next.has(key)) next.delete(key);
          else next.add(key);
          return next;
        });
      };
      const more = (key, currentLimit, step) => {
        setLimits((current) => ({ ...current, [key]: currentLimit + step }));
      };
      const openObjectNode = (object) => {
        const key = objectNodeKey(object);
        if (sameObject(object, selected)) {
          toggle(key);
          return;
        }
        setExpanded((current) => new Set(current).add(key));
        onSelect(object);
      };
      const openGroup = (group) => {
        toggle(groupNodeKey(detail?.object, group.kind));
        if (group.kind !== MODULES_GROUP) onSection(group.kind);
      };
      const leafAction = (group, entry) => {
        if (entry.token === "form") onOpenForm(entry.name);
        else if (entry.token === "module") onOpenModule(entry.name);
        else if (group.kind !== MODULES_GROUP) onSection(group.kind);
      };
      const leafActive = (group, entry) => {
        if (entry.token === "form") return viewMode === "form" && entry.name === formName;
        if (entry.token === "module") return viewMode === "module" && entry.name === moduleName;
        return false;
      };
      const groupTitle = (kind, fallback) => {
        if (kind === MODULES_GROUP) return t("panel.modules");
        return sectionLabel(t, { kind, title: fallback });
      };
      return /* @__PURE__ */ import_react4.default.createElement("div", { className: "v8r-tree" }, /* @__PURE__ */ import_react4.default.createElement("div", { className: "v8r-filter" }, /* @__PURE__ */ import_react4.default.createElement(
        "input",
        {
          className: "v8r-input",
          type: "search",
          value: filter,
          spellCheck: false,
          placeholder: t("panel.filter.placeholder"),
          "aria-label": t("panel.filter.placeholder"),
          onChange: (event) => {
            setFilter(event.target.value);
          }
        }
      )), /* @__PURE__ */ import_react4.default.createElement("div", { className: "v8r-tree-list" }, /* @__PURE__ */ import_react4.default.createElement(
        "button",
        {
          type: "button",
          className: "v8r-node v8r-node--root",
          "aria-expanded": rootOpen,
          onClick: () => {
            setRootOpen((current) => !current);
          }
        },
        /* @__PURE__ */ import_react4.default.createElement("span", { className: "v8r-caret", "aria-hidden": "true" }, rootOpen ? "\u25BE" : "\u25B8"),
        /* @__PURE__ */ import_react4.default.createElement("span", { className: "v8r-node-label" }, t("tree.root")),
        file?.name !== void 0 && file.name !== "" && /* @__PURE__ */ import_react4.default.createElement("span", { className: "v8r-node-synonym" }, ` \xAB${file.name}\xBB`),
        /* @__PURE__ */ import_react4.default.createElement("span", { className: "v8r-node-count" }, query === "" ? `(${total})` : `(${matched})`)
      ), rootOpen && groups.length === 0 && /* @__PURE__ */ import_react4.default.createElement("p", { className: "v8r-muted v8r-tree-empty" }, t("panel.noObjects")), rootOpen && groups.map(({ category, items, total: count }) => {
        const key = categoryKey(category);
        const open = query !== "" ? count > 0 : expanded.has(key);
        const limit = limits[key] ?? CATEGORY_PAGE;
        return /* @__PURE__ */ import_react4.default.createElement("div", { key }, /* @__PURE__ */ import_react4.default.createElement(
          "button",
          {
            type: "button",
            className: "v8r-node v8r-node--category",
            "aria-expanded": open,
            onClick: () => {
              toggle(key);
            }
          },
          /* @__PURE__ */ import_react4.default.createElement("span", { className: "v8r-caret", "aria-hidden": "true" }, open ? "\u25BE" : "\u25B8"),
          /* @__PURE__ */ import_react4.default.createElement("span", { className: "v8r-node-label" }, category.title),
          /* @__PURE__ */ import_react4.default.createElement("span", { className: "v8r-node-count" }, `(${count})`)
        ), open && items.slice(0, limit).map((object) => {
          const nodeKey = objectNodeKey(object);
          const isOpen = expanded.has(nodeKey);
          const selectedHere = sameObject(object, selected);
          return /* @__PURE__ */ import_react4.default.createElement("div", { key: `${nodeKey}:${object.name}` }, /* @__PURE__ */ import_react4.default.createElement(
            "button",
            {
              type: "button",
              className: "v8r-node v8r-node--object",
              "aria-current": selectedHere ? "true" : "false",
              "aria-expanded": isOpen,
              title: object.synonym === void 0 || object.synonym === "" ? object.name : `${object.name} \u2014 ${object.synonym}`,
              onClick: () => {
                openObjectNode(object);
              }
            },
            /* @__PURE__ */ import_react4.default.createElement("span", { className: "v8r-caret", "aria-hidden": "true" }, isOpen ? "\u25BE" : "\u25B8"),
            /* @__PURE__ */ import_react4.default.createElement("span", { className: "v8r-node-label v8r-node-label--mono" }, object.name),
            object.synonym !== void 0 && object.synonym !== "" && /* @__PURE__ */ import_react4.default.createElement("span", { className: "v8r-node-synonym" }, ` \u2014 ${object.synonym}`)
          ), isOpen && selectedHere && busy && !detailMatches && /* @__PURE__ */ import_react4.default.createElement("div", { className: "v8r-children v8r-muted" }, t("tree.loading")), isOpen && selectedHere && detailMatches && branches.map((branch) => {
            const gkey = groupNodeKey(detail.object, branch.kind);
            const groupOpen = query !== "" ? branch.total > 0 : expanded.has(gkey);
            const groupLimit = limits[gkey] ?? GROUP_PAGE;
            return /* @__PURE__ */ import_react4.default.createElement("div", { key: gkey, className: "v8r-children" }, /* @__PURE__ */ import_react4.default.createElement(
              "button",
              {
                type: "button",
                className: "v8r-node v8r-node--group",
                "aria-expanded": groupOpen,
                "data-active": viewMode === "object" && activeSection === branch.kind ? "true" : "false",
                onClick: () => {
                  openGroup(branch);
                }
              },
              /* @__PURE__ */ import_react4.default.createElement("span", { className: "v8r-caret", "aria-hidden": "true" }, groupOpen ? "\u25BE" : "\u25B8"),
              /* @__PURE__ */ import_react4.default.createElement("span", { className: "v8r-node-label" }, groupTitle(branch.kind, branch.title)),
              /* @__PURE__ */ import_react4.default.createElement("span", { className: "v8r-node-count" }, `(${branch.total})`)
            ), groupOpen && branch.total === 0 && /* @__PURE__ */ import_react4.default.createElement("div", { className: "v8r-leaf v8r-leaf--empty v8r-muted" }, t("panel.sectionEmpty")), groupOpen && branch.rows.slice(0, groupLimit).map(({ entry, depth }, index) => /* @__PURE__ */ import_react4.default.createElement(
              "button",
              {
                key: `${entry.name}:${index}`,
                type: "button",
                className: "v8r-leaf",
                "data-token": entry.token,
                "data-active": leafActive(branch, entry) ? "true" : "false",
                style: { paddingLeft: `${18 + depth * 12}px` },
                title: entry.synonym === void 0 || entry.synonym === "" ? entry.name : `${entry.name} \u2014 ${entry.synonym}`,
                onClick: () => {
                  leafAction(branch, entry);
                }
              },
              /* @__PURE__ */ import_react4.default.createElement("span", { className: "v8r-leaf-name" }, entry.name),
              entry.typeText !== void 0 && entry.typeText !== "" && /* @__PURE__ */ import_react4.default.createElement("span", { className: "v8r-leaf-type" }, entry.typeText)
            )), groupOpen && branch.total > groupLimit && /* @__PURE__ */ import_react4.default.createElement(
              "button",
              {
                type: "button",
                className: "v8r-more",
                onClick: () => {
                  more(gkey, groupLimit, GROUP_PAGE);
                }
              },
              t("panel.showMore"),
              " (",
              branch.total - groupLimit,
              ")"
            ));
          }));
        }), open && items.length > limit && /* @__PURE__ */ import_react4.default.createElement(
          "button",
          {
            type: "button",
            className: "v8r-more",
            onClick: () => {
              more(key, limit, CATEGORY_PAGE);
            }
          },
          t("panel.showMore"),
          " (",
          items.length - limit,
          ")"
        ));
      })));
    }
    
    // src/client/PanelView.jsx
    var LAST_PATH_KEY = "dsh.v8reader.path.v1";
    var LAST_DIR_KEY = "dsh.v8reader.dir.v1";
    var FORM_SETTINGS_KEY = "dsh.v8reader.form.v1";
    var ZOOM_STEPS = Object.freeze([50, 75, 100, 125, 150]);
    var DEFAULT_ZOOM = 100;
    function readStored(key) {
      try {
        return globalThis.localStorage?.getItem(key) ?? "";
      } catch {
        return "";
      }
    }
    function writeStored(key, value) {
      try {
        globalThis.localStorage?.setItem(key, value);
      } catch {
      }
    }
    function readLastPath() {
      return readStored(LAST_PATH_KEY);
    }
    function readZoom() {
      const raw = readStored(FORM_SETTINGS_KEY);
      if (raw === "") return DEFAULT_ZOOM;
      try {
        const parsed = JSON.parse(raw);
        const zoom = Number(parsed?.zoom);
        if (!Number.isFinite(zoom) || !ZOOM_STEPS.includes(zoom)) return DEFAULT_ZOOM;
        return zoom;
      } catch {
        return DEFAULT_ZOOM;
      }
    }
    function writeZoom(zoom) {
      writeStored(FORM_SETTINGS_KEY, JSON.stringify({ zoom }));
    }
    function ModuleView({
      object,
      modules,
      moduleName,
      text,
      loaded,
      busy,
      scheme,
      translate: t,
      onBack,
      onOpenModule
    }) {
      return /* @__PURE__ */ import_react5.default.createElement(import_react5.default.Fragment, null, /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-viewhead" }, /* @__PURE__ */ import_react5.default.createElement("button", { type: "button", className: "v8r-button", onClick: onBack }, `\u2190 ${t("panel.back")}`), /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-viewhead-text" }, /* @__PURE__ */ import_react5.default.createElement("h3", { className: "v8r-viewhead-title" }, moduleName), /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-viewhead-sub" }, object.name))), /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-modules" }, /* @__PURE__ */ import_react5.default.createElement("span", { className: "v8r-muted" }, t("panel.modules"), ":"), modules.map((module2) => /* @__PURE__ */ import_react5.default.createElement(
        "button",
        {
          key: module2.name,
          type: "button",
          className: "v8r-button",
          "aria-pressed": module2.name === moduleName ? "true" : "false",
          disabled: busy,
          onClick: () => {
            onOpenModule(module2.name);
          }
        },
        module2.name
      ))), !loaded && !busy && /* @__PURE__ */ import_react5.default.createElement("p", { className: "v8r-muted" }, t("panel.objectUnavailable")), loaded && text === "" && !busy && /* @__PURE__ */ import_react5.default.createElement("p", { className: "v8r-muted" }, t("panel.module.empty")), loaded && text !== "" && /* @__PURE__ */ import_react5.default.createElement(CodeView, { text, scheme, translate: t }));
    }
    function FormView({
      object,
      formName,
      src,
      zoom,
      nonce,
      translate: t,
      onBack,
      onZoom,
      onReload
    }) {
      return /* @__PURE__ */ import_react5.default.createElement(import_react5.default.Fragment, null, /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-viewhead" }, /* @__PURE__ */ import_react5.default.createElement("button", { type: "button", className: "v8r-button", onClick: onBack }, `\u2190 ${t("panel.back")}`), /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-viewhead-text" }, /* @__PURE__ */ import_react5.default.createElement("h3", { className: "v8r-viewhead-title" }, formName), /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-viewhead-sub" }, object.name)), /* @__PURE__ */ import_react5.default.createElement("span", { className: "v8r-zoom" }, /* @__PURE__ */ import_react5.default.createElement("label", { htmlFor: "v8r-form-zoom" }, t("form.zoom")), /* @__PURE__ */ import_react5.default.createElement(
        "select",
        {
          id: "v8r-form-zoom",
          className: "v8r-select",
          value: String(zoom),
          onChange: (event) => {
            onZoom(Number(event.target.value));
          }
        },
        ZOOM_STEPS.map((step) => /* @__PURE__ */ import_react5.default.createElement("option", { key: step, value: String(step) }, `${step}%`))
      ), /* @__PURE__ */ import_react5.default.createElement("button", { type: "button", className: "v8r-button", onClick: onReload }, t("panel.reload")))), /* @__PURE__ */ import_react5.default.createElement(
        FormPreview,
        {
          key: `${src}#${nonce}`,
          src,
          title: formName,
          zoom,
          nonce,
          translate: t,
          onRetry: onReload
        }
      ));
    }
    function SectionTable({ section, translate: t, onOpenForm }) {
      const rows = flattenEntries(Array.isArray(section.entries) ? section.entries : []);
      if (rows.length === 0) return /* @__PURE__ */ import_react5.default.createElement("p", { className: "v8r-muted" }, t("panel.sectionEmpty"));
      const forms = section.kind === "forms" && typeof onOpenForm === "function";
      return /* @__PURE__ */ import_react5.default.createElement("table", { className: "v8r-table" }, /* @__PURE__ */ import_react5.default.createElement("thead", null, /* @__PURE__ */ import_react5.default.createElement("tr", null, /* @__PURE__ */ import_react5.default.createElement("th", null, t("panel.column.name")), /* @__PURE__ */ import_react5.default.createElement("th", null, t("panel.column.synonym")), /* @__PURE__ */ import_react5.default.createElement("th", null, t("panel.column.type")), forms && /* @__PURE__ */ import_react5.default.createElement("th", null, t("panel.openForm")))), /* @__PURE__ */ import_react5.default.createElement("tbody", null, rows.map(({ entry, depth }, index) => /* @__PURE__ */ import_react5.default.createElement("tr", { key: `${entry.name}:${index}` }, /* @__PURE__ */ import_react5.default.createElement("td", null, /* @__PURE__ */ import_react5.default.createElement("span", { style: { paddingLeft: `${depth * 14}px` }, className: "v8r-name" }, entry.name), entry.comment !== void 0 && entry.comment !== "" && /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-muted" }, entry.comment)), /* @__PURE__ */ import_react5.default.createElement("td", null, entry.synonym), /* @__PURE__ */ import_react5.default.createElement("td", { className: "v8r-muted" }, entry.typeText), forms && /* @__PURE__ */ import_react5.default.createElement("td", null, /* @__PURE__ */ import_react5.default.createElement(
        "button",
        {
          type: "button",
          className: "v8r-button",
          title: t("panel.openForm"),
          onClick: () => {
            onOpenForm(entry.name);
          }
        },
        t("panel.openForm")
      ))))));
    }
    function ErrorBanner({ error, translate: t }) {
      const code = typeof error?.code === "string" ? error.code : "unknown";
      const key = `error.${code}`;
      const localized = t(key, error?.detail ?? {});
      const title = localized === key ? t("error.unknown") : localized;
      const hint = code === "cli-missing" || code === "cli-old" ? t("error.hint.cli-missing") : "";
      const searched = error?.detail?.searched;
      return /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-banner", role: "alert" }, /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-banner-title" }, t("error.title"), ": ", title), hint !== "" && /* @__PURE__ */ import_react5.default.createElement("div", null, hint), Array.isArray(searched) && searched.length > 0 && /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-banner-detail" }, searched.join("\n")), typeof error?.message === "string" && error.message !== "" && /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-banner-detail" }, t("error.details"), ": ", error.message));
    }
    function PanelView({ useTabInfo, t: provided, scheme = "light" }) {
      const t = typeof provided === "function" ? provided : (key) => key;
      const tab = typeof useTabInfo === "function" ? useTabInfo().tab : void 0;
      const [pathInput, setPathInput] = (0, import_react5.useState)(readLastPath);
      const [lastDir, setLastDir] = (0, import_react5.useState)(() => readStored(LAST_DIR_KEY));
      const [pickerOpen, setPickerOpen] = (0, import_react5.useState)(false);
      const [document2, setDocument] = (0, import_react5.useState)(null);
      const [detail, setDetail] = (0, import_react5.useState)(null);
      const [selected, setSelected] = (0, import_react5.useState)(null);
      const [viewMode, setViewMode] = (0, import_react5.useState)("object");
      const [moduleName, setModuleName] = (0, import_react5.useState)("");
      const [loadedModule, setLoadedModule] = (0, import_react5.useState)("");
      const [formName, setFormName] = (0, import_react5.useState)("");
      const [formNonce, setFormNonce] = (0, import_react5.useState)(0);
      const [zoom, setZoom] = (0, import_react5.useState)(readZoom);
      const [activeSection, setActiveSection] = (0, import_react5.useState)("");
      const [busy, setBusy] = (0, import_react5.useState)(false);
      const [error, setError] = (0, import_react5.useState)(null);
      const controller = (0, import_react5.useRef)(null);
      const browseButton = (0, import_react5.useRef)(null);
      const run = (0, import_react5.useCallback)(async (params, onDone, noStore = false) => {
        controller.current?.abort();
        const next = new AbortController();
        controller.current = next;
        setBusy(true);
        setError(null);
        try {
          const body = await fetchState({ ...params, signal: next.signal, noStore });
          if (next.signal.aborted) return;
          onDone(body);
        } catch (failure) {
          if (next.signal.aborted || failure?.name === "AbortError") return;
          setError(failure instanceof StateError ? failure : new StateError("unknown", String(failure?.message ?? failure), 0));
        } finally {
          if (!next.signal.aborted) setBusy(false);
        }
      }, []);
      const openPath = (0, import_react5.useCallback)((raw) => {
        const value = String(raw ?? "").trim();
        if (value === "") return;
        setPathInput(value);
        run({ path: value }, (body) => {
          setDocument(body);
          setDetail(null);
          setSelected(null);
          setModuleName("");
          setLoadedModule("");
          setFormName("");
          setViewMode("object");
          setActiveSection("");
          writeStored(LAST_PATH_KEY, value);
          const dir = dirOf(value);
          if (dir !== "") {
            setLastDir(dir);
            writeStored(LAST_DIR_KEY, dir);
          }
        });
      }, [run]);
      const openFromToolbar = (0, import_react5.useCallback)(() => {
        if (String(pathInput ?? "").trim() === "") {
          setPickerOpen(true);
          return;
        }
        openPath(pathInput);
      }, [openPath, pathInput]);
      const openChooser = (0, import_react5.useCallback)(() => {
        setPickerOpen(true);
      }, []);
      const closeChooser = (0, import_react5.useCallback)((finalDir = "") => {
        setPickerOpen(false);
        if (finalDir !== "") {
          setLastDir(finalDir);
          writeStored(LAST_DIR_KEY, finalDir);
        }
        browseButton.current?.focus?.();
      }, []);
      const pickFromChooser = (0, import_react5.useCallback)((picked) => {
        setPickerOpen(false);
        openPath(picked);
      }, [openPath]);
      const openObject = (0, import_react5.useCallback)((object2) => {
        if (document2 === null || object2 === void 0 || object2 === null) return;
        const query = typeof object2.guid === "string" && object2.guid !== "" ? object2.guid : object2.name;
        setSelected({ name: object2.name, guid: object2.guid ?? "" });
        setModuleName("");
        setLoadedModule("");
        setFormName("");
        setViewMode("object");
        run({ path: document2.file.path, object: query }, (body) => {
          setDetail(body);
          const first = Array.isArray(body.object?.sections) && body.object.sections.length > 0 ? body.object.sections[0].kind : "";
          setActiveSection(first);
        });
      }, [document2, run]);
      const openModule = (0, import_react5.useCallback)((name) => {
        if (document2 === null || selected === null) return;
        const query = selected.guid !== "" ? selected.guid : selected.name;
        setModuleName(name);
        setLoadedModule("");
        setFormName("");
        setViewMode("module");
        run({ path: document2.file.path, object: query, module: name }, (body) => {
          setDetail(body);
          setLoadedModule(name);
        });
      }, [document2, selected, run]);
      const openForm = (0, import_react5.useCallback)((name) => {
        if (document2 === null || detail === null || detail.object === null) return;
        if (!sameObject(detail.object, selected)) return;
        setError(null);
        setFormName(name);
        setModuleName("");
        setLoadedModule("");
        setViewMode("form");
      }, [document2, detail, selected]);
      const showObject = (0, import_react5.useCallback)((kind) => {
        if (kind !== void 0 && kind !== "") setActiveSection(kind);
        setError(null);
        setModuleName("");
        setLoadedModule("");
        setFormName("");
        setViewMode("object");
      }, []);
      const reload = (0, import_react5.useCallback)(() => {
        if (document2 === null) {
          if (String(pathInput ?? "").trim() === "") return;
          openPath(pathInput);
          return;
        }
        if (viewMode === "form") {
          setFormNonce((current) => current + 1);
          return;
        }
        if (viewMode === "module") {
          if (moduleName !== "") openModule(moduleName);
          return;
        }
        if (selected === null) {
          openPath(document2.file.path);
          return;
        }
        openObject(selected);
      }, [document2, moduleName, openModule, openObject, openPath, pathInput, selected, viewMode]);
      (0, import_react5.useEffect)(() => {
        const actions = tab?.actions;
        if (actions === void 0 || typeof actions.bindCommands !== "function") return void 0;
        return actions.bindCommands({ refresh: reload });
      }, [tab, reload]);
      (0, import_react5.useEffect)(() => () => {
        controller.current?.abort();
      }, []);
      const onZoom = (0, import_react5.useCallback)((value) => {
        setZoom(value);
        writeZoom(value);
      }, []);
      const object = detail?.object ?? null;
      const content = detail?.content ?? null;
      const modules = Array.isArray(object?.modules) ? object.modules : [];
      const sections = Array.isArray(object?.sections) ? object.sections : [];
      const active = sections.find((section) => section.kind === activeSection) ?? sections[0];
      const detailMatches = sameObject(object, selected);
      const moduleText = detailMatches && loadedModule === moduleName && typeof content?.text === "string" ? content.text : "";
      const hasPath = String(pathInput ?? "").trim() !== "";
      const selectedQuery = selected === null ? "" : selected.guid !== "" ? selected.guid : selected.name;
      const modulesTab = activeSection === MODULES_GROUP && modules.length > 0;
      return /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r", "data-scheme": scheme }, /* @__PURE__ */ import_react5.default.createElement("style", null, PANEL_CSS), /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-toolbar" }, /* @__PURE__ */ import_react5.default.createElement(
        "input",
        {
          className: "v8r-input",
          type: "text",
          value: pathInput,
          spellCheck: false,
          placeholder: t("panel.path.placeholder"),
          "aria-label": t("panel.path.placeholder"),
          onChange: (event) => {
            setPathInput(event.target.value);
          },
          onKeyDown: (event) => {
            if (event.key === "Enter") openFromToolbar();
          }
        }
      ), /* @__PURE__ */ import_react5.default.createElement(
        "button",
        {
          type: "button",
          className: hasPath ? "v8r-button v8r-button--primary" : "v8r-button",
          disabled: busy,
          title: t("panel.open.title"),
          onClick: openFromToolbar
        },
        t("panel.open")
      ), /* @__PURE__ */ import_react5.default.createElement(
        "button",
        {
          type: "button",
          className: hasPath ? "v8r-button" : "v8r-button v8r-button--primary",
          title: t("panel.browse.title"),
          ref: browseButton,
          onClick: openChooser
        },
        t("panel.browse")
      ), document2 !== null && /* @__PURE__ */ import_react5.default.createElement(
        "button",
        {
          type: "button",
          className: "v8r-button",
          disabled: busy,
          onClick: reload
        },
        t("panel.reload")
      )), busy && /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-loading", role: "status" }, document2 !== null ? t("panel.loading.object") : t("panel.loading")), error !== null && /* @__PURE__ */ import_react5.default.createElement(ErrorBanner, { error, translate: t }), document2 === null && !busy && error === null && !pickerOpen && /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-empty" }, /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-empty-title" }, t("panel.empty.title")), /* @__PURE__ */ import_react5.default.createElement("div", null, t("panel.empty.hint")), /* @__PURE__ */ import_react5.default.createElement(
        "button",
        {
          type: "button",
          className: "v8r-button v8r-button--primary",
          onClick: openChooser
        },
        t("panel.chooseFile")
      )), document2 !== null && /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-body" }, /* @__PURE__ */ import_react5.default.createElement(
        Tree,
        {
          key: document2.file?.path ?? "tree",
          file: document2.file,
          categories: document2.categories,
          selected,
          detail,
          viewMode,
          activeSection,
          moduleName,
          formName,
          busy,
          onSelect: openObject,
          onSection: showObject,
          onOpenForm: openForm,
          onOpenModule: openModule,
          translate: t
        }
      ), /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-main", "data-view": viewMode }, /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-subtitle" }, t("panel.file.summary", {
        kind: fileKindLabel(t, document2.file?.kind),
        size: formatBytes(document2.file?.size),
        count: document2.file?.objectCount ?? 0
      }), document2.file?.formatVersion !== void 0 && /* @__PURE__ */ import_react5.default.createElement(import_react5.default.Fragment, null, " \xB7 ", t("panel.file.format", { version: document2.file.formatVersion })), Array.isArray(document2.file?.warnings) && document2.file.warnings.length > 0 && /* @__PURE__ */ import_react5.default.createElement(import_react5.default.Fragment, null, " \xB7 ", t("panel.warnings", { count: document2.file.warnings.length }))), viewMode === "object" && selected === null && /* @__PURE__ */ import_react5.default.createElement("p", { className: "v8r-muted" }, t("panel.selectObject")), viewMode === "object" && selected !== null && !detailMatches && /* @__PURE__ */ import_react5.default.createElement(import_react5.default.Fragment, null, /* @__PURE__ */ import_react5.default.createElement("h3", { className: "v8r-title" }, selected.name), /* @__PURE__ */ import_react5.default.createElement("p", { className: "v8r-muted" }, busy ? t("panel.loading.object") : t("panel.objectUnavailable"))), viewMode === "object" && detailMatches && object !== null && /* @__PURE__ */ import_react5.default.createElement(import_react5.default.Fragment, null, /* @__PURE__ */ import_react5.default.createElement("h3", { className: "v8r-title" }, object.name), /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-subtitle" }, object.typeTitle, object.synonym !== void 0 && object.synonym !== "" && /* @__PURE__ */ import_react5.default.createElement(import_react5.default.Fragment, null, " \xB7 ", object.synonym)), /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-tabs", role: "tablist" }, sections.map((section) => /* @__PURE__ */ import_react5.default.createElement(
        "button",
        {
          key: section.kind,
          type: "button",
          role: "tab",
          className: "v8r-tab",
          "aria-selected": !modulesTab && active !== void 0 && section.kind === active.kind ? "true" : "false",
          onClick: () => {
            setActiveSection(section.kind);
          }
        },
        sectionLabel(t, section),
        " (",
        Array.isArray(section.entries) ? section.entries.length : 0,
        ")"
      )), modules.length > 0 && /* @__PURE__ */ import_react5.default.createElement(
        "button",
        {
          type: "button",
          role: "tab",
          className: "v8r-tab",
          "aria-selected": modulesTab ? "true" : "false",
          onClick: () => {
            setActiveSection(MODULES_GROUP);
          }
        },
        t("panel.modules"),
        " (",
        modules.length,
        ")"
      )), modulesTab ? /* @__PURE__ */ import_react5.default.createElement("div", { className: "v8r-modules" }, modules.map((module2) => /* @__PURE__ */ import_react5.default.createElement(
        "button",
        {
          key: module2.name,
          type: "button",
          className: "v8r-button",
          disabled: busy,
          onClick: () => {
            openModule(module2.name);
          }
        },
        module2.name
      ))) : active !== void 0 ? /* @__PURE__ */ import_react5.default.createElement(
        SectionTable,
        {
          section: active,
          translate: t,
          onOpenForm: openForm
        }
      ) : null), viewMode === "module" && detailMatches && object !== null && /* @__PURE__ */ import_react5.default.createElement(
        ModuleView,
        {
          object,
          modules,
          moduleName,
          text: moduleText,
          loaded: detailMatches && loadedModule === moduleName,
          busy,
          scheme,
          translate: t,
          onBack: () => {
            showObject();
          },
          onOpenModule: openModule
        }
      ), viewMode === "module" && !detailMatches && /* @__PURE__ */ import_react5.default.createElement("p", { className: "v8r-muted" }, busy ? t("panel.loading.object") : t("panel.objectUnavailable")), viewMode === "form" && detailMatches && object !== null && /* @__PURE__ */ import_react5.default.createElement(
        FormView,
        {
          object,
          formName,
          src: buildFormUrl(document2.file.path, selectedQuery, formName),
          zoom,
          nonce: formNonce,
          translate: t,
          onBack: () => {
            showObject();
          },
          onZoom,
          onReload: reload
        }
      ), viewMode === "form" && !detailMatches && /* @__PURE__ */ import_react5.default.createElement("p", { className: "v8r-muted" }, busy ? t("panel.loading.object") : t("panel.objectUnavailable")))), pickerOpen && /* @__PURE__ */ import_react5.default.createElement(
        FilePicker,
        {
          initialDir: lastDir,
          translate: t,
          onPick: pickFromChooser,
          onClose: closeChooser
        }
      ));
    }
    
    // src/client/messages.js
    var ru = {
      "type.title": "1\u0421: \u043A\u043E\u043D\u0444\u0438\u0433\u0443\u0440\u0430\u0446\u0438\u044F",
      "guide.title": "1\u0421: \u0447\u0442\u0435\u043D\u0438\u0435 \u043A\u043E\u043D\u0444\u0438\u0433\u0443\u0440\u0430\u0446\u0438\u0438",
      "guide.description": "\u0414\u0435\u0440\u0435\u0432\u043E \u043C\u0435\u0442\u0430\u0434\u0430\u043D\u043D\u044B\u0445, \u0441\u043E\u0441\u0442\u0430\u0432 \u043E\u0431\u044A\u0435\u043A\u0442\u0430 \u0438 \u043C\u043E\u0434\u0443\u043B\u0438 \u0444\u0430\u0439\u043B\u0430 .cf/.cfe/.epf/.erf.",
      "panel.open": "\u041E\u0442\u043A\u0440\u044B\u0442\u044C",
      "panel.open.title": "\u041F\u0440\u043E\u0447\u0438\u0442\u0430\u0442\u044C \u0444\u0430\u0439\u043B \u043F\u043E \u043F\u0443\u0442\u0438 \u0438\u0437 \u0441\u0442\u0440\u043E\u043A\u0438; \u043F\u0443\u0441\u0442\u0430\u044F \u0441\u0442\u0440\u043E\u043A\u0430 \u043E\u0442\u043A\u0440\u044B\u0432\u0430\u0435\u0442 \u043E\u0431\u0437\u043E\u0440",
      "panel.browse": "\u041E\u0431\u0437\u043E\u0440\u2026",
      "panel.browse.title": "\u0412\u044B\u0431\u0440\u0430\u0442\u044C \u0444\u0430\u0439\u043B \u043A\u043E\u043D\u0444\u0438\u0433\u0443\u0440\u0430\u0446\u0438\u0438 \u043D\u0430 \u0434\u0438\u0441\u043A\u0435",
      "panel.chooseFile": "\u0412\u044B\u0431\u0440\u0430\u0442\u044C \u0444\u0430\u0439\u043B\u2026",
      "panel.reload": "\u041E\u0431\u043D\u043E\u0432\u0438\u0442\u044C",
      "panel.path.placeholder": "\u0410\u0431\u0441\u043E\u043B\u044E\u0442\u043D\u044B\u0439 \u043F\u0443\u0442\u044C \u043A \u0444\u0430\u0439\u043B\u0443 .cf, .cfe, .epf \u0438\u043B\u0438 .erf",
      "panel.filter.placeholder": "\u0424\u0438\u043B\u044C\u0442\u0440 \u043F\u043E \u0438\u043C\u0435\u043D\u0438 \u0438\u043B\u0438 \u0441\u0438\u043D\u043E\u043D\u0438\u043C\u0443",
      "panel.clear": "\u041E\u0447\u0438\u0441\u0442\u0438\u0442\u044C",
      "panel.empty.title": "\u0424\u0430\u0439\u043B \u043D\u0435 \u043E\u0442\u043A\u0440\u044B\u0442",
      "panel.empty.hint": "\u0423\u043A\u0430\u0436\u0438\u0442\u0435 \u0430\u0431\u0441\u043E\u043B\u044E\u0442\u043D\u044B\u0439 \u043F\u0443\u0442\u044C \u043A \u043A\u043E\u043D\u0444\u0438\u0433\u0443\u0440\u0430\u0446\u0438\u0438 1\u0421 \u0438 \u043D\u0430\u0436\u043C\u0438\u0442\u0435 \xAB\u041E\u0442\u043A\u0440\u044B\u0442\u044C\xBB \u0438\u043B\u0438 \u0432\u044B\u0431\u0435\u0440\u0438\u0442\u0435 \u0444\u0430\u0439\u043B \u043D\u0430 \u0434\u0438\u0441\u043A\u0435. \u0424\u0430\u0439\u043B \u043C\u043E\u0436\u0435\u0442 \u043B\u0435\u0436\u0430\u0442\u044C \u0433\u0434\u0435 \u0443\u0433\u043E\u0434\u043D\u043E: \u0447\u0438\u0442\u0430\u0435\u0442\u0441\u044F \u043E\u043D \u0442\u043E\u043B\u044C\u043A\u043E \u043D\u0430 \u0447\u0442\u0435\u043D\u0438\u0435.",
      "panel.loading": "\u0427\u0442\u0435\u043D\u0438\u0435 \u0444\u0430\u0439\u043B\u0430\u2026",
      "panel.loading.object": "\u0427\u0442\u0435\u043D\u0438\u0435 \u0441\u043E\u0441\u0442\u0430\u0432\u0430 \u043E\u0431\u044A\u0435\u043A\u0442\u0430\u2026",
      "panel.file.summary": "{kind}, {size}, \u043E\u0431\u044A\u0435\u043A\u0442\u043E\u0432: {count}",
      "panel.file.format": "\u0444\u043E\u0440\u043C\u0430\u0442 {version}",
      "panel.noObjects": "\u0412 \u0444\u0430\u0439\u043B\u0435 \u043D\u0435\u0442 \u043E\u0431\u044A\u0435\u043A\u0442\u043E\u0432 \u043C\u0435\u0442\u0430\u0434\u0430\u043D\u043D\u044B\u0445.",
      "panel.selectObject": "\u0412\u044B\u0431\u0435\u0440\u0438\u0442\u0435 \u043E\u0431\u044A\u0435\u043A\u0442 \u0432 \u0434\u0435\u0440\u0435\u0432\u0435 \u0441\u043B\u0435\u0432\u0430.",
      "panel.objects.count": "\u043E\u0431\u044A\u0435\u043A\u0442\u043E\u0432: {count}",
      "panel.matched": "\u0441\u043E\u0432\u043F\u0430\u0434\u0435\u043D\u0438\u0439: {count}",
      "panel.showMore": "\u041F\u043E\u043A\u0430\u0437\u0430\u0442\u044C \u0435\u0449\u0451",
      "panel.sectionEmpty": "\u0420\u0430\u0437\u0434\u0435\u043B \u043F\u0443\u0441\u0442.",
      "panel.modules": "\u041C\u043E\u0434\u0443\u043B\u0438",
      "panel.noModules": "\u0423 \u043E\u0431\u044A\u0435\u043A\u0442\u0430 \u043D\u0435\u0442 \u043C\u043E\u0434\u0443\u043B\u0435\u0439.",
      "panel.module.empty": "\u041C\u043E\u0434\u0443\u043B\u044C \u043F\u0443\u0441\u0442.",
      "panel.objectUnavailable": "\u0421\u043E\u0441\u0442\u0430\u0432 \u043E\u0431\u044A\u0435\u043A\u0442\u0430 \u043D\u0435 \u043F\u0440\u043E\u0447\u0438\u0442\u0430\u043D. \u041F\u043E\u0434\u0440\u043E\u0431\u043D\u043E\u0441\u0442\u0438 \u2014 \u0432 \u0441\u043E\u043E\u0431\u0449\u0435\u043D\u0438\u0438 \u043E\u0431 \u043E\u0448\u0438\u0431\u043A\u0435.",
      "panel.back": "\u041A \u043E\u0431\u044A\u0435\u043A\u0442\u0443",
      "panel.openForm": "\u041E\u0442\u043A\u0440\u044B\u0442\u044C \u0444\u043E\u0440\u043C\u0443",
      "panel.column.name": "\u0418\u043C\u044F",
      "panel.column.synonym": "\u0421\u0438\u043D\u043E\u043D\u0438\u043C",
      "panel.column.type": "\u0422\u0438\u043F",
      "panel.warnings": "\u041F\u0440\u0435\u0434\u0443\u043F\u0440\u0435\u0436\u0434\u0435\u043D\u0438\u044F: {count}",
      "tree.root": "\u041A\u043E\u043D\u0444\u0438\u0433\u0443\u0440\u0430\u0446\u0438\u044F",
      "tree.loading": "\u0427\u0442\u0435\u043D\u0438\u0435 \u0441\u043E\u0441\u0442\u0430\u0432\u0430 \u043E\u0431\u044A\u0435\u043A\u0442\u0430\u2026",
      "code.highlight": "\u041F\u043E\u0434\u0441\u0432\u0435\u0442\u043A\u0430",
      "code.highlight.title": "\u0420\u0430\u0441\u043A\u0440\u0430\u0448\u0438\u0432\u0430\u0442\u044C \u043A\u043B\u044E\u0447\u0435\u0432\u044B\u0435 \u0441\u043B\u043E\u0432\u0430, \u0441\u0442\u0440\u043E\u043A\u0438 \u0438 \u043A\u043E\u043C\u043C\u0435\u043D\u0442\u0430\u0440\u0438\u0438",
      "code.wrap": "\u041F\u0435\u0440\u0435\u043D\u043E\u0441",
      "code.wrap.title": "\u041F\u0435\u0440\u0435\u043D\u043E\u0441\u0438\u0442\u044C \u0434\u043B\u0438\u043D\u043D\u044B\u0435 \u0441\u0442\u0440\u043E\u043A\u0438 \u043F\u043E \u0448\u0438\u0440\u0438\u043D\u0435 \u043F\u0430\u043D\u0435\u043B\u0438",
      "code.font.smaller": "\u0423\u043C\u0435\u043D\u044C\u0448\u0438\u0442\u044C \u0448\u0440\u0438\u0444\u0442",
      "code.font.larger": "\u0423\u0432\u0435\u043B\u0438\u0447\u0438\u0442\u044C \u0448\u0440\u0438\u0444\u0442",
      "code.lines": "\u0421\u0442\u0440\u043E\u043A: {count}",
      "code.copy": "\u041A\u043E\u043F\u0438\u0440\u043E\u0432\u0430\u0442\u044C",
      "code.copied": "\u0421\u043A\u043E\u043F\u0438\u0440\u043E\u0432\u0430\u043D\u043E",
      "code.region": "\u041A\u043E\u0434 \u043C\u043E\u0434\u0443\u043B\u044F",
      "code.showMore": "\u041F\u043E\u043A\u0430\u0437\u0430\u0442\u044C \u0435\u0449\u0451 {rest} \u0441\u0442\u0440\u043E\u043A",
      "form.loading": "\u041E\u0442\u0440\u0438\u0441\u043E\u0432\u043A\u0430 \u0444\u043E\u0440\u043C\u044B\u2026",
      "form.error.title": "\u0424\u043E\u0440\u043C\u0430 \u043D\u0435 \u043E\u0442\u0440\u0438\u0441\u043E\u0432\u0430\u043D\u0430",
      "form.retry": "\u041F\u043E\u0432\u0442\u043E\u0440\u0438\u0442\u044C",
      "form.zoom": "\u041C\u0430\u0441\u0448\u0442\u0430\u0431",
      "picker.title": "\u0412\u044B\u0431\u043E\u0440 \u0444\u0430\u0439\u043B\u0430 \u043A\u043E\u043D\u0444\u0438\u0433\u0443\u0440\u0430\u0446\u0438\u0438",
      "picker.close": "\u0417\u0430\u043A\u0440\u044B\u0442\u044C \u043E\u0431\u0437\u043E\u0440",
      "picker.up": "\u0420\u043E\u0434\u0438\u0442\u0435\u043B\u044C\u0441\u043A\u0430\u044F \u043F\u0430\u043F\u043A\u0430",
      "picker.goto": "\u041F\u0443\u0442\u044C \u043A \u043F\u0430\u043F\u043A\u0435 \u0438\u043B\u0438 \u0444\u0430\u0439\u043B\u0443",
      "picker.goto.action": "\u041F\u0435\u0440\u0435\u0439\u0442\u0438",
      "picker.filter": "\u0424\u0438\u043B\u044C\u0442\u0440 \u043F\u043E \u0438\u043C\u0435\u043D\u0438 \u0444\u0430\u0439\u043B\u0430",
      "picker.allFiles": "\u0412\u0441\u0435 \u0444\u0430\u0439\u043B\u044B",
      "picker.loading": "\u0427\u0442\u0435\u043D\u0438\u0435 \u043F\u0430\u043F\u043A\u0438\u2026",
      "picker.empty": "\u041F\u0430\u043F\u043A\u0430 \u043F\u0443\u0441\u0442\u0430.",
      "picker.emptyFiltered": "\u0417\u0434\u0435\u0441\u044C \u043D\u0435\u0442 \u0444\u0430\u0439\u043B\u043E\u0432 .cf, .cfe, .epf, .erf. \u0412\u043A\u043B\u044E\u0447\u0438\u0442\u0435 \xAB\u0412\u0441\u0435 \u0444\u0430\u0439\u043B\u044B\xBB, \u0447\u0442\u043E\u0431\u044B \u0443\u0432\u0438\u0434\u0435\u0442\u044C \u043E\u0441\u0442\u0430\u043B\u044C\u043D\u044B\u0435.",
      "picker.notFound": "\u041D\u0438\u0447\u0435\u0433\u043E \u043D\u0435 \u043D\u0430\u0439\u0434\u0435\u043D\u043E \u043F\u043E \xAB{query}\xBB.",
      "picker.resetFilter": "\u0421\u0431\u0440\u043E\u0441\u0438\u0442\u044C \u0444\u0438\u043B\u044C\u0442\u0440",
      "picker.hint": "\u0412\u044B\u0431\u0435\u0440\u0438\u0442\u0435 \u043F\u0430\u043F\u043A\u0443 \u0438\u043B\u0438 \u0444\u0430\u0439\u043B .cf, .cfe, .epf, .erf.",
      "picker.hidden": "\u041F\u043E\u043A\u0430\u0437\u0430\u0442\u044C \u0441\u043A\u0440\u044B\u0442\u044B\u0435 \u0444\u0430\u0439\u043B\u044B: {count}",
      "picker.truncated": "\u041F\u043E\u043A\u0430\u0437\u0430\u043D\u044B \u043F\u0435\u0440\u0432\u044B\u0435 {count} \u044D\u043B\u0435\u043C\u0435\u043D\u0442\u043E\u0432.",
      "picker.unsupported": "\u043F\u0430\u043D\u0435\u043B\u044C \u043E\u0442\u043A\u0440\u044B\u0432\u0430\u0435\u0442 \u0442\u043E\u043B\u044C\u043A\u043E .cf, .cfe, .epf \u0438 .erf",
      "picker.retry": "\u041F\u043E\u0432\u0442\u043E\u0440\u0438\u0442\u044C",
      "picker.home": "\u0414\u043E\u043C\u0430\u0448\u043D\u044F\u044F \u043F\u0430\u043F\u043A\u0430",
      "picker.error.missing": "\u041F\u0430\u043F\u043A\u0430 \u043D\u0435 \u043D\u0430\u0439\u0434\u0435\u043D\u0430.",
      "picker.error.notDirectory": "\u042D\u0442\u043E \u043D\u0435 \u043F\u0430\u043F\u043A\u0430.",
      "kind.configuration": "\u041A\u043E\u043D\u0444\u0438\u0433\u0443\u0440\u0430\u0446\u0438\u044F",
      "kind.extension": "\u0420\u0430\u0441\u0448\u0438\u0440\u0435\u043D\u0438\u0435",
      "kind.externalProcessing": "\u0412\u043D\u0435\u0448\u043D\u044F\u044F \u043E\u0431\u0440\u0430\u0431\u043E\u0442\u043A\u0430",
      "kind.externalReport": "\u0412\u043D\u0435\u0448\u043D\u0438\u0439 \u043E\u0442\u0447\u0451\u0442",
      "section.attributes": "\u0420\u0435\u043A\u0432\u0438\u0437\u0438\u0442\u044B",
      "section.tabularSections": "\u0422\u0430\u0431\u043B\u0438\u0447\u043D\u044B\u0435 \u0447\u0430\u0441\u0442\u0438",
      "section.tabularAttributes": "\u0420\u0435\u043A\u0432\u0438\u0437\u0438\u0442\u044B \u0442\u0430\u0431\u043B\u0438\u0447\u043D\u044B\u0445 \u0447\u0430\u0441\u0442\u0435\u0439",
      "section.forms": "\u0424\u043E\u0440\u043C\u044B",
      "section.templates": "\u041C\u0430\u043A\u0435\u0442\u044B",
      "section.commands": "\u041A\u043E\u043C\u0430\u043D\u0434\u044B",
      "section.dimensions": "\u0418\u0437\u043C\u0435\u0440\u0435\u043D\u0438\u044F",
      "section.resources": "\u0420\u0435\u0441\u0443\u0440\u0441\u044B",
      "section.accountingFlags": "\u041F\u0440\u0438\u0437\u043D\u0430\u043A\u0438 \u0443\u0447\u0451\u0442\u0430",
      "section.subcontoFlags": "\u041F\u0440\u0438\u0437\u043D\u0430\u043A\u0438 \u0441\u0443\u0431\u043A\u043E\u043D\u0442\u043E",
      "section.values": "\u0417\u043D\u0430\u0447\u0435\u043D\u0438\u044F",
      "section.columns": "\u041A\u043E\u043B\u043E\u043D\u043A\u0438",
      "section.journalColumns": "\u0413\u0440\u0430\u0444\u044B \u0436\u0443\u0440\u043D\u0430\u043B\u0430",
      "error.title": "\u041D\u0435 \u0443\u0434\u0430\u043B\u043E\u0441\u044C \u043F\u0440\u043E\u0447\u0438\u0442\u0430\u0442\u044C \u0444\u0430\u0439\u043B",
      "error.bad-path": "\u041F\u0443\u0442\u044C \u043D\u0435 \u043F\u0440\u0438\u043D\u044F\u0442. \u041D\u0443\u0436\u0435\u043D \u0430\u0431\u0441\u043E\u043B\u044E\u0442\u043D\u044B\u0439 \u043F\u0443\u0442\u044C \u043A \u043E\u0431\u044B\u0447\u043D\u043E\u043C\u0443 \u0444\u0430\u0439\u043B\u0443 .cf, .cfe, .epf \u0438\u043B\u0438 .erf.",
      "error.unsupported-format": "\u042D\u0442\u043E\u0442 \u0444\u043E\u0440\u043C\u0430\u0442 \u0444\u0430\u0439\u043B\u0430 \u043D\u0435 \u043F\u043E\u0434\u0434\u0435\u0440\u0436\u0438\u0432\u0430\u0435\u0442\u0441\u044F.",
      "error.cli-failed": "v8reader_cli \u0437\u0430\u0432\u0435\u0440\u0448\u0438\u043B\u0441\u044F \u0441 \u043E\u0448\u0438\u0431\u043A\u043E\u0439.",
      "error.cli-missing": "v8reader_cli \u043D\u0435 \u043D\u0430\u0439\u0434\u0435\u043D.",
      "error.cli-old": "\u0412\u0435\u0440\u0441\u0438\u044F v8reader_cli \u0441\u0442\u0430\u0440\u0435\u0435 \u0442\u0440\u0435\u0431\u0443\u0435\u043C\u043E\u0439 ({min}).",
      "error.too-large": "\u041E\u0442\u0432\u0435\u0442 \u0431\u043E\u043B\u044C\u0448\u0435 \u0434\u043E\u043F\u0443\u0441\u0442\u0438\u043C\u043E\u0433\u043E \u0440\u0430\u0437\u043C\u0435\u0440\u0430. \u041E\u0442\u043A\u0440\u043E\u0439\u0442\u0435 \u0444\u0430\u0439\u043B \u0432 \u043F\u0440\u0438\u043B\u043E\u0436\u0435\u043D\u0438\u0438 qtv8reader.",
      "error.timeout": "v8reader_cli \u043D\u0435 \u043E\u0442\u0432\u0435\u0442\u0438\u043B \u0437\u0430 \u043E\u0442\u0432\u0435\u0434\u0451\u043D\u043D\u043E\u0435 \u0432\u0440\u0435\u043C\u044F.",
      "error.denied": "\u041D\u0435\u0442 \u0434\u043E\u0441\u0442\u0443\u043F\u0430 \u043A \u043F\u0430\u043F\u043A\u0435 \u0438\u043B\u0438 \u0444\u0430\u0439\u043B\u0443.",
      "error.forbidden": "\u0417\u0430\u043F\u0440\u043E\u0441 \u043F\u0440\u0438\u0448\u0451\u043B \u0441 \u0447\u0443\u0436\u043E\u0433\u043E \u0441\u0430\u0439\u0442\u0430 \u0438 \u043E\u0442\u043A\u043B\u043E\u043D\u0451\u043D.",
      "error.not-found": "\u041C\u0430\u0440\u0448\u0440\u0443\u0442 \u043F\u043B\u0430\u0433\u0438\u043D\u0430 \u043D\u0435 \u043D\u0430\u0439\u0434\u0435\u043D. \u0412\u043E\u0437\u043C\u043E\u0436\u043D\u043E, \u0432\u0435\u0440\u0441\u0438\u0438 \u043F\u043E\u043B\u043E\u0432\u0438\u043D \u043F\u043B\u0430\u0433\u0438\u043D\u0430 \u0440\u0430\u0437\u043E\u0448\u043B\u0438\u0441\u044C.",
      "error.method-not-allowed": "\u041D\u0435\u043F\u043E\u0434\u0434\u0435\u0440\u0436\u0438\u0432\u0430\u0435\u043C\u044B\u0439 \u043C\u0435\u0442\u043E\u0434 \u0437\u0430\u043F\u0440\u043E\u0441\u0430.",
      "error.bad-request": "\u0417\u0430\u043F\u0440\u043E\u0441 \u043A \u043F\u043B\u0430\u0433\u0438\u043D\u0443 \u0441\u043E\u0431\u0440\u0430\u043D \u043D\u0435\u0432\u0435\u0440\u043D\u043E.",
      "error.assets-missing": "\u0420\u0435\u0441\u0443\u0440\u0441\u044B \u0440\u0435\u043D\u0434\u0435\u0440\u0435\u0440\u0430 \u0444\u043E\u0440\u043C\u044B \u043D\u0435 \u0443\u0441\u0442\u0430\u043D\u043E\u0432\u043B\u0435\u043D\u044B: \u0432\u044B\u043F\u043E\u043B\u043D\u0438\u0442\u0435 npm run assets \u0432 \u043A\u0430\u0442\u0430\u043B\u043E\u0433\u0435 \u043F\u043B\u0430\u0433\u0438\u043D\u0430.",
      "error.form-error": "\u0420\u0435\u043D\u0434\u0435\u0440\u0435\u0440 \u0444\u043E\u0440\u043C\u044B \u0441\u043E\u043E\u0431\u0449\u0438\u043B \u043E\u0431 \u043E\u0448\u0438\u0431\u043A\u0435.",
      "error.form-timeout": "\u0420\u0435\u043D\u0434\u0435\u0440\u0435\u0440 \u0444\u043E\u0440\u043C\u044B \u043D\u0435 \u043E\u0442\u0432\u0435\u0442\u0438\u043B \u0437\u0430 \u043E\u0442\u0432\u0435\u0434\u0451\u043D\u043D\u043E\u0435 \u0432\u0440\u0435\u043C\u044F.",
      "error.unknown": "\u041D\u0435\u0438\u0437\u0432\u0435\u0441\u0442\u043D\u0430\u044F \u043E\u0448\u0438\u0431\u043A\u0430.",
      "error.hint.cli-missing": "\u0421\u043E\u0431\u0435\u0440\u0438\u0442\u0435 v8reader_cli (cmake --build out/mingw) \u0438 \u0440\u0430\u0437\u043B\u043E\u0436\u0438\u0442\u0435 \u0435\u0433\u043E windeployqt \u0432 out/cli-deploy, \u043B\u0438\u0431\u043E \u0437\u0430\u0434\u0430\u0439\u0442\u0435 config.cliPath \u0432 \u0441\u0442\u0440\u043E\u043A\u0435 \u043F\u043B\u0430\u0433\u0438\u043D\u0430 \u0438\u043B\u0438 \u043F\u0435\u0440\u0435\u043C\u0435\u043D\u043D\u0443\u044E V8READER_CLI.",
      "error.details": "\u041E\u0442\u0432\u0435\u0442 \u0443\u0442\u0438\u043B\u0438\u0442\u044B"
    };
    var en = {
      "type.title": "1C: configuration",
      "guide.title": "1C: read a configuration",
      "guide.description": "Metadata tree, object composition and modules of a .cf/.cfe/.epf/.erf file.",
      "panel.open": "Open",
      "panel.open.title": "Read the file named in the field; an empty field opens the chooser",
      "panel.browse": "Browse\u2026",
      "panel.browse.title": "Choose a configuration file on disk",
      "panel.chooseFile": "Choose a file\u2026",
      "panel.reload": "Reload",
      "panel.path.placeholder": "Absolute path to a .cf, .cfe, .epf or .erf file",
      "panel.filter.placeholder": "Filter by name or synonym",
      "panel.clear": "Clear",
      "panel.empty.title": "No file open",
      "panel.empty.hint": "Enter the absolute path to a 1C configuration and choose Open, or pick a file on disk. The file may live anywhere; it is only read.",
      "panel.loading": "Reading the file\u2026",
      "panel.loading.object": "Reading the object composition\u2026",
      "panel.file.summary": "{kind}, {size}, objects: {count}",
      "panel.file.format": "format {version}",
      "panel.noObjects": "The file has no metadata objects.",
      "panel.selectObject": "Select an object in the tree on the left.",
      "panel.objects.count": "objects: {count}",
      "panel.matched": "matches: {count}",
      "panel.showMore": "Show more",
      "panel.sectionEmpty": "The section is empty.",
      "panel.modules": "Modules",
      "panel.noModules": "The object has no modules.",
      "panel.module.empty": "The module is empty.",
      "panel.objectUnavailable": "The object composition was not read. See the error message.",
      "panel.back": "Back to object",
      "panel.openForm": "Open form",
      "panel.column.name": "Name",
      "panel.column.synonym": "Synonym",
      "panel.column.type": "Type",
      "panel.warnings": "Warnings: {count}",
      "tree.root": "Configuration",
      "tree.loading": "Reading the object composition\u2026",
      "code.highlight": "Highlight",
      "code.highlight.title": "Colour keywords, strings and comments",
      "code.wrap": "Wrap",
      "code.wrap.title": "Wrap long lines to the panel width",
      "code.font.smaller": "Smaller font",
      "code.font.larger": "Larger font",
      "code.lines": "Lines: {count}",
      "code.copy": "Copy",
      "code.copied": "Copied",
      "code.region": "Module code",
      "code.showMore": "Show {rest} more lines",
      "form.loading": "Rendering the form\u2026",
      "form.error.title": "The form did not render",
      "form.retry": "Retry",
      "form.zoom": "Zoom",
      "picker.title": "Choose a configuration file",
      "picker.close": "Close the chooser",
      "picker.up": "Parent folder",
      "picker.goto": "Path to a folder or file",
      "picker.goto.action": "Go",
      "picker.filter": "Filter by file name",
      "picker.allFiles": "All files",
      "picker.loading": "Reading the folder\u2026",
      "picker.empty": "The folder is empty.",
      "picker.emptyFiltered": "No .cf, .cfe, .epf or .erf files here. Turn on \u201CAll files\u201D to see the rest.",
      "picker.notFound": "Nothing matches \u201C{query}\u201D.",
      "picker.resetFilter": "Reset the filter",
      "picker.hint": "Choose a folder or a .cf, .cfe, .epf, .erf file.",
      "picker.hidden": "Show hidden files: {count}",
      "picker.truncated": "Showing the first {count} entries.",
      "picker.unsupported": "the panel opens only .cf, .cfe, .epf and .erf",
      "picker.retry": "Retry",
      "picker.home": "Home folder",
      "picker.error.missing": "The folder was not found.",
      "picker.error.notDirectory": "This is not a folder.",
      "kind.configuration": "Configuration",
      "kind.extension": "Extension",
      "kind.externalProcessing": "External data processor",
      "kind.externalReport": "External report",
      "section.attributes": "Attributes",
      "section.tabularSections": "Tabular sections",
      "section.tabularAttributes": "Tabular section attributes",
      "section.forms": "Forms",
      "section.templates": "Templates",
      "section.commands": "Commands",
      "section.dimensions": "Dimensions",
      "section.resources": "Resources",
      "section.accountingFlags": "Accounting flags",
      "section.subcontoFlags": "Subconto flags",
      "section.values": "Values",
      "section.columns": "Columns",
      "section.journalColumns": "Journal columns",
      "error.title": "Could not read the file",
      "error.bad-path": "The path was rejected. Use an absolute path to a regular .cf, .cfe, .epf or .erf file.",
      "error.unsupported-format": "This file format is not supported.",
      "error.cli-failed": "v8reader_cli exited with an error.",
      "error.cli-missing": "v8reader_cli was not found.",
      "error.cli-old": "v8reader_cli is older than the required {min}.",
      "error.too-large": "The answer is larger than the allowed size. Open the file in the qtv8reader application.",
      "error.timeout": "v8reader_cli did not answer in time.",
      "error.denied": "No access to the folder or file.",
      "error.forbidden": "The request came from another site and was refused.",
      "error.not-found": "The plugin route was not found. The plugin halves may be out of sync.",
      "error.method-not-allowed": "Unsupported request method.",
      "error.bad-request": "The plugin request is malformed.",
      "error.assets-missing": "The form renderer assets are not installed: run npm run assets in the plugin directory.",
      "error.form-error": "The form renderer reported an error.",
      "error.form-timeout": "The form renderer did not answer in time.",
      "error.unknown": "Unknown error.",
      "error.hint.cli-missing": "Build v8reader_cli (cmake --build out/mingw) and deploy it with windeployqt into out/cli-deploy, or set config.cliPath on the plugin row or the V8READER_CLI environment variable.",
      "error.details": "Tool output"
    };
    
    // src/client/scheme.js
    function colorSchemeOf(snapshot, fallback = "light") {
      const scheme = snapshot?.active?.colorScheme ?? snapshot?.colorScheme;
      if (scheme === "dark") return "dark";
      if (scheme === "light") return "light";
      return fallback;
    }
    
    // src/client/index.jsx
    var TYPE_KIND = "v8reader";
    var TYPE_ID = "@fishca/dsh-v8reader";
    var LOCALE_NAMESPACE = "v8reader";
    var inject = ["slots", "locale", "sidebarRightTabs", "theme"];
    function apply(ctx) {
      const t = ctx.locale.bind(LOCALE_NAMESPACE);
      ctx.effect(
        () => ctx.locale.register(LOCALE_NAMESPACE, { ru, en }),
        "dsh-v8reader: dictionaries"
      );
      ctx.effect(
        () => ctx.sidebarRightTabs.register({
          id: TYPE_ID,
          kind: TYPE_KIND,
          priority: "extension",
          keepMounted: true,
          title: () => t("type.title"),
          guide: [{
            id: "open",
            order: 10,
            title: () => t("guide.title"),
            description: () => t("guide.description")
          }]
        }),
        "dsh-v8reader: tab type"
      );
      const listeners = /* @__PURE__ */ new Set();
      let scheme = "light";
      try {
        scheme = colorSchemeOf(ctx.theme.getTheme(), scheme);
      } catch {
      }
      ctx.effect(
        () => ctx.on("theme/change", (snapshot) => {
          scheme = colorSchemeOf(snapshot, scheme);
          for (const listener of listeners) listener(scheme);
        }),
        "dsh-v8reader: theme scheme"
      );
      function PanelBody(props) {
        const [current, setCurrent] = (0, import_react6.useState)(scheme);
        (0, import_react6.useEffect)(() => {
          listeners.add(setCurrent);
          setCurrent(scheme);
          return () => {
            listeners.delete(setCurrent);
          };
        }, []);
        return import_react6.default.createElement(PanelView, { ...props, scheme: current });
      }
      ctx.effect(
        () => ctx.slots.inject("sidebar.right.pane.tab", () => ctx.slots.register(
          { name: "sidebar.right.pane.tab", key: TYPE_ID, locale: LOCALE_NAMESPACE },
          PanelBody
        )),
        "dsh-v8reader: tab body"
      );
    }
    
    return module.exports;
  },
});
