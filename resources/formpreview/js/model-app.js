/* Renders a 1C managed-form preview from a JSON model alone.
 *
 * No XML, no configuration dump, no FormContext: the model already contains
 * everything FormPreview.render needs (items, properties, commands, the
 * owner's parsed metadata, referenced metadata, style items and common
 * pictures as base64 data).
 *
 * Sources, in order of precedence:
 *   1. window.__QV8_MODEL__  (the C++ host may inject it before this script)
 *   2. ?model=<url>          (default: model.json next to this page)
 *
 * The optional ?title= overrides the window caption; otherwise the renderer
 * uses model.properties.Title, exactly as it does for XML-parsed forms.
 */
(function () {
    'use strict';

    var params = new URLSearchParams(window.location.search);
    var modelUrl = params.get('model') || 'model.json';
    var titleOverride = params.get('title') || '';
    var host = document.getElementById('host');
    var errorBox = document.getElementById('qv8-error');
    var startedAt = Date.now();

    function setState(state) {
        document.documentElement.setAttribute('data-qv8-state', state);
        window.__QV8_STATE = state;
    }

    function fail(error) {
        var text = String(error && error.stack || error);
        setState('error');
        window.__QV8_ERROR = text;
        console.error('[qv8-model] FAILED', error);
        if (errorBox) {
            errorBox.hidden = false;
            errorBox.textContent = text;
        }
    }

    function loadModel() {
        if (window.__QV8_MODEL__) {
            console.log('[qv8-model] using window.__QV8_MODEL__');
            return Promise.resolve(window.__QV8_MODEL__);
        }
        console.log('[qv8-model] loading ' + modelUrl);
        return fetch(modelUrl, { cache: 'no-store' }).then(function (response) {
            if (!response.ok) throw new Error('HTTP ' + response.status + ' for ' + modelUrl);
            return response.json();
        });
    }

    /* parse() attaches the form version to every item as a non-enumerable
     * `_fpFormVersion`. JSON keeps only enumerable own properties, so a host
     * that loads a serialised model must restore it; 8.5 layout decisions read
     * it through formVersionAtLeast(). */
    function attachFormVersion(item, version) {
        if (!item || typeof item !== 'object') return;
        Object.defineProperty(item, '_fpFormVersion', {
            value: version, configurable: true, writable: true
        });
        if (item.autoCommandBar) attachFormVersion(item.autoCommandBar, version);
        var children = item.childItems || [];
        for (var i = 0; i < children.length; i++) attachFormVersion(children[i], version);
    }

    function restoreModelRuntime(model) {
        if (!model || !model.version) return;
        attachFormVersion(model, model.version);
        var roots = model.childItemsRoot || [];
        for (var i = 0; i < roots.length; i++) attachFormVersion(roots[i], model.version);
        if (model.autoCommandBar) attachFormVersion(model.autoCommandBar, model.version);
    }

    /* The renderer paints its own glyphs (field choice/open buttons, command
     * bar arrows, table sorters) as <svg><use href="#i-…"></svg>. The shapes
     * live in an icon sprite, and a host page that does not carry it paints
     * empty squares: the platform's ▾, «Открыть» and «…» buttons, the clear
     * cross all disappear. icons.svg is loaded once, before the first paint,
     * so every <use> resolves. A missing sprite keeps the old behaviour
     * (invisible glyphs) instead of failing the preview. */
    function loadIconSprite() {
        if (document.getElementById('i-px-caret-down'))
            return Promise.resolve(true);
        return fetch('icons.svg', { cache: 'force-cache' })
            .then(function (response) {
                if (!response.ok) throw new Error('HTTP ' + response.status);
                return response.text();
            })
            .then(function (text) {
                var holder = document.createElement('div');
                holder.id = 'qv8-icon-sprite';
                holder.innerHTML = text;
                document.body.appendChild(holder);
                return true;
            })
            .catch(function (error) {
                console.warn('[qv8-model] icon sprite unavailable', error);
                return false;
            });
    }

    function nextFrame() {
        return new Promise(function (resolve) {
            requestAnimationFrame(function () { requestAnimationFrame(resolve); });
        });
    }

    /* rAF does not run in some headless/dump contexts, so the ready gate uses
     * a macrotask; a frame callback is still scheduled for the final reflow. */
    function nextTask(ms) {
        return new Promise(function (resolve) { setTimeout(resolve, ms || 0); });
    }

    /* Optional demonstration: `?tab=Товары` clicks the authored page tab after
     * the first paint.  The renderer keeps the active page in module state, so
     * a click is the supported way to show another page of the same model. */
    async function activateTab(title) {
        if (!title) return false;
        var tabs = host.querySelectorAll('.fp-pages-tab');
        for (var i = 0; i < tabs.length; i++) {
            var label = tabs[i].querySelector('.fp-page-tab-title');
            if ((label ? label.textContent : tabs[i].textContent).trim() !== title) continue;
            tabs[i].click();
            await nextFrame();
            return true;
        }
        return false;
    }

    async function main() {
        setState('loading');
        var model = await loadModel();
        if (!model || !model.tag) throw new Error('JSON не похож на модель формы: нет поля tag');
        await loadIconSprite();
        restoreModelRuntime(model);
        /* The XML viewer fills item.runtime (typeFeatures, inputButtons,
         * defaultChars) while parsing the form. A serialised model carries
         * only authored properties, so the same derivation must run here or
         * every field falls back to the 20-character default width and loses
         * its choice buttons. */
        if (FormPreview.applyTypeDefaultsToModel)
            FormPreview.applyTypeDefaultsToModel(model);

        var renderStarted = performance.now();
        FormPreview.render(model, host, {
            windowTitle: titleOverride,
            filePath: ''
        });
        FormPreview.reflow(host);
        await nextTask(0);
        var tab = params.get('tab') || '';
        var tabActivated = await activateTab(tab);
        FormPreview.reflow(host);
        await nextTask(0);
        var renderMs = performance.now() - renderStarted;

        var items = host.querySelectorAll('.fp-item').length;
        var resources = performance.getEntriesByType('resource').map(function (entry) {
            return {
                url: entry.name.replace(window.location.origin, ''),
                initiatorType: entry.initiatorType,
                transferSize: entry.transferSize || 0
            };
        });
        var info = {
            state: 'ready',
            title: titleOverride || (model.properties && model.properties.Title) || '',
            renderMs: Math.round(renderMs),
            totalMs: Date.now() - startedAt,
            items: items,
            tab: tabActivated ? tab : '',
            modelBytes: JSON.stringify(model).length,
            modelUrl: window.__QV8_MODEL__ ? '(inline)' : modelUrl,
            resources: resources.length
        };
        window.__QV8_INFO = info;
        window.__QV8_MODEL = model;
        window.__QV8_RESOURCES = resources;
        window.__QV8_READY = true;
        document.title = info.title || 'model';
        setState('ready');
        console.log('[qv8-model] READY ' + JSON.stringify(info));
        nextFrame().then(function () { FormPreview.reflow(host); });
        if (params.get('dump') === '1') {
            document.body.setAttribute('data-qv8-info', JSON.stringify(info));
            var dump = document.createElement('script');
            dump.type = 'application/json';
            dump.id = 'qv8-resources';
            dump.textContent = JSON.stringify(resources);
            document.body.appendChild(dump);
        }
    }

    main().catch(fail);
})();
