// SPDX-License-Identifier: GPL-2.0-or-later
(function () {
    "use strict";
    const service = "com.github.guiodic.MaterialTiling";
    const path = "/Tiling";
    function excludeDecorationPopup(window) {
        // Older KWin versions include internal QMenus in Overview and Grid.
        if (window.popupWindow && window.caption === "com.github.guiodic.material-decoration.popup") {
            window.skipSwitcher = true;
        }
    }
    workspace.windowAdded.connect(excludeDecorationPopup);
    workspace.windowList().forEach(excludeDecorationPopup);
    const tileActions = {
        2: "slotWindowQuickTileLeft",
        4: "slotWindowQuickTileRight",
        1: "slotWindowQuickTileTop",
        8: "slotWindowQuickTileBottom",
        3: "slotWindowQuickTileTopLeft",
        5: "slotWindowQuickTileTopRight",
        10: "slotWindowQuickTileBottomLeft",
        12: "slotWindowQuickTileBottomRight"
    };
    const lastSelection = {};
    const layouts = {};
    function id(window) { return String(window.internalId).replace(/[{}]/g, "").toLowerCase(); }
    workspace.windowRemoved.connect(function (window) {
        delete lastSelection[id(window)];
        delete layouts[id(window)];
    });

    function readLayout(target) {
        if (typeof workspace.tilingForScreen !== "function") return null;
        const manager = workspace.tilingForScreen(target.output);
        const root = manager && manager.rootTile;
        if (!root) return null;
        const tiles = [];
        function visit(tile) {
            if (tile.tiles.length) tile.tiles.forEach(visit);
            else if (tile !== root) tiles.push(tile);
        }
        visit(root);
        return {root: root, output: target.output, tiles: tiles, geometry: tiles.map(function (tile) {
            // KWin normalizes relativeGeometry to the screen, even for nested
            // tiles. Composing it with ancestor geometry would transform it twice.
            const g = tile.relativeGeometry;
            return {x: g.x, y: g.y, width: g.width, height: g.height};
        })};
    }

    function captureLayout(target, request) {
        const layout = readLayout(target);
        layouts[request.uuid] = layout;
        if (!layout) return [];
        layout.ids = layout.tiles.map(function (_, i) { return request.id + ":" + i; });
        return layout.geometry.map(function (g, i) {
            return {id: layout.ids[i], x: g.x, y: g.y, width: g.width, height: g.height};
        });
    }

    function customTile(target, request) {
        const snapshot = layouts[request.uuid];
        const current = readLayout(target);
        const index = snapshot ? snapshot.ids.indexOf(request.tileId) : -1;
        if (!snapshot || !current || index < 0 || snapshot.root !== current.root || snapshot.output !== current.output ||
            snapshot.tiles.length !== current.tiles.length || snapshot.tiles.some(function (tile, i) {
                const before = snapshot.geometry[i], now = current.geometry[i];
                return tile !== current.tiles[i] || before.x !== now.x || before.y !== now.y ||
                    before.width !== now.width || before.height !== now.height;
            })) throw new Error("Custom tiling layout changed; reopen the popup");
        return current.tiles[index];
    }

    function restoreState(target, state) {
        if (!target.normalWindow || target.transient || target.deleted) throw new Error("Target window cannot be restored");
        const g = state && state.geometry;
        if (!state || state.version !== 1 || !g ||
            ![g.x, g.y, g.width, g.height].every(function (n) { return typeof n === "number" && Number.isFinite(n); }) ||
            g.width <= 0 || g.height <= 0) throw new Error("Invalid saved geometry");
        const mode = state.maximizeMode;
        if (!Number.isInteger(mode) || mode < 0 || mode > 3 || !Array.isArray(state.desktopIds)) throw new Error("Invalid saved state");
        const screens = workspace.screens;
        const output = screens.find(function (screen) { return state.outputUuid && String(screen.uuid) === state.outputUuid; }) ||
            screens.find(function (screen) { return state.outputName && screen.name === state.outputName; }) || target.output || screens[0];
        if (!output) throw new Error("No screen is available");
        const desktops = workspace.desktops.filter(function (desktop) { return state.desktopIds.indexOf(desktop.id) !== -1; });

        // This path deliberately does not activate the window or change the
        // current desktop. The popup's tiling activation is unrelated.
        target.fullScreen = false;
        target.setMaximize(false, false);
        if (target.tile) target.tile.unmanage(target);
        if (state.onAllDesktops) target.onAllDesktops = true;
        else if (desktops.length) target.desktops = desktops;
        workspace.sendClientToScreen(target, output);

        const area = workspace.clientArea(KWin.PlacementArea, output, target.desktops[0] || workspace.currentDesktop);
        const before = state.outputGeometry;
        let x = g.x, y = g.y;
        if (before && Number.isFinite(before.x) && Number.isFinite(before.y)) {
            x += output.geometry.x - before.x;
            y += output.geometry.y - before.y;
        }
        // Fixed-size windows can still restore their position and desktop.
        const width = target.resizeable ? Math.min(g.width, area.width) : target.frameGeometry.width;
        const height = target.resizeable ? Math.min(g.height, area.height) : target.frameGeometry.height;
        x = Math.max(area.x, Math.min(x, area.x + area.width - width));
        y = Math.max(area.y, Math.min(y, area.y + area.height - height));
        // KWin registers a converter from a JavaScript rectangle to RectF.
        // Its returned native value wrapper has read-only fields in JS.
        target.frameGeometry = {x: x, y: y, width: width, height: height};
        target.setMaximize(Boolean(mode & 1), Boolean(mode & 2));
        target.fullScreen = Boolean(state.fullScreen);
    }

    function apply(request) {
        const target = workspace.windowList().find(function (window) { return id(window) === request.uuid; });
        if (!target || target.deleted || target.specialWindow) throw new Error("Target window is unavailable");
        if (request.operation === "restoreState") return restoreState(target, JSON.parse(request.stateJson));
        if (!target.resizeable) throw new Error("Target window is not resizeable");
        if (request.operation === "ping") return request.layout ? captureLayout(target, request) : undefined;
        const action = tileActions[request.edges];
        const custom = request.operation === "tileCustom";
        if (!custom && (request.operation !== "tile" || !action || typeof workspace[action] !== "function")) throw new Error("Unsupported tiling request");
        // Activation can switch desktops; validate both before and after it.
        if (custom) customTile(target, request);
        const output = target.output;
        const tile = target.tile;
        const previous = lastSelection[request.uuid];
        workspace.activeWindow = target;
        if (workspace.activeWindow !== target) throw new Error("Target window could not be activated");
        if (custom) {
            // manage() transfers the native association without rewriting the
            // user's layout or restoring geometry on another output first.
            const destination = customTile(target, request);
            if (!destination.manage(target)) throw new Error("Custom tile could not manage the window");
            delete lastSelection[request.uuid];
            return;
        }
        // Keep a resized native tile when the user selects it again, without
        // invoking a shortcut that might combine modes or switch monitors.
        if (tile && previous && previous.edges === request.edges && previous.tile === tile && previous.output === output) return;
        if (tile) {
            if (typeof tile.unmanage !== "function") throw new Error("This KWin version lacks tile.unmanage");
            tile.unmanage(target);
            // Restore geometry may refer to a different monitor. Keep tiling on
            // the output which contained the window when the request arrived.
            if (target.output !== output) workspace.sendClientToScreen(target, output);
        }
        workspace[action]();
        if (!target.tile) throw new Error("KWin did not assign a native tile");
        lastSelection[request.uuid] = {edges: request.edges, tile: target.tile, output: output};
    }

    registerShortcut("MaterialTilingPopupDispatch", "Material Tiling Popup", "", function () {
        callDBus(service, path, service, "takeRequest", function (request) {
            if (!request || !request.id) return;
            let success = true;
            let reason = "";
            let layout;
            try { layout = apply(request); } catch (error) { success = false; reason = String(error); }
            if (success && layout !== undefined) callDBus(service, path, service, "completeLayout", request.id, JSON.stringify(layout));
            else callDBus(service, path, service, "complete", request.id, success, reason);
        });
    });
})();
