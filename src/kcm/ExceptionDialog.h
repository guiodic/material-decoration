#pragma once

#include "ExceptionList.h"
#include <QDialog>
#include <QMap>
#include <memory>

class QCheckBox;

namespace Ui
{
class MaterialExceptionDialog;
}

namespace Material
{

class DetectDialog;

class ExceptionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExceptionDialog(QWidget *parent = nullptr);
    ~ExceptionDialog() override;

    void setException(InternalSettingsPtr exception);
    void save();

    bool isChanged() const { return m_changed; }

Q_SIGNALS:
    void changed(bool changed);

protected:
    void setChanged(bool value);

private Q_SLOTS:
    void updateChanged();
    void selectWindowProperties();
    void readWindowProperties(bool success);

private:
    using CheckBoxMap = QMap<ExceptionMask, QCheckBox *>;

    std::unique_ptr<Ui::MaterialExceptionDialog> m_ui;
    CheckBoxMap m_checkboxes;
    InternalSettingsPtr m_exception;
    DetectDialog *m_detectDialog = nullptr;
    bool m_changed = false;
};

} // namespace Material
