/**
 * The panel's stylesheet.
 *
 * Rendered as a `<style>` element inside the panel, so it exists exactly while
 * the panel is mounted and its class names cannot leak into the host UI. Colours
 * come from the `--dsw-*` design tokens, so light and dark themes follow the app.
 */
export const PANEL_CSS = `
.v8r{display:flex;flex-direction:column;height:100%;min-height:0;color:var(--dsw-alias-label-primary);background:var(--dsw-alias-bg-base);font-size:13px;line-height:1.45}
.v8r-toolbar{display:flex;gap:8px;align-items:center;padding:8px 12px;border-bottom:1px solid var(--dsw-alias-border-l1);flex:0 0 auto}
.v8r-input{flex:1 1 auto;min-width:0;height:28px;padding:0 8px;border-radius:6px;border:1px solid var(--dsw-alias-border-l2);background:var(--dsw-alias-bg-layer-1);color:inherit;font:inherit;outline:none}
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
.v8r-empty{display:flex;flex-direction:column;gap:6px;align-items:flex-start;color:var(--dsw-alias-label-secondary);padding:16px 4px;max-width:520px}
.v8r-empty-title{color:var(--dsw-alias-label-primary);font-weight:600}
.v8r-banner{margin:8px 12px;padding:8px 10px;border-radius:8px;border:1px solid var(--dsw-alias-state-error-primary);color:var(--dsw-alias-state-error-primary);background:var(--dsw-alias-bg-layer-1);font-size:12.5px;flex:0 0 auto}
.v8r-banner-title{font-weight:600;margin-bottom:2px}
.v8r-banner-detail{margin-top:6px;white-space:pre-wrap;word-break:break-word;font-family:ui-monospace,SFMono-Regular,Consolas,monospace;font-size:11.5px;color:var(--dsw-alias-label-secondary)}
.v8r-loading{padding:8px 12px;color:var(--dsw-alias-label-secondary);font-size:12.5px;flex:0 0 auto}
.v8r-children{padding-left:14px}
`
