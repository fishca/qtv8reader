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
    
    // src/client/PanelView.jsx
    var import_react3 = __toESM(require("react"), 1);
    
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
    
    // src/client/FilePicker.jsx
    var import_react = __toESM(require("react"), 1);
    
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
      return /* @__PURE__ */ import_react.default.createElement(
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
        /* @__PURE__ */ import_react.default.createElement("span", { className: "v8r-picker-mark", "aria-hidden": "true" }, entry.kind === "dir" ? "\u25B8" : ""),
        /* @__PURE__ */ import_react.default.createElement("span", { className: "v8r-picker-name" }, entry.name),
        entry.kind === "file" && /* @__PURE__ */ import_react.default.createElement("span", { className: "v8r-picker-size" }, formatBytes(entry.size))
      );
    }
    function FilePicker({ initialDir = "", translate: t, onPick, onClose }) {
      const [dir, setDir] = (0, import_react.useState)("");
      const [listing, setListing] = (0, import_react.useState)(null);
      const [pathBar, setPathBar] = (0, import_react.useState)(initialDir);
      const [filter, setFilter] = (0, import_react.useState)("");
      const [showAll, setShowAll] = (0, import_react.useState)(false);
      const [busy, setBusy] = (0, import_react.useState)(true);
      const [error, setError] = (0, import_react.useState)(null);
      const controller = (0, import_react.useRef)(null);
      const layer = (0, import_react.useRef)(null);
      const filterInput = (0, import_react.useRef)(null);
      const crumbRef = (0, import_react.useRef)(null);
      const fellBack = (0, import_react.useRef)(false);
      const lastTarget = (0, import_react.useRef)(initialDir);
      const load = (0, import_react.useCallback)(async (target, all, { fallback = true } = {}) => {
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
      (0, import_react.useEffect)(() => {
        load(initialDir, false);
        filterInput.current?.focus?.();
        return () => {
          controller.current?.abort();
        };
      }, [initialDir, load]);
      (0, import_react.useEffect)(() => {
        crumbRef.current?.scrollIntoView?.({ block: "nearest", inline: "end" });
      }, [dir]);
      const entries = Array.isArray(listing?.entries) ? listing.entries : [];
      const supported = Array.isArray(listing?.supported) ? listing.supported : [];
      const needle = filter.trim().toLowerCase();
      const shown = (0, import_react.useMemo)(
        () => needle === "" ? entries : entries.filter(
          (entry) => entry.name.toLowerCase().includes(needle)
        ),
        [entries, needle]
      );
      const openable = (0, import_react.useCallback)((entry) => {
        if (entry.kind === "dir") return true;
        if (supported.length === 0) return true;
        return supported.includes(extensionOf(entry.name));
      }, [supported]);
      const close = (0, import_react.useCallback)(() => {
        onClose(dir);
      }, [dir, onClose]);
      const openEntry = (0, import_react.useCallback)((entry) => {
        if (entry.kind === "dir") {
          load(entry.path, showAll, { fallback: false });
          return;
        }
        onPick(entry.path);
      }, [load, onPick, showAll]);
      const onKeyDown = (0, import_react.useCallback)((event) => {
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
      return /* @__PURE__ */ import_react.default.createElement(
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
        /* @__PURE__ */ import_react.default.createElement("div", { className: "v8r-picker-head" }, /* @__PURE__ */ import_react.default.createElement("div", { className: "v8r-picker-title" }, t("picker.title")), /* @__PURE__ */ import_react.default.createElement(
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
        /* @__PURE__ */ import_react.default.createElement("div", { className: "v8r-picker-crumbs" }, /* @__PURE__ */ import_react.default.createElement(
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
        ), crumbs.map((crumb) => /* @__PURE__ */ import_react.default.createElement(
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
        /* @__PURE__ */ import_react.default.createElement("div", { className: "v8r-picker-goto" }, /* @__PURE__ */ import_react.default.createElement(
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
        ), /* @__PURE__ */ import_react.default.createElement(
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
        /* @__PURE__ */ import_react.default.createElement("div", { className: "v8r-picker-controls" }, /* @__PURE__ */ import_react.default.createElement(
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
        ), /* @__PURE__ */ import_react.default.createElement("label", { className: "v8r-picker-check" }, /* @__PURE__ */ import_react.default.createElement(
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
        busy && /* @__PURE__ */ import_react.default.createElement("div", { className: "v8r-loading", role: "status" }, t("picker.loading")),
        error !== null && /* @__PURE__ */ import_react.default.createElement("div", { className: "v8r-banner", role: "alert" }, /* @__PURE__ */ import_react.default.createElement("div", { className: "v8r-banner-title" }, t("error.title"), ": ", browseErrorText(t, error)), /* @__PURE__ */ import_react.default.createElement("div", { className: "v8r-banner-actions" }, /* @__PURE__ */ import_react.default.createElement(
          "button",
          {
            type: "button",
            className: "v8r-button",
            onClick: () => {
              load(lastTarget.current, showAll, { fallback: false });
            }
          },
          t("picker.retry")
        ), /* @__PURE__ */ import_react.default.createElement("button", { type: "button", className: "v8r-button", onClick: () => {
          load("", showAll, { fallback: false });
        } }, t("picker.home")))),
        /* @__PURE__ */ import_react.default.createElement("div", { className: "v8r-picker-list", "data-busy": busy ? "true" : "false" }, shown.length === 0 && !busy && error === null && (needle === "" ? /* @__PURE__ */ import_react.default.createElement("p", { className: "v8r-muted" }, showAll ? t("picker.empty") : t("picker.emptyFiltered")) : /* @__PURE__ */ import_react.default.createElement("div", { className: "v8r-picker-none" }, /* @__PURE__ */ import_react.default.createElement("span", { className: "v8r-muted" }, t("picker.notFound", { query: filter.trim() })), /* @__PURE__ */ import_react.default.createElement("button", { type: "button", className: "v8r-button", onClick: () => {
          setFilter("");
        } }, t("picker.resetFilter")))), shown.map((entry) => /* @__PURE__ */ import_react.default.createElement(
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
        /* @__PURE__ */ import_react.default.createElement("div", { className: "v8r-picker-foot" }, /* @__PURE__ */ import_react.default.createElement("span", { className: "v8r-muted" }, t("picker.hint")), typeof listing?.hiddenCount === "number" && listing.hiddenCount > 0 && /* @__PURE__ */ import_react.default.createElement(
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
        ), listing?.truncated === true && /* @__PURE__ */ import_react.default.createElement("span", { className: "v8r-muted" }, t("picker.truncated", { count: entries.length })))
      );
    }
    
    // src/client/styles.js
    var PANEL_CSS = `
    .v8r{position:relative;display:flex;flex-direction:column;height:100%;min-height:0;color:var(--dsw-alias-label-primary);background:var(--dsw-alias-bg-base);font-size:13px;line-height:1.45}
    .v8r-toolbar{display:flex;flex-wrap:wrap;gap:8px;align-items:center;padding:8px 12px;border-bottom:1px solid var(--dsw-alias-border-l1);flex:0 0 auto}
    .v8r-input{flex:1 1 140px;min-width:0;height:28px;padding:0 8px;border-radius:6px;border:1px solid var(--dsw-alias-border-l2);background:var(--dsw-alias-bg-layer-1);color:inherit;font:inherit;outline:none}
    .v8r-input:focus{border-color:var(--dsw-alias-brand-primary)}
    .v8r-button{flex:0 0 auto;height:28px;padding:0 12px;border-radius:6px;border:1px solid var(--dsw-alias-border-l2);background:var(--dsw-alias-bg-layer-1);color:var(--dsw-alias-label-primary);font:inherit;cursor:pointer}
    .v8r-button:hover{background:var(--dsw-alias-bg-layer-2)}
    .v8r-button:disabled{color:var(--dsw-alias-label-secondary);cursor:default}
    .v8r-button--primary{border-color:var(--dsw-alias-brand-primary);color:var(--dsw-alias-brand-primary)}
    .v8r-button--ghost{border-color:transparent;background:transparent}
    .v8r-body{display:flex;flex:1 1 auto;min-height:0}
    .v8r-tree{display:flex;flex-direction:column;flex:0 0 264px;min-width:180px;border-right:1px solid var(--dsw-alias-border-l1);min-height:0}
    .v8r-filter{padding:8px;border-bottom:1px solid var(--dsw-alias-border-l1)}
    .v8r-tree-list{flex:1 1 auto;overflow:auto;padding:4px 4px 12px}
    .v8r-category{display:block;width:100%;text-align:left;padding:5px 8px;border:0;border-radius:5px;background:transparent;color:inherit;font:inherit;cursor:pointer}
    .v8r-category:hover{background:var(--dsw-alias-bg-layer-2)}
    .v8r-category-count{color:var(--dsw-alias-label-secondary);font-size:12px}
    .v8r-object{display:block;width:100%;text-align:left;padding:4px 8px 4px 20px;border:0;border-radius:5px;background:transparent;color:inherit;font:inherit;cursor:pointer;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
    .v8r-object:hover{background:var(--dsw-alias-bg-layer-2)}
    .v8r-object[aria-current="true"]{background:var(--dsw-alias-bg-layer-2);font-weight:600}
    .v8r-object-synonym{color:var(--dsw-alias-label-secondary);font-size:12px;font-weight:400}
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
    .v8r-modules{display:flex;flex-wrap:wrap;gap:6px;margin:12px 0 8px}
    .v8r-code{margin:0;padding:10px 12px;border-radius:8px;border:1px solid var(--dsw-alias-border-l1);background:var(--dsw-alias-bg-layer-1);font-family:ui-monospace,SFMono-Regular,Consolas,monospace;font-size:12.5px;line-height:1.4;white-space:pre;overflow:auto;max-height:60vh}
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
    .v8r-button:focus-visible,.v8r-crumb:focus-visible,.v8r-picker-row:focus-visible,.v8r-link:focus-visible{outline:2px solid var(--dsw-alias-brand-primary);outline-offset:1px}
    .v8r-banner{margin:8px 12px;padding:8px 10px;border-radius:8px;border:1px solid var(--dsw-alias-state-error-primary);color:var(--dsw-alias-state-error-primary);background:var(--dsw-alias-bg-layer-1);font-size:12.5px;flex:0 0 auto}
    .v8r-banner-title{font-weight:600;margin-bottom:2px}
    .v8r-banner-detail{margin-top:6px;white-space:pre-wrap;word-break:break-word;font-family:ui-monospace,SFMono-Regular,Consolas,monospace;font-size:11.5px;color:var(--dsw-alias-label-secondary)}
    .v8r-loading{padding:8px 12px;color:var(--dsw-alias-label-secondary);font-size:12.5px;flex:0 0 auto}
    .v8r-children{padding-left:14px}
    `;
    
    // src/client/Tree.jsx
    var import_react2 = __toESM(require("react"), 1);
    var PAGE = 200;
    function filterCategory(category, query) {
      const objects = Array.isArray(category.objects) ? category.objects : [];
      if (query === "") return { category, items: objects, total: objects.length };
      const items = objects.filter((object) => {
        const name = String(object.name ?? "").toLowerCase();
        const synonym = String(object.synonym ?? "").toLowerCase();
        return name.includes(query) || synonym.includes(query);
      });
      return { category, items, total: items.length };
    }
    function isSelected(object, selected) {
      if (selected === null || selected === void 0) return false;
      if (typeof selected.guid === "string" && selected.guid !== "") {
        return object.guid === selected.guid;
      }
      return object.name === selected.name;
    }
    function Tree({ categories, selected, onSelect, translate: t }) {
      const [filter, setFilter] = (0, import_react2.useState)("");
      const [expanded, setExpanded] = (0, import_react2.useState)(() => {
        const first = Array.isArray(categories) && categories.length > 0 ? categories[0].guid : void 0;
        return new Set(first === void 0 ? [] : [first]);
      });
      const [limits, setLimits] = (0, import_react2.useState)({});
      const query = filter.trim().toLowerCase();
      const groups = (0, import_react2.useMemo)(
        () => (Array.isArray(categories) ? categories : []).map((category) => filterCategory(category, query)),
        [categories, query]
      );
      const toggle = (guid, title) => {
        const key = guid === void 0 || guid === "" ? `title:${title}` : guid;
        setExpanded((current) => {
          const next = new Set(current);
          if (next.has(key)) next.delete(key);
          else next.add(key);
          return next;
        });
      };
      const more = (guid, title, currentLimit) => {
        const key = guid === void 0 || guid === "" ? `title:${title}` : guid;
        setLimits((current) => ({ ...current, [key]: currentLimit + PAGE }));
      };
      return /* @__PURE__ */ import_react2.default.createElement("div", { className: "v8r-tree" }, /* @__PURE__ */ import_react2.default.createElement("div", { className: "v8r-filter" }, /* @__PURE__ */ import_react2.default.createElement(
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
      )), /* @__PURE__ */ import_react2.default.createElement("div", { className: "v8r-tree-list" }, groups.length === 0 && /* @__PURE__ */ import_react2.default.createElement("p", { className: "v8r-muted" }, t("panel.noObjects")), groups.map(({ category, items, total }) => {
        const key = category.guid === void 0 || category.guid === "" ? `title:${category.title}` : category.guid;
        const open = query !== "" ? total > 0 : expanded.has(key);
        const limit = limits[key] ?? PAGE;
        return /* @__PURE__ */ import_react2.default.createElement("div", { key }, /* @__PURE__ */ import_react2.default.createElement(
          "button",
          {
            type: "button",
            className: "v8r-category",
            "aria-expanded": open,
            onClick: () => {
              toggle(category.guid, category.title);
            }
          },
          category.title,
          " ",
          /* @__PURE__ */ import_react2.default.createElement("span", { className: "v8r-category-count" }, `(${total})`)
        ), open && items.slice(0, limit).map((object) => /* @__PURE__ */ import_react2.default.createElement(
          "button",
          {
            key: `${object.guid ?? object.name}:${object.name}`,
            type: "button",
            className: "v8r-object",
            "aria-current": isSelected(object, selected) ? "true" : "false",
            title: object.synonym === "" ? object.name : `${object.name} \u2014 ${object.synonym}`,
            onClick: () => {
              onSelect(object);
            }
          },
          object.name,
          object.synonym !== void 0 && object.synonym !== "" && /* @__PURE__ */ import_react2.default.createElement("span", { className: "v8r-object-synonym" }, ` \u2014 ${object.synonym}`)
        )), open && items.length > limit && /* @__PURE__ */ import_react2.default.createElement(
          "button",
          {
            type: "button",
            className: "v8r-more",
            onClick: () => {
              more(category.guid, category.title, limit);
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
    var MODULE_LIMIT = 3e5;
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
    function SectionTable({ section, translate: t }) {
      const rows = flattenEntries(Array.isArray(section.entries) ? section.entries : []);
      if (rows.length === 0) return /* @__PURE__ */ import_react3.default.createElement("p", { className: "v8r-muted" }, t("panel.sectionEmpty"));
      return /* @__PURE__ */ import_react3.default.createElement("table", { className: "v8r-table" }, /* @__PURE__ */ import_react3.default.createElement("thead", null, /* @__PURE__ */ import_react3.default.createElement("tr", null, /* @__PURE__ */ import_react3.default.createElement("th", null, t("panel.column.name")), /* @__PURE__ */ import_react3.default.createElement("th", null, t("panel.column.synonym")), /* @__PURE__ */ import_react3.default.createElement("th", null, t("panel.column.type")))), /* @__PURE__ */ import_react3.default.createElement("tbody", null, rows.map(({ entry, depth }, index) => /* @__PURE__ */ import_react3.default.createElement("tr", { key: `${entry.name}:${index}` }, /* @__PURE__ */ import_react3.default.createElement("td", null, /* @__PURE__ */ import_react3.default.createElement("span", { style: { paddingLeft: `${depth * 14}px` }, className: "v8r-name" }, entry.name), entry.comment !== void 0 && entry.comment !== "" && /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-muted" }, entry.comment)), /* @__PURE__ */ import_react3.default.createElement("td", null, entry.synonym), /* @__PURE__ */ import_react3.default.createElement("td", { className: "v8r-muted" }, entry.typeText)))));
    }
    function ErrorBanner({ error, translate: t }) {
      const code = typeof error?.code === "string" ? error.code : "unknown";
      const key = `error.${code}`;
      const localized = t(key, error?.detail ?? {});
      const title = localized === key ? t("error.unknown") : localized;
      const hint = code === "cli-missing" || code === "cli-old" ? t("error.hint.cli-missing") : "";
      const searched = error?.detail?.searched;
      return /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-banner", role: "alert" }, /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-banner-title" }, t("error.title"), ": ", title), hint !== "" && /* @__PURE__ */ import_react3.default.createElement("div", null, hint), Array.isArray(searched) && searched.length > 0 && /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-banner-detail" }, searched.join("\n")), typeof error?.message === "string" && error.message !== "" && /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-banner-detail" }, t("error.details"), ": ", error.message));
    }
    function PanelView({ useTabInfo, t: provided }) {
      const t = typeof provided === "function" ? provided : (key) => key;
      const tab = typeof useTabInfo === "function" ? useTabInfo().tab : void 0;
      const [pathInput, setPathInput] = (0, import_react3.useState)(readLastPath);
      const [lastDir, setLastDir] = (0, import_react3.useState)(() => readStored(LAST_DIR_KEY));
      const [pickerOpen, setPickerOpen] = (0, import_react3.useState)(false);
      const [document2, setDocument] = (0, import_react3.useState)(null);
      const [detail, setDetail] = (0, import_react3.useState)(null);
      const [selected, setSelected] = (0, import_react3.useState)(null);
      const [moduleName, setModuleName] = (0, import_react3.useState)("");
      const [activeSection, setActiveSection] = (0, import_react3.useState)("");
      const [busy, setBusy] = (0, import_react3.useState)(false);
      const [error, setError] = (0, import_react3.useState)(null);
      const controller = (0, import_react3.useRef)(null);
      const browseButton = (0, import_react3.useRef)(null);
      const run = (0, import_react3.useCallback)(async (params, onDone, noStore = false) => {
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
      const openPath = (0, import_react3.useCallback)((raw) => {
        const value = String(raw ?? "").trim();
        if (value === "") return;
        setPathInput(value);
        run({ path: value }, (body) => {
          setDocument(body);
          setDetail(null);
          setSelected(null);
          setModuleName("");
          setActiveSection("");
          writeStored(LAST_PATH_KEY, value);
          const dir = dirOf(value);
          if (dir !== "") {
            setLastDir(dir);
            writeStored(LAST_DIR_KEY, dir);
          }
        });
      }, [run]);
      const openFromToolbar = (0, import_react3.useCallback)(() => {
        if (String(pathInput ?? "").trim() === "") {
          setPickerOpen(true);
          return;
        }
        openPath(pathInput);
      }, [openPath, pathInput]);
      const openChooser = (0, import_react3.useCallback)(() => {
        setPickerOpen(true);
      }, []);
      const closeChooser = (0, import_react3.useCallback)((finalDir = "") => {
        setPickerOpen(false);
        if (finalDir !== "") {
          setLastDir(finalDir);
          writeStored(LAST_DIR_KEY, finalDir);
        }
        browseButton.current?.focus?.();
      }, []);
      const pickFromChooser = (0, import_react3.useCallback)((picked) => {
        setPickerOpen(false);
        openPath(picked);
      }, [openPath]);
      const openObject = (0, import_react3.useCallback)((object2) => {
        if (document2 === null || object2 === void 0 || object2 === null) return;
        const query = typeof object2.guid === "string" && object2.guid !== "" ? object2.guid : object2.name;
        run({ path: document2.file.path, object: query }, (body) => {
          setDetail(body);
          setSelected({ name: object2.name, guid: object2.guid ?? "" });
          setModuleName("");
          const first = Array.isArray(body.object?.sections) && body.object.sections.length > 0 ? body.object.sections[0].kind : "";
          setActiveSection(first);
        });
      }, [document2, run]);
      const openModule = (0, import_react3.useCallback)((name) => {
        if (document2 === null || selected === null) return;
        const query = selected.guid !== "" ? selected.guid : selected.name;
        run({ path: document2.file.path, object: query, module: name }, (body) => {
          setDetail(body);
          setModuleName(name);
        });
      }, [document2, selected, run]);
      const reload = (0, import_react3.useCallback)(() => {
        if (document2 === null) {
          if (String(pathInput ?? "").trim() === "") return;
          openPath(pathInput);
          return;
        }
        if (selected === null) {
          openPath(document2.file.path);
          return;
        }
        if (moduleName !== "") {
          openModule(moduleName);
          return;
        }
        openObject(selected);
      }, [document2, moduleName, openModule, openObject, openPath, pathInput, selected]);
      (0, import_react3.useEffect)(() => {
        const actions = tab?.actions;
        if (actions === void 0 || typeof actions.bindCommands !== "function") return void 0;
        return actions.bindCommands({ refresh: reload });
      }, [tab, reload]);
      (0, import_react3.useEffect)(() => () => {
        controller.current?.abort();
      }, []);
      const object = detail?.object ?? null;
      const content = detail?.content ?? null;
      const modules = Array.isArray(object?.modules) ? object.modules : [];
      const sections = Array.isArray(object?.sections) ? object.sections : [];
      const active = sections.find((section) => section.kind === activeSection) ?? sections[0];
      const moduleText = typeof content?.text === "string" ? content.text : "";
      const moduleShown = moduleText.length > MODULE_LIMIT ? moduleText.slice(0, MODULE_LIMIT) : moduleText;
      const hasPath = String(pathInput ?? "").trim() !== "";
      return /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r" }, /* @__PURE__ */ import_react3.default.createElement("style", null, PANEL_CSS), /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-toolbar" }, /* @__PURE__ */ import_react3.default.createElement(
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
      ), /* @__PURE__ */ import_react3.default.createElement(
        "button",
        {
          type: "button",
          className: hasPath ? "v8r-button v8r-button--primary" : "v8r-button",
          disabled: busy || !hasPath,
          title: t("panel.open.title"),
          onClick: openFromToolbar
        },
        t("panel.open")
      ), /* @__PURE__ */ import_react3.default.createElement(
        "button",
        {
          type: "button",
          className: hasPath ? "v8r-button" : "v8r-button v8r-button--primary",
          title: t("panel.browse.title"),
          ref: browseButton,
          onClick: openChooser
        },
        t("panel.browse")
      ), document2 !== null && /* @__PURE__ */ import_react3.default.createElement(
        "button",
        {
          type: "button",
          className: "v8r-button",
          disabled: busy,
          onClick: reload
        },
        t("panel.reload")
      )), busy && /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-loading" }, t("panel.loading")), error !== null && /* @__PURE__ */ import_react3.default.createElement(ErrorBanner, { error, translate: t }), document2 === null && !busy && error === null && !pickerOpen && /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-empty" }, /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-empty-title" }, t("panel.empty.title")), /* @__PURE__ */ import_react3.default.createElement("div", null, t("panel.empty.hint")), /* @__PURE__ */ import_react3.default.createElement(
        "button",
        {
          type: "button",
          className: "v8r-button v8r-button--primary",
          onClick: openChooser
        },
        t("panel.chooseFile")
      )), document2 !== null && /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-body" }, /* @__PURE__ */ import_react3.default.createElement(
        Tree,
        {
          categories: document2.categories,
          selected,
          onSelect: openObject,
          translate: t
        }
      ), /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-main" }, /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-subtitle" }, t("panel.file.summary", {
        kind: fileKindLabel(t, document2.file?.kind),
        size: formatBytes(document2.file?.size),
        count: document2.file?.objectCount ?? 0
      }), document2.file?.formatVersion !== void 0 && /* @__PURE__ */ import_react3.default.createElement(import_react3.default.Fragment, null, " \xB7 ", t("panel.file.format", { version: document2.file.formatVersion })), Array.isArray(document2.file?.warnings) && document2.file.warnings.length > 0 && /* @__PURE__ */ import_react3.default.createElement(import_react3.default.Fragment, null, " \xB7 ", t("panel.warnings", { count: document2.file.warnings.length }))), object === null && /* @__PURE__ */ import_react3.default.createElement("p", { className: "v8r-muted" }, t("panel.selectObject")), object !== null && /* @__PURE__ */ import_react3.default.createElement(import_react3.default.Fragment, null, /* @__PURE__ */ import_react3.default.createElement("h3", { className: "v8r-title" }, object.name), /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-subtitle" }, object.typeTitle, object.synonym !== void 0 && object.synonym !== "" && /* @__PURE__ */ import_react3.default.createElement(import_react3.default.Fragment, null, " \xB7 ", object.synonym)), /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-tabs", role: "tablist" }, sections.map((section) => /* @__PURE__ */ import_react3.default.createElement(
        "button",
        {
          key: section.kind,
          type: "button",
          role: "tab",
          className: "v8r-tab",
          "aria-selected": active !== void 0 && section.kind === active.kind ? "true" : "false",
          onClick: () => {
            setActiveSection(section.kind);
          }
        },
        sectionLabel(t, section),
        " (",
        Array.isArray(section.entries) ? section.entries.length : 0,
        ")"
      ))), active !== void 0 && /* @__PURE__ */ import_react3.default.createElement(SectionTable, { section: active, translate: t }), modules.length > 0 && /* @__PURE__ */ import_react3.default.createElement(import_react3.default.Fragment, null, /* @__PURE__ */ import_react3.default.createElement("div", { className: "v8r-modules" }, /* @__PURE__ */ import_react3.default.createElement("span", { className: "v8r-muted" }, t("panel.modules"), ":"), modules.map((module2) => /* @__PURE__ */ import_react3.default.createElement(
        "button",
        {
          key: module2.name,
          type: "button",
          className: "v8r-button",
          "aria-pressed": module2.name === moduleName ? "true" : "false",
          disabled: busy,
          onClick: () => {
            openModule(module2.name);
          }
        },
        module2.name
      ))), moduleName !== "" && moduleText !== "" && /* @__PURE__ */ import_react3.default.createElement(import_react3.default.Fragment, null, /* @__PURE__ */ import_react3.default.createElement("pre", { className: "v8r-code" }, moduleShown), moduleText.length > MODULE_LIMIT && /* @__PURE__ */ import_react3.default.createElement("p", { className: "v8r-muted" }, t("panel.module.truncated", {
        size: MODULE_LIMIT,
        total: moduleText.length
      }))))))), pickerOpen && /* @__PURE__ */ import_react3.default.createElement(
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
      "panel.module.truncated": "\u041F\u043E\u043A\u0430\u0437\u0430\u043D\u044B \u043F\u0435\u0440\u0432\u044B\u0435 {size} \u0441\u0438\u043C\u0432\u043E\u043B\u043E\u0432 \u0438\u0437 {total}.",
      "panel.column.name": "\u0418\u043C\u044F",
      "panel.column.synonym": "\u0421\u0438\u043D\u043E\u043D\u0438\u043C",
      "panel.column.type": "\u0422\u0438\u043F",
      "panel.warnings": "\u041F\u0440\u0435\u0434\u0443\u043F\u0440\u0435\u0436\u0434\u0435\u043D\u0438\u044F: {count}",
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
      "error.unknown": "\u041D\u0435\u0438\u0437\u0432\u0435\u0441\u0442\u043D\u0430\u044F \u043E\u0448\u0438\u0431\u043A\u0430.",
      "error.hint.cli-missing": "\u0421\u043E\u0431\u0435\u0440\u0438\u0442\u0435 v8reader_cli (cmake --build out/mingw) \u0438 \u0440\u0430\u0437\u043B\u043E\u0436\u0438\u0442\u0435 \u0435\u0433\u043E windeployqt \u0432 out/cli-deploy, \u043B\u0438\u0431\u043E \u0437\u0430\u0434\u0430\u0439\u0442\u0435 cliPath \u0432 \u043D\u0430\u0441\u0442\u0440\u043E\u0439\u043A\u0430\u0445 \u043F\u043B\u0430\u0433\u0438\u043D\u0430 \u0438\u043B\u0438 \u043F\u0435\u0440\u0435\u043C\u0435\u043D\u043D\u0443\u044E V8READER_CLI.",
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
      "panel.module.truncated": "Showing the first {size} characters of {total}.",
      "panel.column.name": "Name",
      "panel.column.synonym": "Synonym",
      "panel.column.type": "Type",
      "panel.warnings": "Warnings: {count}",
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
      "error.unknown": "Unknown error.",
      "error.hint.cli-missing": "Build v8reader_cli (cmake --build out/mingw) and deploy it with windeployqt into out/cli-deploy, or set the plugin row cliPath or the V8READER_CLI environment variable.",
      "error.details": "Tool output"
    };
    
    // src/client/index.jsx
    var TYPE_KIND = "v8reader";
    var TYPE_ID = "@fishca/dsh-v8reader";
    var LOCALE_NAMESPACE = "v8reader";
    var inject = ["slots", "locale", "sidebarRightTabs"];
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
      ctx.effect(
        () => ctx.slots.inject("sidebar.right.pane.tab", () => ctx.slots.register(
          { name: "sidebar.right.pane.tab", key: TYPE_ID, locale: LOCALE_NAMESPACE },
          PanelView
        )),
        "dsh-v8reader: tab body"
      );
    }
    
    return module.exports;
  },
});
