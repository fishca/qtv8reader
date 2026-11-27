/**
 * The panel's stylesheet.
 *
 * Rendered as a `<style>` element inside the panel, so it exists exactly while
 * the panel is mounted and its class names cannot leak into the host UI. Colours
 * come from the `--dsw-*` design tokens, so light and dark themes follow the app.
 */
export const PANEL_CSS = `
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
.v8r-main[data-view="module"],.v8r-main[data-view="form"],.v8r-main[data-view="template"]{display:flex;flex-direction:column;overflow:hidden}
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
.v8r-mxl{position:relative;flex:1 1 auto;min-height:280px;border:1px solid var(--dsw-alias-border-l1);border-radius:8px;overflow:hidden;background:var(--dsw-alias-bg-layer-1)}
.v8r-mxl-frame{display:block;width:100%;height:100%;border:0;background:var(--dsw-alias-bg-base)}
.v8r-mxl[data-status="error"] .v8r-mxl-frame{visibility:hidden}
.v8r--document .v8r-document-wait{padding:12px 16px}
.v8r--document .v8r-tree{flex:0 0 min(260px,42%)}
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
`
