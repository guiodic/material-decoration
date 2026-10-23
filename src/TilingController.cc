// SPDX-License-Identifier: GPL-2.0-or-later
#include "TilingController.h"
#include "BuildConfig.h"

#if HAVE_WAYLAND
#include <window.h>
#include <workspace.h>
#endif

namespace Material
{
#if HAVE_WAYLAND
static KWin::Window *findWindow(KDecoration3::Decoration *decoration)
{
    auto *workspace = KWin::workspace();
    if (!workspace || !decoration) return nullptr;
    return workspace->findWindow([decoration](const KWin::Window *window) {
        return window->decoration() == decoration;
    });
}
#endif

bool canTileWindow(KDecoration3::Decoration *decoration)
{
#if HAVE_WAYLAND
    const auto *window = findWindow(decoration);
    return window && window->isResizable() && !window->isSpecialWindow();
#else
    Q_UNUSED(decoration)
    return false;
#endif
}

bool tileWindow(KDecoration3::Decoration *decoration, Qt::Edges edges)
{
#if HAVE_WAYLAND
    auto *window = findWindow(decoration);
    if (!window || !window->isResizable() || window->isSpecialWindow()) return false;
    KWin::QuickTileMode mode;
    if (edges.testFlag(Qt::LeftEdge)) mode |= KWin::QuickTileFlag::Left;
    if (edges.testFlag(Qt::RightEdge)) mode |= KWin::QuickTileFlag::Right;
    if (edges.testFlag(Qt::TopEdge)) mode |= KWin::QuickTileFlag::Top;
    if (edges.testFlag(Qt::BottomEdge)) mode |= KWin::QuickTileFlag::Bottom;
    if (!mode || (edges.testFlag(Qt::LeftEdge) && edges.testFlag(Qt::RightEdge))
        || (edges.testFlag(Qt::TopEdge) && edges.testFlag(Qt::BottomEdge))) return false;
    // Set a specific mode, rather than invoke a shortcut which can toggle it or
    // act on the popup/another active window. KWin handles work areas and restore.
    window->setQuickTileMode(mode, window->frameGeometry().center());
    return true;
#else
    Q_UNUSED(decoration)
    Q_UNUSED(edges)
    return false;
#endif
}
}
