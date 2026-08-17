#pragma once

#include <QObject>
#include <QVariantMap>

namespace Material
{

class DetectDialog : public QObject
{
    Q_OBJECT

public:
    explicit DetectDialog(QObject *parent = nullptr);

    void detect();
    QVariantMap properties() const;

Q_SIGNALS:
    void detectionDone(bool success);

private:
    QVariantMap m_properties;
};

} // namespace Material
