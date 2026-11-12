# Window restoration

## 1. How it works

**Identify the window.** The decoration reads Qt properties from its parent,
the KWin window. A SHA-256 key combines the application's `desktopFileName`
(falling back to `resourceClass`), window role and Wayland tag. Window titles
are not used. Windows without distinct roles or tags share the same state.

**Save the state.** KConfig stores JSON in
`$XDG_STATE_HOME/material-window-staterc` (normally
`~/.local/state/material-window-staterc`). Each window key has separate entries
for display and virtual-desktop configurations. The configuration hash includes
screen identity, geometry and scale, plus desktop UUIDs and grid dimensions.
The current desktop and desktop names do not affect it.

State is written synchronously when a window closes and before configuration
changes relocate windows. Signals maintain the necessary information in memory;
there is no polling or disk write on every movement.

**Restore the state.** The controller reads the matching entry and calls the
Material Tiling Popup script synchronously inside KWin's JavaScript engine.
When desktop restoration is enabled, membership is prepared before mapping.
Placement and native tiling are restored when the window becomes ready for
painting, before rendering resumes.
If the script is unavailable, bounded retries run for about five seconds.
Availability after the first frame can still cause a visible repositioning.

## 2. Current restoration policies

- **Eligibility:** experimental, disabled by default, and enabled in the KCM's
  Window Management tab. Requires the Material Tiling Popup KWin script. Applies
  to normal, non-transient Wayland windows that use this decoration; excludes
  X11/XWayland windows, dialogs and popups. Per-window exceptions can disable it.
- **Restored data:** position and native quick or custom tiling. Custom tiling
  requires a compatible layout. The two subordinate options are independent:
  **Remember the screen where the window was last closed** is on by default;
  when off, placement uses the screen KWin chose for the new window.
  **Remember the virtual desktop where the window was last closed** is off by
  default; when on, it restores membership (including all desktops), switches
  to a destination desktop and activates the window. Otherwise, membership and
  focus are left to KWin. For multiple saved desktops, the current one is used
  if included, otherwise the first available saved desktop. Sticky windows do
  not require a desktop switch.
- **Sizes and modes:** floating sizes belong to the application. For tiled
  windows, the known normal size is saved only as tile metadata, allowing
  unsnapping after reopening. A new floating sibling can recover that size if
  its initial dimensions match a live tiled sibling and the normal size is
  unambiguous. App-requested maximization and fullscreen take precedence;
  maximized, fullscreen and minimized states are not persisted.
- **Placement:** positions are translated relative to the saved screen origin
  and constrained to the available work area. Floating windows cascade to avoid nearby placements
  by 1.25 times the title-bar height, with a 30-pixel fallback when unavailable.
  Collision checks consider matching screen, desktop and activity contexts.
- **Disconnected screens:** automatic relocation to a remaining screen preserves
  the old configuration's placement. An interactive move or resize afterwards
  also updates the preceding configuration at closure or the next configuration
  change, provided the destination screen existed there. Reconnecting the
  original layout then uses that deliberate placement. Automatic geometry
  changes and activation alone do not count as user intervention.
- **Shared identity and failures:** normally, the last closed window supplies
  the shared state. A failed restoration protects pre-existing data, including
  a tile that could not be restored. Interactive movement/resizing cancels
  pending restoration and permits saving the user's placement.
- **Desktop identity:** deleted and recreated desktops receive new UUIDs and
  therefore form a different configuration, even with identical names.

When desktop restoration is enabled, a temporary minimization guard prevents
initial activation from moving an off-desktop window to the current desktop. Removing it can lose a concurrent
application minimize request; the accessible API cannot distinguish the two.
