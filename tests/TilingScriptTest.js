// SPDX-License-Identifier: GPL-2.0-or-later
// Run with: node tests/TilingScriptTest.js
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../scripts/material-tiling/contents/code/main.js'), 'utf8');
let assertions = 0;
function fixture(extraWindows = []) {
    const output = {name: 'Virtual-1', uuid: 'output-1', geometry: {x: 0, y: 0, width: 1280, height: 800}};
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
        return {relativeGeometry: {x, y, width, height}, tiles: [], manage(window) { window.tile = this; return true; }, unmanage(window) { window.tile = null; }};
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
            ++assertions;
            const target = workspace.windowList().find(window => String(window.internalId).replace(/[{}]/g, '') === uuid);
            try {
                if (operation === 'restoreState') return {success: context.materialRestoreWindowState(target, JSON.stringify(extra.state))};
                const result = context.materialTiling(target, JSON.stringify({edges, operation: operation === 'ping' ? 'layout' : operation, ...extra}));
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
    assert.ok(f.send(0, 'target', 'ping').success);
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
    const result = f.send(0, 'target', 'ping', {layout: true});
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
    const result = f.send(0, 'target', 'ping', {layout: true});
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
    f => { f.send(0, 'target', 'ping', {layout: true}); }
]) {
    const f = fixture();
    assert.ok(f.send(2).success);
    const previousTile = f.target.tile;
    const result = f.send(0, 'target', 'ping', {layout: true});
    change(f);
    assert.ok(!f.send(0, 'target', 'tileCustom', {tileId: result.layout[0].id}).success);
    assert.equal(f.target.tile, previousTile, 'a stale selection must leave the previous tile untouched');
    const refreshed = f.send(0, 'target', 'ping', {layout: true});
    assert.ok(f.send(0, 'target', 'tileCustom', {tileId: refreshed.layout[0].id}).success,
        'reopening the popup must allow selection from the current layout');
}
{
    const f = fixture();
    assert.ok(!f.send(0, 'target', 'tileCustom', {tileId: 'invented'}).success);
    delete f.workspace.tilingForScreen;
    assert.deepEqual(f.send(0, 'target', 'ping', {layout: true}).layout, []);
    assert.ok(f.send(2).success, 'native quick tiling remains available without the custom-layout API');
}
{
    const f = fixture();
    const result = f.send(0, 'target', 'ping', {layout: true});
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
        onAllDesktops: false, outputUuid: 'output-1', outputName: 'Virtual-1',
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
    assert.equal(f.workspace.currentDesktop.id, 'desktop-1');
    assert.equal(f.workspace.activeWindow, f.other);
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
    assert.equal(f.workspace.activeWindow, f.other);
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
    assert.equal(f.workspace.activeWindow, f.other);
    assert.equal(f.workspace.currentDesktop.id, 'desktop-1');
}
{
    const f = fixture();
    const movedOutput = {name: 'Virtual-1', uuid: 'output-1', geometry: {x: 1920, y: 50, width: 800, height: 600}};
    f.workspace.screens = [movedOutput];
    assert.ok(f.send(0, 'target', 'restoreState', {state: savedState()}).success);
    assert.equal(f.workspace.lastClientAreaOutput, movedOutput);
    assert.deepEqual({...f.target.frameGeometry}, {x: 2020, y: 130, width: 640, height: 480});
    const tinyOutput = {name: 'Virtual-1', uuid: 'output-1', geometry: {x: 1920, y: 50, width: 400, height: 300}};
    f.workspace.screens = [tinyOutput];
    assert.ok(f.send(0, 'target', 'restoreState', {state: savedState()}).success);
    assert.deepEqual({...f.target.frameGeometry}, {x: 1920, y: 50, width: 640, height: 480}, 'a smaller output must not shrink the app');
}
{
    const f = fixture();
    f.target.resizeable = false;
    assert.ok(f.send(0, 'target', 'restoreState', {state: savedState({desktopIds: ['removed'], outputUuid: 'removed', outputName: 'removed'})}).success);
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
    const destination = {name: 'Virtual-2', uuid: 'output-2', geometry: {x: 1280, y: 0, width: 1280, height: 800}};
    f.workspace.screens.push(destination);
    assert.ok(f.send(0, 'target', 'restoreState', {state: savedState({outputUuid: 'output-2', onAllDesktops: true})}).success);
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
    f.workspace.quickTiles = [wrongDesktop, quick];
    assert.equal(f.restoreImmediately(savedState({tile: {kind: 'quick', edges: 2}})), true);
    assert.equal(f.target.tile, quick, 'restore a native quick tile, not just its coordinates');
    assert.equal(f.workspace.activeWindow, f.other);
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
    f.workspace.quickTiles = [inactive, active];
    f.restoreImmediately(savedState({onAllDesktops: allDesktops, desktopIds: allDesktops ? [] : [first.id, second.id], tile: {kind: 'quick', edges: 2}}));
    assert.equal(f.target.tile, active, 'bind the root of the current desktop, even when another root is enumerated first');
    assert.equal(active.attempts, 1);
    assert.equal(f.target.onAllDesktops, allDesktops);
    assert.deepEqual(Array.from(f.target.desktops), allDesktops ? [] : [first, second]);
    assert.equal(f.workspace.currentDesktop, second);
    assert.equal(f.workspace.activeWindow, f.other);
    assert.equal(f.calls.length, 0);
}
{
    const f = fixture();
    const first = f.workspace.currentDesktop, second = {id: 'desktop-2'}, third = {id: 'desktop-3'};
    f.workspace.desktops.push(second, third);
    f.workspace.currentDesktop = third;
    const wrong = desktopTile(f, second), destination = desktopTile(f, first);
    f.workspace.quickTiles = [wrong, destination];
    f.restoreImmediately(savedState({desktopIds: [first.id, second.id], tile: {kind: 'quick', edges: 2}}));
    assert.equal(wrong.owner, undefined);
    assert.equal(destination.owner, f.target);
    assert.deepEqual(Array.from(f.target.desktops), [first, second]);
    assert.equal(f.workspace.currentDesktop, third);
    assert.equal(f.workspace.activeWindow, f.other);
}
{
    const f = fixture();
    const globalDesktop = f.workspace.currentDesktop, outputDesktop = {id: 'desktop-2'};
    f.workspace.desktops.push(outputDesktop);
    f.workspace.currentDesktopForScreen = output => { assert.equal(output, f.output); return outputDesktop; };
    const inactive = desktopTile(f, globalDesktop), active = desktopTile(f, outputDesktop);
    f.workspace.quickTiles = [inactive, active];
    f.restoreImmediately(savedState({onAllDesktops: true, desktopIds: [], tile: {kind: 'quick', edges: 2}}));
    assert.equal(f.target.tile, active, 'use the output desktop when desktops differ across screens');
    assert.equal(f.target.onAllDesktops, true);
    assert.equal(f.workspace.currentDesktop, globalDesktop);
}
for (const behavior of ['missing-tile', 'throws', 'rule-rejects-restriction']) {
    const f = fixture();
    const tile = f.leaf(0, 0, 0.5, 1);
    f.workspace.quickTiles = behavior === 'missing-tile' ? [] : [tile];
    if (behavior === 'throws') tile.manage = () => { throw new Error('tile disappeared'); };
    if (behavior === 'rule-rejects-restriction') {
        let desktops = [];
        Object.defineProperty(f.target, 'desktops', {get() { return desktops; }, set() { desktops = []; }});
        tile.manage = () => { throw new Error('must not use a root without a desktop restriction'); };
    }
    const state = savedState({onAllDesktops: true, desktopIds: [], tile: {kind: 'quick', edges: 2}});
    if (behavior === 'throws') assert.throws(() => f.restoreImmediately(state), /tile disappeared/);
    else assert.equal(f.restoreImmediately(state), true);
    assert.equal(f.target.onAllDesktops, true, 'restore sticky membership even after failure');
    assert.deepEqual(Array.from(f.target.desktops), []);
    assert.equal(f.workspace.activeWindow, f.other);
}

{
    const f = fixture();
    const region = {x: 1 / 3, y: 0, width: 1 / 3, height: 1};
    assert.equal(f.restoreImmediately(savedState({tile: {kind: 'custom', path: [1], region}})), true);
    assert.equal(f.target.tile, f.root.tiles[1]);
    assert.equal(f.workspace.activeWindow, f.other);
    const changed = fixture();
    assert.equal(changed.restoreImmediately(savedState({tile: {kind: 'custom', path: [1], region: {...region, width: 0.9}}})), true);
    assert.equal(changed.target.tile, null, 'changed layouts must fall back to floating placement');
    assert.equal(changed.target.frameGeometry.width, 640);
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
    assert.equal(f.workspace.activeWindow, f.other);
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
    f.workspace.quickTiles = [tile];
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
    if (kind === 'quick') f.workspace.quickTiles = [tile];
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
    assert.equal(f.workspace.activeWindow, f.other);
}
for (const outcome of ['rejected', 'throws', 'missing']) {
    const f = fixture();
    const tile = f.leaf(0, 0, 0.5, 1);
    tile.manage = () => { if (outcome === 'throws') throw new Error('tile unavailable'); return false; };
    f.workspace.quickTiles = outcome === 'missing' ? [] : [tile];
    const state = savedState({tile: {kind: 'quick', edges: 2, normalSize: {width: 900, height: 650}}});
    if (outcome === 'throws') assert.throws(() => f.restoreImmediately(state), /tile unavailable/);
    else f.restoreImmediately(state);
    assert.deepEqual({...f.target.frameGeometry}, {x: 100, y: 80, width: 640, height: 480}, 'failed native tiling keeps the app-requested size');
}
for (const size of [undefined, {width: 0, height: 650}, {width: -1, height: 650}, {width: 900}, {width: '900', height: 650}, {width: Infinity, height: 650}]) {
    const f = fixture();
    const tile = f.leaf(0, 0, 0.5, 1);
    let initial;
    tile.manage = window => { initial = {...window.frameGeometry}; window.tile = tile; return true; };
    f.workspace.quickTiles = [tile];
    f.restoreImmediately(savedState({tile: {kind: 'quick', edges: 2, normalSize: size}}));
    assert.equal(initial.width, 640);
    assert.equal(initial.height, 480);
}
{
    const f = fixture();
    f.workspace.quickTiles = [f.leaf(0, 0, 0.5, 1)];
    f.restoreImmediately(savedState({normalSize: {width: 900, height: 650}}));
    assert.equal(f.target.frameGeometry.width, 640, 'floating states never resize the app');
    assert.equal(f.target.frameGeometry.height, 480);
}
console.log('Restored native tile normal sizes: unsnapping, unavailable tiles and missing metadata passed.');
