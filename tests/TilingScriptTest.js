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
    function leaf(x, y, width, height) {
        return {relativeGeometry: {x, y, width, height}, tiles: [], manage(window) { window.tile = this; return true; }, unmanage(window) { window.tile = null; }};
    }
    const root = {tiles: [leaf(0, 0, 1 / 3, 1), leaf(1 / 3, 0, 1 / 3, 1), leaf(2 / 3, 0, 1 / 3, 1)]};
    const workspace = {activeWindow: other, layoutRoot: root, screens: [output], desktops: [desktop], currentDesktop: desktop,
        clientArea(option, screen) { this.lastClientAreaOutput = screen; return screen.geometry; },
        tilingForScreen() { return {rootTile: this.layoutRoot}; },
        windows: [target, other, ...extraWindows], windowList() { return this.windows; },
        windowAdded: {connect(callback) { this.callback = callback; }},
        windowRemoved: {connect(callback) { this.callback = callback; }},
        sendClientToScreen(window, screen) { window.output = screen; }};
    let shortcut, request, result;
    const calls = [];
    for (const name of ['Left', 'Right', 'Top', 'Bottom', 'TopLeft', 'TopRight', 'BottomLeft', 'BottomRight']) {
        workspace['slotWindowQuickTile' + name] = () => {
            calls.push({name, target: workspace.activeWindow});
            workspace.activeWindow.tile = {unmanage(window) { window.tile = null; }};
        };
    }
    const context = {workspace, KWin: {PlacementArea: 0},
        registerShortcut(name, text, key, callback) { assert.equal(key, ''); shortcut = callback; },
        callDBus(service, objectPath, iface, method, ...args) {
            if (method === 'takeRequest') args[0](request);
            else if (method === 'completeLayout') result = {id: args[0], success: true, layout: JSON.parse(args[1])};
            else { assert.equal(method, 'complete'); result = {id: args[0], success: args[1], reason: args[2]}; }
        }};
    vm.runInNewContext(source, context);
    return {workspace, target, other, calls, output, root, leaf,
        restoreImmediately(state) { return context.materialRestoreWindowState(target, JSON.stringify(state)); },
        prepareImmediately(state) { return context.materialPrepareWindowState(target, JSON.stringify(state)); },
        send(edges, uuid = 'target', operation = 'tile', extra = {}) {
            request = {id: String(++assertions), uuid, edges, operation, ...extra}; result = undefined;
            if (operation === 'restoreState') request.stateJson = JSON.stringify(extra.state);
            shortcut(); assert.ok(result); assert.equal(result.id, request.id); return result;
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
        assert.deepEqual(result.layout[i], {id: result.id + ':' + i, ...f.root.tiles[i].relativeGeometry});
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
    return {version: 2, position: {x: 100, y: 80}, desktopIds: ['desktop-1'],
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
    assert.throws(() => f.restoreImmediately(savedState({version: 3})), /Invalid saved position/);
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
console.log('Tiling and window positions: app sizes/modes preserved, desktop/output fallbacks and validation passed.');
