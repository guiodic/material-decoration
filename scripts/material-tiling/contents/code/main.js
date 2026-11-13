// SPDX-License-Identifier: GPL-2.0-or-later
// Synchronous entry points for the decoration's in-process Qt bridge.
var materialRestoreWindowState;
var materialPrepareWindowState;
var materialTiling;
(function () {
    "use strict";
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
    let layoutGeneration = 0;
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

    function captureLayout(target) {
        const layout = readLayout(target);
        layouts[id(target)] = layout;
        if (!layout) return [];
        const generation = ++layoutGeneration;
        layout.ids = layout.tiles.map(function (_, i) { return generation + ":" + i; });
        return layout.geometry.map(function (g, i) {
            return {id: layout.ids[i], x: g.x, y: g.y, width: g.width, height: g.height};
        });
    }

    function customTile(target, request) {
        const snapshot = layouts[id(target)];
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

    function manageRestoredTile(target, saved, tile) {
        const size = saved.normalSize;
        const original = target.frameGeometry;
        const prepareSize = !target.tile && !target.fullScreen && !target.maximizeMode && size &&
            [size.width, size.height].every(function (n) { return typeof n === "number" && Number.isFinite(n) && n > 0; }) &&
            (original.width !== size.width || original.height !== size.height);
        // Native manage() stores the synchronous moveResizeGeometry for unsnapping.
        if (prepareSize) target.frameGeometry = {x: original.x, y: original.y, width: size.width, height: size.height};
        const managed = tile.manage(target);
        if (!managed && prepareSize) target.frameGeometry = original;
        return managed;
    }

    function quickTileMode(tile) {
        // The native mode survives resizing an outer edge away from the screen.
        const siblings = tile.parent && tile.parent.tiles;
        const index = siblings && siblings.length === 8 ? Array.from(siblings).indexOf(tile) : -1;
        return index >= 0 ? [2, 4, 1, 8, 3, 5, 10, 12][index] : 0;
    }

    function tileFrame(target, region, tile, output) {
        const area = workspace.clientArea(KWin.MaximizeArea, output, restorationDesktop(target, output));
        const padding = tile.padding || 0;
        const x = Math.round(area.x + region.x * area.width);
        const y = Math.round(area.y + region.y * area.height);
        const right = Math.min(output.geometry.x + output.geometry.width, x + Math.round(region.width * area.width)) -
            (region.x + region.width < 1 ? padding / 2 : padding);
        const bottom = Math.min(output.geometry.y + output.geometry.height, y + Math.round(region.height * area.height)) -
            (region.y + region.height < 1 ? padding / 2 : padding);
        const left = Math.max(output.geometry.x, x) + (region.x > 0 ? padding / 2 : padding);
        const top = Math.max(output.geometry.y, y) + (region.y > 0 ? padding / 2 : padding);
        return {x: left, y: top, width: right - left, height: bottom - top};
    }

    function restoreQuickTileRegion(target, saved, tile, output, resizeOthers) {
        const region = saved.region;
        const keys = ["x", "y", "width", "height"];
        // Old states need only their edge; invalid optional proportions are ignored.
        if (!region || !keys.every(function (key) { return typeof region[key] === "number" && Number.isFinite(region[key]); }) ||
            region.x < 0 || region.y < 0 || region.width <= 0 || region.height <= 0 ||
            region.x + region.width > 1.00001 || region.y + region.height > 1.00001) return true;
        const copy = function (r) { return {x: r.x, y: r.y, width: r.width, height: r.height}; };
        const same = function (a, b) { return keys.every(function (key) { return Math.abs(a[key] - b[key]) < 0.00001; }); };
        const minimum = tile.minimumSize;
        if (minimum && (region.width < minimum.width || region.height < minimum.height)) return false;
        if (!resizeOthers) {
            // Keep the native association and unsnap geometry, but apply the
            // saved frame only to this window without changing the shared tile.
            target.frameGeometry = tileFrame(target, region, tile, output);
            return true;
        }
        // KWin owns the shared layout and propagates the saved split.
        tile.relativeGeometry = copy(region);
        return same(tile.relativeGeometry, region);
    }

    function compatibleCustomTile(target, saved, output) {
        if (!saved.path || !saved.region || typeof workspace.rootTile !== "function") return null;
        let tile = workspace.rootTile(output, restorationDesktop(target, output));
        for (const index of saved.path) {
            if (!Number.isInteger(index) || index < 0 || !tile || !tile.tiles || index >= tile.tiles.length) return null;
            tile = tile.tiles[index];
        }
        if (!tile || tile.tiles.length || !["x", "y", "width", "height"].every(function (key) {
            return typeof saved.region[key] === "number" && Math.abs(tile.relativeGeometry[key] - saved.region[key]) < 0.00001;
        })) return null;
        return tile;
    }

    function restoreNativeTile(target, saved, output, resizeOthers, context) {
        if (!saved || !target.resizeable) return false;
        if (saved.kind === "quick") {
            if (!tileActions[saved.edges] || typeof materialWindowStateBridge === "undefined" || typeof workspace.tilingForScreen !== "function") return false;
            const tiles = materialWindowStateBridge.quickTiles(workspace.tilingForScreen(output));
            const originalDesktops = Array.from(target.desktops);
            const desktop = restorationDesktop(target, output);
            if (!desktop) return false;
            const restrictDesktop = target.onAllDesktops || originalDesktops.length > 1;
            try {
                // QuickRootTile's desktop is not exposed through Qt's metaobject.
                // Narrow membership synchronously so native manage() rejects all
                // other roots before modifying an association. Restore membership
                // below without changing the current desktop or activating target.
                if (restrictDesktop) target.desktops = [desktop];
                // Window rules may reject the temporary restriction. In that
                // case leave the window floating instead of choosing a wrong root.
                if (target.onAllDesktops || target.desktops.length !== 1 || target.desktops[0].id !== desktop.id) return false;
                for (let i = 0; i < tiles.length; ++i) {
                    if (quickTileMode(tiles[i]) === saved.edges && manageRestoredTile(target, saved, tiles[i])) {
                        const restoreRegion = function (policy) {
                            const restored = restoreQuickTileRegion(target, saved, tiles[i], output, policy);
                            if (restored) lastSelection[id(target)] = {edges: saved.edges, tile: tiles[i], output: output};
                            return restored;
                        };
                        if (target.tile === tiles[i]) return restoreRegion(resizeOthers);
                        // An inactive desktop owns the association but requests
                        // the tile only when shown. Keep the normal frame until then.
                        const cancel = function () {
                            target.requestedTileChanged.disconnect(whenRequested);
                            context.restorationStopped.disconnect(cancel);
                        };
                        const whenRequested = function () {
                            cancel();
                            if (target.tile !== tiles[i]) return;
                            // Rebind now, while active, so KWin captures the
                            // normal size immediately before requesting the tile.
                            const layout = Array.from(tiles[i].parent.tiles).map(function (tile) {
                                const region = tile.relativeGeometry;
                                return {tile: tile, region: {x: region.x, y: region.y, width: region.width, height: region.height}};
                            });
                            tiles[i].unmanage(target);
                            if (manageRestoredTile(target, saved, tiles[i])) {
                                // Removing the last owner resets the shared splits.
                                layout.forEach(function (entry) { entry.tile.relativeGeometry = entry.region; });
                                restoreRegion(context.resizeOtherTiledWindows);
                            }
                        };
                        context.restorationStopped.connect(cancel);
                        target.requestedTileChanged.connect(whenRequested);
                        return true;
                    }
                }
            } finally {
                if (restrictDesktop) target.desktops = originalDesktops;
            }
        } else if (saved.kind === "custom") {
            const tile = compatibleCustomTile(target, saved, output);
            return !!tile && manageRestoredTile(target, saved, tile);
        }
        return false;
    }

    function vacantPosition(target, output, area, position, size, titleBarHeight) {
        const height = Number.isFinite(titleBarHeight) && titleBarHeight > 0 ? titleBarHeight : 30;
        const step = 1.25 * height;
        function sharesDesktop(other) {
            return target.onAllDesktops || other.onAllDesktops || !target.desktops.length || !other.desktops.length ||
                target.desktops.some(function (desktop) { return other.desktops.some(function (d) { return d.id === desktop.id; }); });
        }
        function sharesActivity(other) {
            const a = target.activities || [], b = other.activities || [];
            return !a.length || !b.length || a.some(function (activity) { return b.indexOf(activity) !== -1; });
        }
        // Include windows awaiting their first buffer, so simultaneous openings
        // also cascade. Read each relevant geometry just once.
        const occupied = workspace.windowList().filter(function (other) {
            return other !== target && other.normalWindow && !other.popupWindow && !other.deleted && !other.minimized &&
                other.output === output && sharesDesktop(other) && sharesActivity(other);
        }).map(function (other) { return other.frameGeometry; });
        const maxX = Math.max(area.x, area.x + area.width - size.width);
        const maxY = Math.max(area.y, area.y + area.height - size.height);
        let x = position.x, y = position.y;
        const visited = new Set();
        for (let attempt = 0; attempt < 64; ++attempt) {
            const key = x + ":" + y;
            if (visited.has(key)) break;
            visited.add(key);
            if (!occupied.some(function (g) { return Math.abs(g.x - x) < height && Math.abs(g.y - y) < height; })) return {x: x, y: y};
            // Wrap at the work-area edges rather than clamping repeatedly to
            // the same occupied corner. Never resize the application window.
            x = x + step <= maxX ? x + step : area.x;
            y = y + step <= maxY ? y + step : area.y;
        }
        return position;
    }

    function restorationOutput(target, state) {
        const screens = workspace.screens;
        const saved = state.rememberScreen !== false &&
            screens.find(function (screen) { return screen.name === state.outputName; });
        return saved || target.output || screens[0];
    }

    function restorationDesktop(target, output) {
        const current = typeof workspace.currentDesktopForScreen === "function" ? workspace.currentDesktopForScreen(output) : workspace.currentDesktop;
        return target.onAllDesktops ? current :
            target.desktops.find(function (d) { return d.id === current.id; }) || target.desktops[0];
    }

    function showRestoredDesktop(target, output) {
        const current = typeof workspace.currentDesktopForScreen === "function" ? workspace.currentDesktopForScreen(output) : workspace.currentDesktop;
        const desktop = restorationDesktop(target, output);
        if (!desktop || desktop.id === current.id) return;
        if (typeof workspace.setCurrentDesktopForScreen === "function") workspace.setCurrentDesktopForScreen(desktop, output);
        else workspace.currentDesktop = desktop;
    }

    function restoreState(target, state) {
        if (!target.normalWindow || target.transient || target.deleted) throw new Error("Target window cannot be restored");
        const position = state && state.position;
        if (!state || state.version !== 3 || !position ||
            ![position.x, position.y].every(function (n) { return typeof n === "number" && Number.isFinite(n); })) throw new Error("Invalid saved position");
        if (!state.desktopIds) throw new Error("Invalid saved state");
        const inherited = state.inheritedTileSize;
        const original = target.frameGeometry;
        const sizeKeys = ["width", "height"];
        const inheritedSize = !target.tile && !target.fullScreen && !target.maximizeMode && target.resizeable && inherited &&
            inherited.normalSize && inherited.tileSize && sizeKeys.every(function (key) {
                return typeof inherited.normalSize[key] === "number" && Number.isFinite(inherited.normalSize[key]) && inherited.normalSize[key] > 0 &&
                    typeof inherited.tileSize[key] === "number" && Number.isFinite(inherited.tileSize[key]) && inherited.tileSize[key] > 0 &&
                    Math.abs(original[key] - inherited.tileSize[key]) <= 2;
            }) ? inherited.normalSize : null;
        if (inheritedSize) {
            // This is a new floating sibling, not a reopening of the old tile.
            // Keep saved placement, but do not clone a saved snap onto it.
            state = Object.assign({}, state);
            delete state.tile;
        }
        const output = restorationOutput(target, state);
        if (!output) throw new Error("No screen is available");
        const desktops = workspace.desktops.filter(function (desktop) { return state.desktopIds.indexOf(desktop.id) !== -1; });

        if (state.rememberDesktop === true) {
            if (state.onAllDesktops) target.onAllDesktops = true;
            else if (desktops.length) target.desktops = desktops;
            // Switch first: activation must not bring the window back here.
            showRestoredDesktop(target, output);
        }
        // Let KWin move app-requested fullscreen, maximized and tiled windows
        // between outputs without undoing their mode or assigning a normal size.
        if (target.fullScreen || target.maximizeMode || target.tile) {
            const restoreTile = !target.fullScreen && !target.maximizeMode && state.tile;
            if (restoreTile && restoreNativeTile(target, state.tile, output, state.resizeOtherTiledWindows === true, state.restoreContext)) return true;
            if (target.output !== output) workspace.sendClientToScreen(target, output);
            return !restoreTile;
        }

        const area = workspace.clientArea(KWin.PlacementArea, output, target.desktops[0] || workspace.currentDesktop);
        const before = state.outputPosition;
        let x = position.x, y = position.y;
        if (before && Number.isFinite(before.x) && Number.isFinite(before.y)) {
            x += output.geometry.x - before.x;
            y += output.geometry.y - before.y;
        }
        // Preserve the app's size except for a matched inherited tile size.
        // Moving to a smaller output still must not shrink a normal window.
        const current = inheritedSize ? {width: inheritedSize.width, height: inheritedSize.height} : original;
        x = Math.max(area.x, Math.min(x, area.x + area.width - current.width));
        y = Math.max(area.y, Math.min(y, area.y + area.height - current.height));
        if (!state.tile) {
            const vacant = vacantPosition(target, output, area, {x: x, y: y}, current, state.titleBarHeight);
            x = vacant.x;
            y = vacant.y;
        }
        // Move first so activation sees the destination output even if the
        // subsequent resize needs a new Wayland buffer. Preserve the app's size.
        if (target.output !== output && (original.width !== current.width || original.height !== current.height)) {
            target.frameGeometry = {x: x, y: y, width: original.width, height: original.height};
        }
        if (original.x !== x || original.y !== y || original.width !== current.width || original.height !== current.height) {
            target.frameGeometry = {x: x, y: y, width: current.width, height: current.height};
        }
        // KWin owns the size of a snapped window; floating app sizes stay intact.
        return !state.tile || restoreNativeTile(target, state.tile, output, state.resizeOtherTiledWindows === true, state.restoreContext);
    }

    materialPrepareWindowState = function (target, state) {
        if (!target.normalWindow || target.transient || target.deleted || state.version !== 3 || !state.desktopIds) return false;
        if (state.rememberDesktop !== true) return true;
        const output = restorationOutput(target, state);
        const current = output && typeof workspace.currentDesktopForScreen === "function" ? workspace.currentDesktopForScreen(output) : workspace.currentDesktop;
        if (state.onAllDesktops) {
            target.onAllDesktops = true;
        } else {
            const desktops = workspace.desktops.filter(function (desktop) { return state.desktopIds.indexOf(desktop.id) !== -1; });
            if (desktops.length) {
                // Before KWin adds/activates the window, keep an off-desktop
                // window out of its activation policy (including "bring here").
                // The C++ controller releases only this temporary minimize flag
                // once the first buffer and complete placement are ready.
                if (!desktops.some(function (desktop) { return desktop.id === current.id; }) && !target.minimized) target.minimized = true;
                target.desktops = desktops;
            }
        }
        return true;
    };

    materialRestoreWindowState = function (target, state) {
        const restored = restoreState(target, state);
        // Even a partial tile failure leaves a real window on its restored
        // desktop; focus policy is independent of protecting the saved tile.
        if (state.rememberDesktop === true && !target.deleted) workspace.activeWindow = target;
        return restored;
    };

    function matchesWindowState(target, state) {
        const saved = state.tile, output = target.output;
        if (!saved || !["quick", "custom"].includes(saved.kind) || !output || output.name !== state.outputName ||
            target.onAllDesktops !== state.onAllDesktops) return false;
        const ids = Array.from(target.desktops, function (desktop) { return desktop.id; }).sort();
        if (ids.join() !== Array.from(state.desktopIds).sort().join()) return false;
        const tile = saved.kind === "custom" ? compatibleCustomTile(target, saved, output) :
            target.tile || materialWindowStateBridge.quickTiles(workspace.tilingForScreen(output)).find(function (tile) {
            return Array.from(tile.windows).indexOf(target) !== -1;
        });
        if (!tile || (saved.kind === "quick" && quickTileMode(tile) !== saved.edges)) return false;
        if (saved.kind === "custom" && target.tile !== tile && Array.from(tile.windows).indexOf(target) === -1) return false;
        const expected = tileFrame(target, saved.region || tile.relativeGeometry, tile, output), frame = target.frameGeometry;
        return ["x", "y", "width", "height"].every(function (key) { return Math.abs(frame[key] - expected[key]) <= 2; });
    }

    materialTiling = function (target, request) {
        if (!target || target.deleted || target.specialWindow) throw new Error("Target window is unavailable");
        if (!target.resizeable) throw new Error("Target window is not resizeable");
        if (request.operation === "layout") return captureLayout(target);
        if (request.operation === "matchWindowState") return matchesWindowState(target, request);
        const action = tileActions[request.edges];
        const custom = request.operation === "tileCustom";
        if (!custom && (request.operation !== "tile" || !action || typeof workspace[action] !== "function")) throw new Error("Unsupported tiling request");
        // Activation can switch desktops; validate both before and after it.
        if (custom) customTile(target, request);
        const output = target.output;
        const tile = target.tile;
        const previous = lastSelection[id(target)];
        workspace.activeWindow = target;
        if (workspace.activeWindow !== target) throw new Error("Target window could not be activated");
        if (custom) {
            // manage() transfers the native association without rewriting the
            // user's layout or restoring geometry on another output first.
            const destination = customTile(target, request);
            if (!destination.manage(target)) throw new Error("Custom tile could not manage the window");
            delete lastSelection[id(target)];
            return true;
        }
        // Keep a resized native tile when the user selects it again, without
        // invoking a shortcut that might combine modes or switch monitors.
        if (tile && previous && previous.edges === request.edges && previous.tile === tile && previous.output === output) return true;
        if (tile) {
            if (typeof tile.unmanage !== "function") throw new Error("This KWin version lacks tile.unmanage");
            tile.unmanage(target);
            // Restore geometry may refer to a different monitor. Keep tiling on
            // the output which contained the window when the request arrived.
            if (target.output !== output) workspace.sendClientToScreen(target, output);
        }
        workspace[action]();
        if (!target.tile) throw new Error("KWin did not assign a native tile");
        lastSelection[id(target)] = {edges: request.edges, tile: target.tile, output: output};
        return true;
    };

})();
