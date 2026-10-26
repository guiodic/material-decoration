// SPDX-License-Identifier: GPL-2.0-or-later
// Run with: node tests/TilingScriptTest.js
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../scripts/material-tiling/contents/code/main.js'), 'utf8');
let assertions = 0;
function fixture(extraWindows = []) {
    const output = {};
    const other = {internalId: '{other}', caption: 'other'};
    const target = {internalId: '{target}', resizeable: true, specialWindow: false, output, tile: null};
    function leaf(x, y, width, height) {
        return {relativeGeometry: {x, y, width, height}, tiles: [], manage(window) { window.tile = this; return true; }, unmanage(window) { window.tile = null; }};
    }
    const root = {tiles: [leaf(0, 0, 1 / 3, 1), leaf(1 / 3, 0, 1 / 3, 1), leaf(2 / 3, 0, 1 / 3, 1)]};
    const workspace = {activeWindow: other, layoutRoot: root,
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
    vm.runInNewContext(source, {workspace,
        registerShortcut(name, text, key, callback) { assert.equal(key, ''); shortcut = callback; },
        callDBus(service, objectPath, iface, method, ...args) {
            if (method === 'takeRequest') args[0](request);
            else if (method === 'completeLayout') result = {id: args[0], success: true, layout: JSON.parse(args[1])};
            else { assert.equal(method, 'complete'); result = {id: args[0], success: args[1], reason: args[2]}; }
        }});
    return {workspace, target, other, calls, output, root, leaf,
        send(edges, uuid = 'target', operation = 'tile', extra = {}) {
            request = {id: String(++assertions), uuid, edges, operation, ...extra}; result = undefined;
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
console.log('Tiling script: quick tiles, custom layouts, nested tiles, stale selections and output preservation passed.');
