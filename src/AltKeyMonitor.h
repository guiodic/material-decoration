// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <memory>

namespace Material
{

class AltKeyMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool altPressed READ isAltPressed NOTIFY altPressedChanged)

public:
    static AltKeyMonitor *instance();
    ~AltKeyMonitor() override;
    bool isAltPressed() const;

signals:
    void altPressedChanged(bool pressed);

private:
    explicit AltKeyMonitor(QObject *parent);
    void setAltPressed(bool pressed);

    class Private;
    bool m_altPressed = false;
    std::unique_ptr<Private> d;
};

}
