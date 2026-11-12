// SPDX-License-Identifier: GPL-2.0-or-later
// Run with: node tests/TilingScriptTest.js
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../scripts/material-tiling/contents/code/main.js'), 'utf8');
function quickTile(tile, edges = 2) {
    const parent = {tiles: [2, 4, 1, 8, 3, 5, 10, 12].map(mode => mode === edges ? tile : {})};
    if (!tile.parent) tile.parent = parent;
    return tile;
}
function fixture(extraWindows = []) {
    const output = {name: 'Virtual-1', geometry: {x: 0, y: 0, width: 1280, height: 800}};
    const other = {internalId: '{other}', caption: 'other'};
    const desktop = {id: 'desktop-1'};
    const target = {internalId: '{target}', resizeable: true, specialWindow: false, normalWindow: true,
        output, tile: null, desktops: [desktop], frameGeometry: {x: 0, y: 0, width: 640, height: 480},
        setMaximize(vertical, horizontal) { this.maximizeMode = Number(vertical) | (Number(horizontal) << 1); }};
    Object.defineProperty(target, 'onAllDesktops', {
        get() { return this.desktops.length === 0; },
        set(value) { this.desktops = value ? [] : [workspace.currentDesktop]; }
    });
    function leaf(x, y, width, height) {
        return {relativeGeometry: {x, y, width, height}, minimumSize: {width: .15, height: .15}, tiles: [], manage(window) { window.tile = this; return true; }, unmanage(window) { window.tile = null; }};
    }
    const root = {tiles: [leaf(0, 0, 1 / 3, 1), leaf(1 / 3, 0, 1 / 3, 1), leaf(2 / 3, 0, 1 / 3, 1)]};
    const workspace = {activeWindow: other, layoutRoot: root, screens: [output], desktops: [desktop], currentDesktop: desktop,
        clientArea(option, screen) { this.lastClientAreaOutput = screen; return screen.geometry; },
        tilingForScreen() { return {rootTile: this.layoutRoot, quickTiles: this.quickTiles || []}; },
        rootTile() { return this.layoutRoot; },
        windows: [target, other, ...extraWindows], windowList() { return this.windows; },
        windowAdded: {connect(callback) { this.callback = callback; }},
        windowRemoved: {connect(callback) { this.callback = callback; }},
        sendClientToScreen(window, screen) { window.output = screen; }};
    const calls = [];
    for (const name of ['Left', 'Right', 'Top', 'Bottom', 'TopLeft', 'TopRight', 'BottomLeft', 'BottomRight']) {
        workspace['slotWindowQuickTile' + name] = () => {
            calls.push({name, target: workspace.activeWindow});
            workspace.activeWindow.tile = {unmanage(window) { window.tile = null; }};
        };
    }
    const context = {workspace, materialWindowStateBridge: {quickTiles(manager) { return manager.quickTiles; }}, KWin: {PlacementArea: 0}};
    vm.runInNewContext(source, context);
    return {workspace, target, other, calls, output, root, leaf,
        restoreImmediately(state) { return context.materialRestoreWindowState(target, JSON.stringify(state)); },
        prepareImmediately(state) { return context.materialPrepareWindowState(target, JSON.stringify(state)); },
        send(edges, uuid = 'target', operation = 'tile', extra = {}) {
            const target = workspace.windowList().find(window => String(window.internalId).replace(/[{}]/g, '') === uuid);
            try {
                if (operation === 'restoreState') return {success: context.materialRestoreWindowState(target, JSON.stringify(extra.state))};
                const result = context.materialTiling(target, JSON.stringify({edges, operation: operation, ...extra}));
                return typeof result === 'string' ? {success: true, layout: JSON.parse(result)} : {success: result};
            } catch (error) { return {success: false, reason: String(error)}; }
        }};
}
{
    const caption = 'com.github.guiodic.material-decoration.popup';
    const existing = {caption, popupWindow: true, skipSwitcher: false};
    const unrelated = {caption: 'Another popup', popupWindow: true, skipSwitcher: false};
    const normal = {caption, popupWindow: false, skipSwitcher: false};
    const f = fixture([existing, unrelated, normal]);
    assert.equal(existing.skipSwitcher, true, 'handle menus already open when the script starts');
    assert.equal(unrelated.skipSwitcher, false);
    assert.equal(normal.skipSwitcher, false);
    const added = {caption, popupWindow: true, skipSwitcher: false};
    f.workspace.windowAdded.callback(added);
    assert.equal(added.skipSwitcher, true, 'exclude newly opened Material popups');
    f.workspace.windowAdded.callback(unrelated);
    f.workspace.windowAdded.callback(normal);
    assert.equal(unrelated.skipSwitcher, false);
    assert.equal(normal.skipSwitcher, false);
    assert.equal(f.workspace.activeWindow, f.other);
    assert.equal(f.calls.length, 0);
}
// Model KWin's immediate shared-layout updates, including minimum-size clamping.
function quickLayoutFixture(f) {
    const layout = {tiles: [], minimumSize: {width: .15, height: .15}, writes: []};
    const initial = [[0,0,.5,1],[.5,0,.5,1],[0,0,1,.5],[0,.5,1,.5],
        [0,0,.5,.5],[.5,0,.5,.5],[0,.5,.5,.5],[.5,.5,.5,.5]];
    let updating = false;
    layout.tiles = initial.map(values => f.leaf(...values));
    layout.tiles.forEach((tile, index) => {
        tile.parent = layout;
        const live = {...tile.relativeGeometry};
        Object.defineProperty(tile, 'relativeGeometry', {get() {return live;}, set(value) {
            layout.writes.push({index, value: {...value}});
            Object.assign(live, value, {width: Math.max(value.width, tile.minimumSize.width), height: Math.max(value.height, tile.minimumSize.height)});
            if (updating) return;
            updating = true;
            try {
                const horizontal = [0,4,6].includes(index) ? live.x + live.width : [1,5,7].includes(index) ? live.x : null;
                const vertical = [2,4,5].includes(index) ? live.y + live.height : [3,6,7].includes(index) ? live.y : null;
                if (horizontal !== null) {
                    const cut = Math.max(layout.minimumSize.width, Math.min(1-layout.minimumSize.width, horizontal));
                    for (const i of [0,4,6]) { const r = layout.tiles[i].relativeGeometry; layout.tiles[i].relativeGeometry = {...r, width: cut-r.x}; }
                    for (const i of [1,5,7]) { const r = layout.tiles[i].relativeGeometry; layout.tiles[i].relativeGeometry = {...r, x: cut, width: r.x+r.width-cut}; }
                }
                if (vertical !== null) {
                    const cut = Math.max(layout.minimumSize.height, Math.min(1-layout.minimumSize.height, vertical));
                    for (const i of [2,4,5]) { const r = layout.tiles[i].relativeGeometry; layout.tiles[i].relativeGeometry = {...r, height: cut-r.y}; }
                    for (const i of [3,6,7]) { const r = layout.tiles[i].relativeGeometry; layout.tiles[i].relativeGeometry = {...r, y: cut, height: r.y+r.height-cut}; }
                }
            } finally { updating = false; }
        }});
    });
    f.workspace.quickTiles = layout.tiles;
    return layout;
}
for (const [edges, expected] of [[2,'Left'],[4,'Right'],[1,'Top'],[8,'Bottom'],[3,'TopLeft'],[5,'TopRight'],[10,'BottomLeft'],[12,'BottomRight']]) {
    const f = fixture();
    assert.ok(f.send(edges).success);
    assert.equal(f.calls[0].name, expected);
    assert.equal(f.calls[0].target, f.target);
    assert.equal(f.other.tile, undefined);
    assert.ok(f.send(edges).success);
    assert.equal(f.calls.length, 1, 'same tile must not toggle or change monitor');
}
{
    const f = fixture();
    assert.ok(f.send(0, 'target', 'layout').success);
    assert.equal(f.calls.length, 0);
    assert.equal(f.workspace.activeWindow, f.other, 'availability check must not change focus');
    assert.ok(!f.send(2, 'closed-window').success);
    assert.equal(f.calls.length, 0);
    f.target.resizeable = false;
    assert.ok(!f.send(2).success);
    f.target.resizeable = true;
    assert.ok(!f.send(6).success);
    assert.equal(f.calls.length, 0);
}
{
    const f = fixture();
    const oldOutput = {};
    f.target.tile = {unmanage(window) { window.tile = null; window.output = oldOutput; }};
    assert.ok(f.send(4).success);
    assert.equal(f.target.output, f.output, 'untile must not move the request to the restore monitor');
    f.workspace.windowRemoved.callback(f.target);
    f.workspace.windows = [f.other];
    assert.ok(!f.send(2).success);
    assert.equal(f.calls.length, 1);
}
{
    const f = fixture();
    const before = JSON.stringify(f.root.tiles.map(tile => tile.relativeGeometry));
    const result = f.send(0, 'target', 'layout');
    assert.equal(result.layout.length, 3);
    assert.equal(f.workspace.activeWindow, f.other, 'reading the layout must not steal focus');
    assert.equal(f.target.tile, null);
    for (let i = 0; i < 3; ++i) {
        assert.deepEqual(result.layout[i], {id: result.layout[i].id, ...f.root.tiles[i].relativeGeometry});
        assert.ok(f.send(0, 'target', 'tileCustom', {tileId: result.layout[i].id}).success);
        assert.equal(f.target.tile, f.root.tiles[i]);
    }
    assert.ok(f.send(0, 'target', 'tileCustom', {tileId: result.layout[2].id}).success);
    assert.equal(f.target.tile, f.root.tiles[2]);
    assert.equal(JSON.stringify(f.root.tiles.map(tile => tile.relativeGeometry)), before, 'selecting a custom tile must not edit the layout');
    assert.equal(f.calls.length, 0, 'custom selection must not invoke a quick-tile shortcut');
    assert.ok(f.send(2).success, 'quick tiling still works after custom tiling');
}
{
    const f = fixture();
    f.root.tiles = [f.leaf(0, 0, 0.5, 1), {tiles: [f.leaf(0.5, 0, 0.5, 0.5), f.leaf(0.5, 0.5, 0.5, 0.5)]}];
    const result = f.send(0, 'target', 'layout');
    assert.equal(result.layout.length, 3, 'only leaf tiles should become popup zones');
    assert.equal(result.layout[2].y, 0.5);
    assert.ok(f.send(0, 'target', 'tileCustom', {tileId: result.layout[2].id}).success);
    assert.equal(f.target.tile, f.root.tiles[1].tiles[1]);
}
for (const change of [
    f => { f.root.tiles[1].relativeGeometry.width = 0.2; },
    f => { f.root.tiles[0] = f.leaf(0, 0, 1 / 3, 1); },
    f => { f.root.tiles.pop(); },
    f => { f.target.output = {}; },
    f => { f.workspace.layoutRoot = {tiles: f.root.tiles}; },
    f => { f.send(0, 'target', 'layout'); }
]) {
    const f = fixture();
    assert.ok(f.send(2).success);
    const previousTile = f.target.tile;
    const result = f.send(0, 'target', 'layout');
    change(f);
    assert.ok(!f.send(0, 'target', 'tileCustom', {tileId: result.layout[0].id}).success);
    assert.equal(f.target.tile, previousTile, 'a stale selection must leave the previous tile untouched');
    const refreshed = f.send(0, 'target', 'layout');
    assert.ok(f.send(0, 'target', 'tileCustom', {tileId: refreshed.layout[0].id}).success,
        'reopening the popup must allow selection from the current layout');
}
{
    const f = fixture();
    assert.ok(!f.send(0, 'target', 'tileCustom', {tileId: 'invented'}).success);
    delete f.workspace.tilingForScreen;
    assert.deepEqual(f.send(0, 'target', 'layout').layout, []);
    assert.ok(f.send(2).success, 'native quick tiling remains available without the custom-layout API');
}
{
    const f = fixture();
    const result = f.send(0, 'target', 'layout');
    let active = f.other;
    Object.defineProperty(f.workspace, 'activeWindow', {
        get() { return active; },
        set(window) { active = window; f.workspace.layoutRoot = {tiles: f.root.tiles}; }
    });
    assert.ok(!f.send(0, 'target', 'tileCustom', {tileId: result.layout[0].id}).success,
        'activation switching the desktop must invalidate the old layout');
    assert.equal(f.target.tile, null);
}
function savedState(extra = {}) {
    return {version: 3, position: {x: 100, y: 80}, desktopIds: ['desktop-1'],
        rememberScreen: true, rememberDesktop: true,
        onAllDesktops: false, outputName: 'Virtual-1',
        outputPosition: {x: 0, y: 0}, ...extra};
}
{
    const f = fixture();
    const destination = {id: 'desktop-2'};
    f.workspace.desktops.push(destination);
    const state = savedState({desktopIds: ['desktop-2']});
    assert.equal(f.prepareImmediately(state), true);
    assert.equal(f.target.desktops[0], destination, 'assign the saved desktop before workspace.windowAdded');
    assert.equal(f.target.minimized, true, 'prevent initial activation from bringing the window to the current desktop');
    // Simulate KWin's "bring to current desktop" activation policy.
    if (!f.target.minimized) {
        f.target.desktops = [f.workspace.currentDesktop];
        f.workspace.activeWindow = f.target;
    }
    assert.equal(f.restoreImmediately(state), true);
    f.target.minimized = false; // The C++ controller releases its startup guard.
    assert.equal(f.target.desktops[0], destination);
    assert.equal(f.workspace.currentDesktop.id, 'desktop-2');
    assert.equal(f.workspace.activeWindow, f.target);
}
for (const extra of [{}, {desktopIds: ['removed']}, {onAllDesktops: true}]) {
    const f = fixture();
    f.target.minimized = false;
    assert.equal(f.prepareImmediately(savedState(extra)), true);
    assert.equal(f.target.minimized, false, 'no guard for current/all/missing desktops');
}
{
    const f = fixture();
    f.target.setMaximize = () => { throw new Error('must not alter app-requested modes'); };
    f.workspace.sendClientToScreen = () => { throw new Error('must not constrain a normal window'); };
    assert.equal(f.restoreImmediately(savedState()), true);
    assert.deepEqual({...f.target.frameGeometry}, {x: 100, y: 80, width: 640, height: 480});
    assert.equal(f.workspace.activeWindow, f.target);
    assert.equal(f.calls.length, 0);
    assert.throws(() => f.restoreImmediately(savedState({version: 4})), /Invalid saved position/);
}
{
    const f = fixture();
    let current = {...f.target.frameGeometry};
    Object.defineProperty(f.target, 'frameGeometry', {
        get() { return {...current}; },
        set(g) {
            assert.equal(g.width, current.width, 'never request another width');
            assert.equal(g.height, current.height, 'never request another height');
            current = {...g};
        }
    });
    // Even unexpected old dimension/mode fields must not affect the app size.
    assert.equal(f.restoreImmediately(savedState({geometry: {width: 900, height: 600}, maximizeMode: 3, fullScreen: true})), true);
    assert.deepEqual(current, {x: 100, y: 80, width: 640, height: 480});
    assert.equal(f.target.fullScreen, undefined);
}
{
    const f = fixture();
    const secondDesktop = {id: 'desktop-2'};
    f.workspace.desktops.push(secondDesktop);
    assert.ok(f.send(0, 'target', 'restoreState', {state: savedState({desktopIds: ['desktop-2']})}).success);
    assert.deepEqual({...f.target.frameGeometry}, {x: 100, y: 80, width: 640, height: 480});
    assert.equal(f.target.desktops[0], secondDesktop);
    assert.equal(f.workspace.activeWindow, f.target);
    assert.equal(f.workspace.currentDesktop.id, 'desktop-2');
}
{
    const f = fixture();
    const movedOutput = {name: 'Virtual-1', geometry: {x: 1920, y: 50, width: 800, height: 600}};
    f.workspace.screens = [movedOutput];
    assert.ok(f.send(0, 'target', 'restoreState', {state: savedState()}).success);
    assert.equal(f.workspace.lastClientAreaOutput, movedOutput);
    assert.deepEqual({...f.target.frameGeometry}, {x: 2020, y: 130, width: 640, height: 480});
    const tinyOutput = {name: 'Virtual-1', geometry: {x: 1920, y: 50, width: 400, height: 300}};
    f.workspace.screens = [tinyOutput];
    assert.ok(f.send(0, 'target', 'restoreState', {state: savedState()}).success);
    assert.deepEqual({...f.target.frameGeometry}, {x: 1920, y: 50, width: 640, height: 480}, 'a smaller output must not shrink the app');
}
{
    const f = fixture();
    f.target.resizeable = false;
    assert.ok(f.send(0, 'target', 'restoreState', {state: savedState({desktopIds: ['removed'], outputName: 'removed'})}).success);
    assert.deepEqual({...f.target.frameGeometry}, {x: 100, y: 80, width: 640, height: 480});
    assert.equal(f.target.desktops[0], f.workspace.currentDesktop);
    assert.equal(f.workspace.lastClientAreaOutput, f.output);
}
for (const mode of ['maximized', 'fullscreen', 'tiled']) {
    const f = fixture();
    const original = {...f.target.frameGeometry};
    if (mode === 'maximized') f.target.maximizeMode = 3;
    if (mode === 'fullscreen') f.target.fullScreen = true;
    if (mode === 'tiled') f.target.tile = {unmanage() { throw new Error('must not untile'); }};
    f.target.setMaximize = () => { throw new Error('must not maximize or restore'); };
    const tile = f.target.tile;
    const destination = {name: 'Virtual-2', geometry: {x: 1280, y: 0, width: 1280, height: 800}};
    f.workspace.screens.push(destination);
    assert.ok(f.send(0, 'target', 'restoreState', {state: savedState({outputName: 'Virtual-2', onAllDesktops: true})}).success);
    assert.deepEqual({...f.target.frameGeometry}, original);
    assert.equal(f.target.output, destination);
    assert.equal(f.target.onAllDesktops, true);
    assert.equal(f.target.tile, tile);
    if (mode === 'maximized') assert.equal(f.target.maximizeMode, 3);
    if (mode === 'fullscreen') assert.equal(f.target.fullScreen, true);
}
{
    const f = fixture();
    for (const state of [null, savedState({version: 1}), savedState({position: {x: NaN, y: 0}}),
        savedState({position: {x: 0}}), savedState({desktopIds: null})]) {
        assert.ok(!f.send(0, 'target', 'restoreState', {state}).success);
    }
    assert.deepEqual({...f.target.frameGeometry}, {x: 0, y: 0, width: 640, height: 480});
    f.target.normalWindow = false;
    assert.ok(!f.send(0, 'target', 'restoreState', {state: savedState()}).success);
}
{
    const f = fixture();
    const wrongDesktop = {relativeGeometry: {x: 0, y: 0, width: 0.5, height: 1}, manage() { return false; }};
    const quick = f.leaf(0, 0, 0.5, 1);
    f.workspace.quickTiles = [quickTile(wrongDesktop), quickTile(quick)];
    assert.equal(f.restoreImmediately(savedState({tile: {kind: 'quick', edges: 2}})), true);
    assert.equal(f.target.tile, quick, 'restore a native quick tile, not just its coordinates');
    assert.equal(f.workspace.activeWindow, f.target);
    assert.equal(f.calls.length, 0, 'no activation or global quick-tile shortcut');
}
// Native manage() accepts any root for sticky/multi-desktop windows. Restrict
// membership during the call so an inactive desktop cannot win by enumeration.
function desktopTile(f, desktop) {
    const tile = f.leaf(0, 0, 0.5, 1);
    tile.attempts = 0;
    tile.manage = function (window) {
        ++this.attempts;
        if (!window.onAllDesktops && !window.desktops.some(d => d.id === desktop.id)) return false;
        this.owner = window;
        // KWin records inactive membership but requests geometry only when active.
        const current = f.workspace.currentDesktopForScreen ? f.workspace.currentDesktopForScreen(window.output) : f.workspace.currentDesktop;
        if (desktop === current) window.tile = this;
        return true;
    };
    return tile;
}
for (const allDesktops of [true, false]) {
    const f = fixture();
    const first = f.workspace.currentDesktop, second = {id: 'desktop-2'};
    f.workspace.desktops.push(second);
    f.workspace.currentDesktop = second;
    const inactive = desktopTile(f, first), active = desktopTile(f, second);
    f.workspace.quickTiles = [quickTile(inactive), quickTile(active)];
    f.restoreImmediately(savedState({onAllDesktops: allDesktops, desktopIds: allDesktops ? [] : [first.id, second.id], tile: {kind: 'quick', edges: 2}}));
    assert.equal(f.target.tile, active, 'bind the root of the current desktop, even when another root is enumerated first');
    assert.equal(active.attempts, 1);
    assert.equal(f.target.onAllDesktops, allDesktops);
    assert.deepEqual(Array.from(f.target.desktops), allDesktops ? [] : [first, second]);
    assert.equal(f.workspace.currentDesktop, second);
    assert.equal(f.workspace.activeWindow, f.target);
    assert.equal(f.calls.length, 0);
}
{
    const f = fixture();
    const first = f.workspace.currentDesktop, second = {id: 'desktop-2'}, third = {id: 'desktop-3'};
    f.workspace.desktops.push(second, third);
    f.workspace.currentDesktop = third;
    const wrong = desktopTile(f, second), destination = desktopTile(f, first);
    f.workspace.quickTiles = [quickTile(wrong), quickTile(destination)];
    f.restoreImmediately(savedState({desktopIds: [first.id, second.id], tile: {kind: 'quick', edges: 2}}));
    assert.equal(wrong.owner, undefined);
    assert.equal(destination.owner, f.target);
    assert.deepEqual(Array.from(f.target.desktops), [first, second]);
    assert.equal(f.workspace.currentDesktop, first);
    assert.equal(f.workspace.activeWindow, f.target);
}
{
    const f = fixture();
    const globalDesktop = f.workspace.currentDesktop, outputDesktop = {id: 'desktop-2'};
    f.workspace.desktops.push(outputDesktop);
    f.workspace.currentDesktopForScreen = output => { assert.equal(output, f.output); return outputDesktop; };
    const inactive = desktopTile(f, globalDesktop), active = desktopTile(f, outputDesktop);
    f.workspace.quickTiles = [quickTile(inactive), quickTile(active)];
    f.restoreImmediately(savedState({onAllDesktops: true, desktopIds: [], tile: {kind: 'quick', edges: 2}}));
    assert.equal(f.target.tile, active, 'use the output desktop when desktops differ across screens');
    assert.equal(f.target.onAllDesktops, true);
    assert.equal(f.workspace.currentDesktop, globalDesktop);
}
for (const behavior of ['missing-tile', 'throws', 'rule-rejects-restriction']) {
    const f = fixture();
    const tile = f.leaf(0, 0, 0.5, 1);
    f.workspace.quickTiles = behavior === 'missing-tile' ? [] : [quickTile(tile)];
    if (behavior === 'throws') tile.manage = () => { throw new Error('tile disappeared'); };
    if (behavior === 'rule-rejects-restriction') {
        let desktops = [];
        Object.defineProperty(f.target, 'desktops', {get() { return desktops; }, set() { desktops = []; }});
        tile.manage = () => { throw new Error('must not use a root without a desktop restriction'); };
    }
    const state = savedState({onAllDesktops: true, desktopIds: [], tile: {kind: 'quick', edges: 2}});
    if (behavior === 'throws') assert.throws(() => f.restoreImmediately(state), /tile disappeared/);
    else assert.equal(f.restoreImmediately(state), false);
    assert.equal(f.target.onAllDesktops, true, 'restore sticky membership even after failure');
    assert.deepEqual(Array.from(f.target.desktops), []);
    assert.equal(f.workspace.activeWindow, behavior === 'throws' ? f.other : f.target);
}

{
    const f = fixture();
    const region = {x: 1 / 3, y: 0, width: 1 / 3, height: 1};
    assert.equal(f.restoreImmediately(savedState({tile: {kind: 'custom', path: [1], region}})), true);
    assert.equal(f.target.tile, f.root.tiles[1]);
    assert.equal(f.workspace.activeWindow, f.target);
    const changed = fixture();
    assert.equal(changed.restoreImmediately(savedState({tile: {kind: 'custom', path: [1], region: {...region, width: 0.9}}})), false);
    assert.equal(changed.target.tile, null, 'changed layouts must fall back to floating placement');
    assert.equal(changed.target.frameGeometry.width, 640);
}
for (const kind of ['quick', 'custom']) {
    for (const behavior of ['missing', 'rejected', 'throws']) {
        const f = fixture();
        const tile = kind === 'quick' ? f.leaf(0, 0, 0.5, 1) : f.root.tiles[1];
        tile.manage = () => { if (behavior === 'throws') throw new Error('tile unavailable'); return false; };
        if (kind === 'quick') f.workspace.quickTiles = behavior === 'missing' ? [] : [quickTile(tile)];
        else if (behavior === 'missing') f.workspace.layoutRoot = null;
        const state = savedState({tile: kind === 'quick' ? {kind, edges: 2} :
            {kind, path: [1], region: {x: 1 / 3, y: 0, width: 1 / 3, height: 1}}});
        // An existing native tile must not hide a failure to restore the saved one.
        const original = f.target.tile = {unmanage() { throw new Error('must not untile'); }};
        const destination = {name: 'Virtual-2', geometry: {x: 1280, y: 0, width: 1280, height: 800}};
        f.workspace.screens.push(destination);
        state.outputName = destination.name;
        if (behavior === 'throws') assert.throws(() => f.restoreImmediately(state), /tile unavailable/);
        else {
            assert.equal(f.restoreImmediately(state), false);
            assert.equal(f.target.output, destination, 'a partial output move does not mean full restoration succeeded');
        }
        assert.equal(f.target.tile, original);
        assert.equal(f.workspace.activeWindow, behavior === 'throws' ? f.other : f.target);
    }
}
for (const mode of ['maximizeMode', 'fullScreen']) {
    const f = fixture();
    f.target[mode] = mode === 'maximizeMode' ? 3 : true;
    assert.equal(f.restoreImmediately(savedState({tile: {kind: 'quick', edges: 2}})), true,
        'an explicit app mode intentionally takes precedence over a saved tile');
    assert.equal(f.target[mode], mode === 'maximizeMode' ? 3 : true);
}
for (const rememberScreen of [false, true]) {
    for (const rememberDesktop of [false, true]) {
        const f = fixture();
        const first = f.workspace.currentDesktop, second = {id: 'desktop-2'};
        const savedOutput = {name: 'Virtual-2', geometry: {x: 1280, y: 0, width: 1280, height: 800}};
        f.workspace.screens.push(savedOutput);
        f.workspace.desktops.push(second);
        const state = savedState({rememberScreen, rememberDesktop, desktopIds: [second.id],
            outputName: savedOutput.name, outputPosition: {x: 1280, y: 0},
            position: {x: 1380, y: 80}});
        assert.equal(f.prepareImmediately(state), true);
        assert.equal(f.target.desktops[0], rememberDesktop ? second : first);
        assert.equal(!!f.target.minimized, rememberDesktop, 'only desktop restoration needs a startup guard');
        let activated = false;
        Object.defineProperty(f.workspace, 'activeWindow', {
            get() { return activated ? f.target : f.other; },
            set(window) {
                assert.equal(window, f.target);
                assert.equal(f.workspace.currentDesktop, second, 'switch before activation can bring the window here');
                assert.equal(window.desktops[0], second);
                window.minimized = false;
                activated = true;
            }
        });
        assert.equal(f.restoreImmediately(state), true);
        assert.equal(activated, rememberDesktop);
        assert.equal(f.workspace.currentDesktop, rememberDesktop ? second : first);
        assert.equal(f.workspace.lastClientAreaOutput, rememberScreen ? savedOutput : f.output);
        assert.equal(f.target.frameGeometry.x, rememberScreen ? 1380 : 100, 'keep the position relative to the selected screen');
        assert.equal(f.target.frameGeometry.width, 640);
    }
}
for (const extra of [{}, {rememberDesktop: false, onAllDesktops: true}]) {
    const f = fixture();
    const state = savedState({desktopIds: ['desktop-2'], ...extra});
    if (!Object.hasOwn(extra, 'rememberDesktop')) delete state.rememberDesktop;
    const second = {id: 'desktop-2'};
    f.workspace.desktops.push(second);
    assert.equal(f.prepareImmediately(state), true);
    assert.equal(f.restoreImmediately(state), true);
    assert.equal(f.target.desktops[0], f.workspace.currentDesktop);
    assert.equal(f.target.onAllDesktops, false, 'disabled desktop restore also ignores sticky membership');
    assert.equal(f.workspace.activeWindow, f.other);
    assert.equal(!!f.target.minimized, false);
}
for (const rememberDesktop of [false, true]) {
    const f = fixture();
    const first = f.workspace.currentDesktop, second = {id: 'desktop-2'};
    f.workspace.desktops.push(second);
    const wrong = desktopTile(f, rememberDesktop ? first : second);
    const destination = desktopTile(f, rememberDesktop ? second : first);
    f.workspace.quickTiles = [quickTile(wrong), quickTile(destination)];
    assert.equal(f.restoreImmediately(savedState({rememberDesktop, desktopIds: [second.id], tile: {kind: 'quick', edges: 2,
        region: {x: 0, y: 0, width: .65, height: 1}}})), true);
    assert.equal(f.target.tile, destination, 'quick tiles belong to the desktop selected by the policy');
    assert.equal(destination.relativeGeometry.width, .65);
    assert.equal(wrong.relativeGeometry.width, .5, 'do not resize a different desktop');
    assert.equal(wrong.owner, undefined);
}
{
    const f = fixture();
    const first = f.workspace.currentDesktop, second = {id: 'desktop-2'};
    const output = {name: 'Virtual-2', geometry: {x: 1280, y: 0, width: 1280, height: 800}};
    f.workspace.screens.push(output);
    f.workspace.desktops.push(second);
    let outputDesktop = first;
    const events = [];
    f.workspace.currentDesktopForScreen = screen => { assert.equal(screen, output); return outputDesktop; };
    f.workspace.setCurrentDesktopForScreen = (desktop, screen) => {
        assert.equal(screen, output);
        assert.equal(desktop, second);
        outputDesktop = desktop;
        events.push('switch');
    };
    Object.defineProperty(f.workspace, 'activeWindow', {set(window) {
        assert.equal(window, f.target);
        assert.equal(outputDesktop, second);
        events.push('activate');
    }});
    assert.equal(f.restoreImmediately(savedState({outputName: output.name, desktopIds: [second.id]})), true);
    assert.deepEqual(events, ['switch', 'activate']);
    assert.equal(f.workspace.currentDesktop, first, 'use the per-screen API when available');
}
for (const [edges, initial, region] of [
    [2, [0, 0, .5, 1], {x: 0, y: 0, width: .65, height: 1}],
    [4, [.5, 0, .5, 1], {x: .65, y: 0, width: .35, height: 1}],
    [1, [0, 0, 1, .5], {x: 0, y: 0, width: 1, height: .4}],
    [8, [0, .5, 1, .5], {x: 0, y: .4, width: 1, height: .6}],
    [3, [0, 0, .5, .5], {x: 0, y: 0, width: .65, height: .4}],
    [5, [.5, 0, .5, .5], {x: .65, y: 0, width: .35, height: .4}],
    [10, [0, .5, .5, .5], {x: 0, y: .4, width: .65, height: .6}],
    [12, [.5, .5, .5, .5], {x: .65, y: .4, width: .35, height: .6}]
]) {
    const f = fixture();
    const wrong = quickTile(f.leaf(...initial), edges), tile = quickTile(f.leaf(...initial), edges);
    const rejectedRegion = wrong.relativeGeometry;
    wrong.manage = () => false;
    Object.defineProperty(wrong, 'relativeGeometry', {get() {return rejectedRegion;}, set() {throw new Error('wrong desktop must not be resized');}});
    f.workspace.quickTiles = [wrong, tile];
    tile.manage = w => {
        assert.deepEqual({...tile.relativeGeometry}, {x: initial[0], y: initial[1], width: initial[2], height: initial[3]},
            'only resize after manage accepted the correct desktop');
        w.tile = tile;
        return true;
    };
    assert.equal(f.restoreImmediately(savedState({tile: {kind: 'quick', edges, region}})), true);
    assert.equal(f.target.tile, tile);
    assert.deepEqual({...tile.relativeGeometry}, region);
}
for (const region of [undefined, {}, {x: 0, y: 0, width: .6}, {x: -.1, y: 0, width: .7, height: 1},
    {x: 0, y: 0, width: 1.2, height: 1}, {x: 0, y: 0, width: 0, height: 1},
    {x: 0, y: 0, width: .6, height: 1.2}, {x: 0, y: 0, width: '.6', height: 1},
    {x: 0, y: 0, width: Infinity, height: 1}]) {
    const f = fixture();
    const tile = f.leaf(0, 0, .5, 1);
    f.workspace.quickTiles = [quickTile(tile)];
    assert.equal(f.restoreImmediately(savedState({tile: {kind: 'quick', edges: 2, region}})), true);
    assert.deepEqual({...tile.relativeGeometry}, {x: 0, y: 0, width: .5, height: 1}, 'missing/invalid proportions keep native layout');
}
{
    const f = fixture();
    const tile = f.leaf(0, 0, .5, 1);
    const initial = tile.relativeGeometry;
    Object.defineProperty(tile, 'relativeGeometry', {get() {return initial;}, set() {}});
    f.workspace.quickTiles = [quickTile(tile)];
    assert.equal(f.restoreImmediately(savedState({tile: {kind: 'quick', edges: 2,
        region: {x: 0, y: 0, width: .65, height: 1}}})), false, 'a rejected split must protect the saved state');
}
{
    const f = fixture();
    const tile = f.leaf(0, 0, .5, 1);
    Object.defineProperty(tile, 'relativeGeometry', {set() {throw new Error('floating sibling must not resize existing tiles');}});
    f.workspace.quickTiles = [quickTile(tile)];
    assert.equal(f.restoreImmediately(savedState({tile: {kind: 'quick', edges: 2,
        region: {x: 0, y: 0, width: .65, height: 1}}, inheritedTileSize: {
        tileSize: {width: 640, height: 480}, normalSize: {width: 900, height: 600}}})), true);
    assert.equal(f.target.tile, null);
    assert.equal(f.target.frameGeometry.width, 900);
}
{
    const f = fixture();
    const root = quickLayoutFixture(f);
    root.tiles[0].relativeGeometry = {x: 0, y: 0, width: .65, height: .85};
    const region = {x: 0, y: 0, width: .7, height: .9};
    assert.equal(f.restoreImmediately(savedState({tile: {kind: 'quick', edges: 2, region}})), true);
    assert.equal(f.target.tile, root.tiles[0], 'an outer-edge resize must not turn a half into a corner');
    assert.deepEqual({...root.tiles[0].relativeGeometry}, region);
}
{
    const f = fixture();
    const layout = quickLayoutFixture(f);
    const region = {x: 0, y: 0, width: .1, height: 1};
    const before = layout.tiles.map(t => ({...t.relativeGeometry}));
    layout.writes.length = 0;
    assert.equal(f.restoreImmediately(savedState({tile: {kind: 'quick', edges: 2, region}})), false);
    assert.equal(layout.writes.length, 0);
    assert.deepEqual(layout.tiles.map(t => ({...t.relativeGeometry})), before);
}

{
    const f = fixture();
    const layout = quickLayoutFixture(f);
    assert.equal(f.restoreImmediately(savedState({tile: {kind: 'quick', edges: 2,
        region: {x: 0, y: 0, width: .65, height: 1}}})), true);
    assert.equal(layout.tiles[0].relativeGeometry.width, .65);
    assert.equal(layout.tiles[1].relativeGeometry.x, .65);
    assert.equal(layout.tiles[1].relativeGeometry.width, .35);
}
console.log('Tiling and window positions: app sizes/modes preserved, desktop/output fallbacks and validation passed.');

// Placement collisions use the current decoration height, without changing size.
function blocker(f, x = 100, y = 80, extra = {}) {
    return {normalWindow: true, output: f.output, desktops: [f.workspace.currentDesktop], activities: [],
        frameGeometry: {x, y, width: 400, height: 300}, ...extra};
}
for (const [height, step] of [[24, 30], [45.5, 56.875], [0, 37.5], [-1, 37.5], [undefined, 37.5]]) {
    const f = fixture();
    f.workspace.windows.push(blocker(f), blocker(f, 100 + step, 80 + step));
    f.restoreImmediately(savedState({titleBarHeight: height}));
    assert.deepEqual({...f.target.frameGeometry}, {x: 100 + step * 2, y: 80 + step * 2, width: 640, height: 480});
    assert.equal(f.workspace.activeWindow, f.target);
}
for (const extra of [{minimized: true}, {deleted: true}, {normalWindow: false}, {popupWindow: true},
    {output: {}}, {desktops: [{id: 'elsewhere'}]}, {activities: ['elsewhere']}]) {
    const f = fixture();
    f.target.activities = ['work'];
    f.workspace.windows.push(blocker(f, 100, 80, extra));
    f.restoreImmediately(savedState({titleBarHeight: 25}));
    assert.equal(f.target.frameGeometry.x, 100);
    assert.equal(f.target.frameGeometry.y, 80);
}
for (const extra of [{onAllDesktops: true, desktops: []}, {activities: []}, {readyForPainting: false}]) {
    const f = fixture();
    f.target.activities = ['work'];
    f.workspace.windows.push(blocker(f, 100, 80, extra));
    f.restoreImmediately(savedState({titleBarHeight: 25}));
    assert.equal(f.target.frameGeometry.x, 131.25);
    assert.equal(f.target.frameGeometry.y, 111.25);
}
{
    const f = fixture();
    f.workspace.windows.push(blocker(f, 124, 104));
    f.restoreImmediately(savedState({titleBarHeight: 24}));
    assert.equal(f.target.frameGeometry.x, 100, 'threshold is strictly less than the title height');
}
{
    const f = fixture();
    f.workspace.windows.push(blocker(f, 640, 320));
    f.restoreImmediately(savedState({position: {x: 640, y: 320}, titleBarHeight: 24}));
    assert.deepEqual({...f.target.frameGeometry}, {x: 0, y: 0, width: 640, height: 480}, 'wrap within the usable area');
}
{
    const f = fixture();
    f.output.geometry = {x: 0, y: 0, width: 640, height: 480};
    f.workspace.windows.push(blocker(f, 0, 0));
    f.restoreImmediately(savedState());
    assert.deepEqual({...f.target.frameGeometry}, {x: 0, y: 0, width: 640, height: 480}, 'no available space: retain bounded saved placement');
}
{
    const f = fixture();
    f.workspace.windows.push(blocker(f));
    const tile = f.leaf(0, 0, 0.5, 1);
    f.workspace.quickTiles = [quickTile(tile)];
    f.restoreImmediately(savedState({titleBarHeight: 25, tile: {kind: 'quick', edges: 2}}));
    assert.equal(f.target.tile, tile);
    assert.equal(f.target.frameGeometry.x, 100, 'saved native snap bypasses cascading');
}
console.log('Window collision placement: decoration height, fallback, scope, cascading and bounds passed.');

// Seed only a restored native tile's unsnap geometry, never a floating size.
for (const kind of ['quick', 'custom']) {
    const f = fixture();
    let normal;
    const tile = f.leaf(0, 0, 0.5, 1);
    tile.manage = function (window) {
        normal = {...window.frameGeometry};
        window.tile = this;
        window.frameGeometry = {x: 0, y: 0, width: 640, height: 800};
        return true;
    };
    tile.unmanage = function (window) { window.tile = null; window.frameGeometry = normal; };
    if (kind === 'quick') f.workspace.quickTiles = [quickTile(tile)];
    else f.workspace.layoutRoot = {tiles: [tile]};
    const saved = kind === 'quick' ? {kind, edges: 2} : {kind, path: [0], region: {...tile.relativeGeometry}};
    saved.normalSize = {width: 900.5, height: 650.25};
    f.restoreImmediately(savedState({tile: saved}));
    assert.equal(normal.width, 900.5);
    assert.equal(normal.height, 650.25);
    assert.equal(f.target.tile, tile);
    tile.unmanage(f.target);
    assert.equal(f.target.frameGeometry.width, 900.5, 'native unsnap restores the pre-tiling width');
    assert.equal(f.target.frameGeometry.height, 650.25);
    assert.equal(f.workspace.activeWindow, f.target);
}
for (const outcome of ['rejected', 'throws', 'missing']) {
    const f = fixture();
    const tile = f.leaf(0, 0, 0.5, 1);
    tile.manage = () => { if (outcome === 'throws') throw new Error('tile unavailable'); return false; };
    f.workspace.quickTiles = outcome === 'missing' ? [] : [quickTile(tile)];
    const state = savedState({tile: {kind: 'quick', edges: 2, normalSize: {width: 900, height: 650}}});
    if (outcome === 'throws') assert.throws(() => f.restoreImmediately(state), /tile unavailable/);
    else assert.equal(f.restoreImmediately(state), false);
    assert.deepEqual({...f.target.frameGeometry}, {x: 100, y: 80, width: 640, height: 480}, 'failed native tiling keeps the app-requested size');
}
for (const size of [undefined, {width: 0, height: 650}, {width: -1, height: 650}, {width: 900}, {width: '900', height: 650}, {width: Infinity, height: 650}]) {
    const f = fixture();
    const tile = f.leaf(0, 0, 0.5, 1);
    let initial;
    tile.manage = window => { initial = {...window.frameGeometry}; window.tile = tile; return true; };
    f.workspace.quickTiles = [quickTile(tile)];
    f.restoreImmediately(savedState({tile: {kind: 'quick', edges: 2, normalSize: size}}));
    assert.equal(initial.width, 640);
    assert.equal(initial.height, 480);
}
{
    const f = fixture();
    f.workspace.quickTiles = [quickTile(f.leaf(0, 0, 0.5, 1))];
    f.restoreImmediately(savedState({normalSize: {width: 900, height: 650}}));
    assert.equal(f.target.frameGeometry.width, 640, 'floating states never resize the app');
    assert.equal(f.target.frameGeometry.height, 480);
}
console.log('Restored native tile normal sizes: unsnapping, unavailable tiles and missing metadata passed.');

// A new floating instance may inherit a live sibling's tile-sized defaults.
{
    const f = fixture();
    assert.equal(f.restoreImmediately(savedState({tile: {kind: 'quick', edges: 2}, inheritedTileSize: {
        tileSize: {width: 640, height: 480}, normalSize: {width: 900, height: 600}
    }})), true);
    assert.deepEqual({...f.target.frameGeometry}, {x: 100, y: 80, width: 900, height: 600});
    assert.equal(f.target.tile, null, 'a new sibling must not clone the saved tile');
    assert.equal(f.workspace.activeWindow, f.target);
}
{
    const f = fixture();
    f.restoreImmediately(savedState({position: {x: 1200, y: 700}, inheritedTileSize: {
        tileSize: {width: 640, height: 480}, normalSize: {width: 900, height: 600}
    }}));
    assert.deepEqual({...f.target.frameGeometry}, {x: 380, y: 200, width: 900, height: 600}, 'bound placement using the recovered size');
}
for (const hint of [
    {tileSize: {width: 700, height: 480}, normalSize: {width: 900, height: 600}},
    {tileSize: {width: 640, height: 480}, normalSize: {width: 0, height: 600}},
    {tileSize: {width: 640, height: 480}, normalSize: {width: 900}},
    {tileSize: {width: 640, height: 480}, normalSize: {width: '900', height: 600}},
    {normalSize: {width: 900, height: 600}}
]) {
    const f = fixture();
    f.restoreImmediately(savedState({inheritedTileSize: hint}));
    assert.equal(f.target.frameGeometry.width, 640);
    assert.equal(f.target.frameGeometry.height, 480);
}
for (const mode of ['maximizeMode', 'fullScreen', 'tile', 'fixed']) {
    const f = fixture();
    if (mode === 'fixed') f.target.resizeable = false;
    else f.target[mode] = mode === 'tile' ? {} : 1;
    f.restoreImmediately(savedState({inheritedTileSize: {
        tileSize: {width: 640, height: 480}, normalSize: {width: 900, height: 600}
    }}));
    assert.equal(f.target.frameGeometry.width, 640, 'preserve explicit app modes and fixed sizes');
    assert.equal(f.target.frameGeometry.height, 480);
}
console.log('Inherited tile sizes: new floating siblings, app modes and mismatched dimensions passed.');
