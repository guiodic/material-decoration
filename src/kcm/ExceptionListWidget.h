#pragma once

#include "ExceptionModel.h"
#include <QWidget>
#include <memory>

namespace Ui
{
class MaterialExceptionListWidget;
}

namespace Material
{

class ExceptionListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ExceptionListWidget(QWidget *parent = nullptr);
    ~ExceptionListWidget() override;

    void setExceptions(const InternalSettingsList &exceptions);
    InternalSettingsList exceptions();

    bool isChanged() const { return m_changed; }

Q_SIGNALS:
    void changed(bool changed);

protected:
    const ExceptionModel &model() const { return m_model; }
    ExceptionModel &model() { return m_model; }

protected Q_SLOTS:
    void updateButtons();
    void add();
    void edit();
    void remove();
    void toggle(const QModelIndex &index);
    void up();
    void down();

protected:
    void resizeColumns() const;
    bool checkException(InternalSettingsPtr exception);
    void setChanged(bool value);

private:
    ExceptionModel m_model;
    std::unique_ptr<Ui::MaterialExceptionListWidget> m_ui;
    bool m_changed = false;
};

} // namespace Material
