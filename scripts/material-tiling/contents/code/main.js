// SPDX-License-Identifier: GPL-2.0-or-later
(function () {
    "use strict";
    const service = "com.github.guiodic.MaterialTiling";
    const path = "/Tiling";
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
    function id(window) { return String(window.internalId).replace(/[{}]/g, "").toLowerCase(); }
    workspace.windowRemoved.connect(function (window) { delete lastSelection[id(window)]; });

    function apply(request) {
        const target = workspace.windowList().find(function (window) { return id(window) === request.uuid; });
        if (!target || !target.resizeable || target.specialWindow) throw new Error("Target window is unavailable");
        if (request.operation === "ping") return;
        const action = tileActions[request.edges];
        if (request.operation !== "tile" || !action || typeof workspace[action] !== "function") throw new Error("Unsupported tiling request");
        const output = target.output;
        const tile = target.tile;
        const previous = lastSelection[request.uuid];
        workspace.activeWindow = target;
        if (workspace.activeWindow !== target) throw new Error("Target window could not be activated");
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
            try { apply(request); } catch (error) { success = false; reason = String(error); }
            callDBus(service, path, service, "complete", request.id, success, reason);
        });
    });
})();
