// SPDX-License-Identifier: GPL-2.0-or-later
// Run with: node tests/TilingScriptTest.js
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../scripts/material-tiling/contents/code/main.js'), 'utf8');
let assertions = 0;
function fixture() {
    const output = {};
    const other = {internalId: '{other}', caption: 'other'};
    const target = {internalId: '{target}', resizeable: true, specialWindow: false, output, tile: null};
    const workspace = {activeWindow: other, windows: [target, other], windowList() { return this.windows; },
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
            else { assert.equal(method, 'complete'); result = {id: args[0], success: args[1], reason: args[2]}; }
        }});
    return {workspace, target, other, calls, output,
        send(edges, uuid = 'target', operation = 'tile') {
            request = {id: String(++assertions), uuid, edges, operation}; result = undefined;
            shortcut(); assert.ok(result); assert.equal(result.id, request.id); return result;
        }};
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
console.log('Tiling script: all eight zones, explicit target, repeat selection, unavailable targets and output preservation passed.');
