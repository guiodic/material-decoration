// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/Private/DecoratedWindowPrivate>
#include <KDecoration3/Private/DecorationBridge>
#include <KDecoration3/Private/DecorationSettingsPrivate>

namespace MaterialTest
{
// Supply compositor state to a real KDecoration DecoratedWindow.
class Window : public KDecoration3::DecoratedWindowPrivate
{
public:
    Window(KDecoration3::DecoratedWindow *window, KDecoration3::Decoration *decoration)
        : DecoratedWindowPrivate(window, decoration) {}
    bool active = true;
    bool resizeable = true;
    bool maximizedHorizontal = false;
    bool maximizedVertical = false;
    bool shaded = false;
    Qt::Edges screenEdges;
    qreal outputScale = 1;

    void setResizeable(bool value) {
        resizeable = value;
        Q_EMIT window()->resizeableChanged(value);
    }
    QPalette colors;

    bool isActive() const override { return active; }
    QString caption() const override { return {}; }
    bool isOnAllDesktops() const override { return false; }
    bool isShaded() const override { return shaded; }
    QIcon icon() const override { return {}; }
    bool isMaximized() const override { return maximizedHorizontal && maximizedVertical; }
    bool isMaximizedHorizontally() const override { return maximizedHorizontal; }
    bool isMaximizedVertically() const override { return maximizedVertical; }
    bool isKeepAbove() const override { return false; }
    bool isKeepBelow() const override { return false; }
    bool isCloseable() const override { return true; }
    bool isMaximizeable() const override { return true; }
    bool isMinimizeable() const override { return true; }
    bool providesContextHelp() const override { return false; }
    bool isModal() const override { return false; }
    bool isShadeable() const override { return false; }
    bool isMoveable() const override { return true; }
    bool isResizeable() const override { return resizeable; }
    qreal width() const override { return 640; }
    qreal height() const override { return 480; }
    QSizeF size() const override { return {width(), height()}; }
    QPalette palette() const override { return colors; }
    QColor color(KDecoration3::ColorGroup group, KDecoration3::ColorRole role) const override {
        return colors.color(group == KDecoration3::ColorGroup::Active ? QPalette::Active : QPalette::Inactive,
                            role == KDecoration3::ColorRole::Foreground ? QPalette::WindowText : QPalette::Window);
    }
    Qt::Edges adjacentScreenEdges() const override { return screenEdges; }
    qreal scale() const override { return outputScale; }
    qreal nextScale() const override { return outputScale; }
    QString windowClass() const override { return {}; }
    bool hasApplicationMenu() const override { return false; }
    bool isApplicationMenuActive() const override { return false; }
    void requestShowToolTip(const QString &) override {}
    void requestHideToolTip() override {}
    void requestClose() override {}
    void requestToggleMaximization(Qt::MouseButtons) override {}
    void requestMinimize() override {}
    void requestContextHelp() override {}
    void requestToggleOnAllDesktops() override {}
    void requestToggleShade() override {}
    void requestToggleKeepAbove() override {}
    void requestToggleKeepBelow() override {}
    void requestShowWindowMenu(const QRect &) override {}
    void showApplicationMenu(int) override {}
    void requestShowApplicationMenu(const QRect &, int) override {}
};

class Settings : public KDecoration3::DecorationSettingsPrivate
{
public:
    explicit Settings(KDecoration3::DecorationSettings *settings) : DecorationSettingsPrivate(settings) {}
    bool isOnAllDesktopsAvailable() const override { return false; }
    bool isAlphaChannelSupported() const override { return true; }
    bool isCloseOnDoubleClickOnMenu() const override { return false; }
    QList<KDecoration3::DecorationButtonType> leftButtons;
    QList<KDecoration3::DecorationButtonType> rightButtons;
    QList<KDecoration3::DecorationButtonType> decorationButtonsLeft() const override { return leftButtons; }
    QList<KDecoration3::DecorationButtonType> decorationButtonsRight() const override { return rightButtons; }
    KDecoration3::BorderSize borderMode = KDecoration3::BorderSize::Normal;
    KDecoration3::BorderSize borderSize() const override { return borderMode; }
};

class Bridge : public KDecoration3::DecorationBridge
{
public:
    Window *client = nullptr;
    Settings *options = nullptr;
    std::unique_ptr<KDecoration3::DecoratedWindowPrivate> createClient(
        KDecoration3::DecoratedWindow *window, KDecoration3::Decoration *decoration) override {
        auto backend = std::make_unique<Window>(window, decoration);
        client = backend.get();
        return backend;
    }
    std::unique_ptr<KDecoration3::DecorationSettingsPrivate> settings(KDecoration3::DecorationSettings *settings) override {
        auto backend = std::make_unique<Settings>(settings);
        options = backend.get();
        return backend;
    }
};
}
