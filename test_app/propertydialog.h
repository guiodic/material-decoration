#ifndef PROPERTYDIALOG_H
#define PROPERTYDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QFormLayout>
#include <QDialogButtonBox>

class PropertyDialog : public QDialog {
    Q_OBJECT

public:
    struct Properties {
        QString text;
        QString icon;
        QString shortcut;
        bool checkable = false;
        bool radio = false;
        bool enabled = true;
        bool isSubmenu = false;
        bool isSeparator = false;
    };

    explicit PropertyDialog(const QString &title, bool isMenu, QWidget *parent = nullptr);

    void setProperties(const Properties &props);
    Properties properties() const;

private:
    QLineEdit *m_textEdit;
    QLineEdit *m_iconEdit;
    QLineEdit *m_shortcutEdit;
    QCheckBox *m_checkableCheck;
    QCheckBox *m_radioCheck;
    QCheckBox *m_enabledCheck;
    QCheckBox *m_isSubmenuCheck;
    QCheckBox *m_isSeparatorCheck;
};

#endif // PROPERTYDIALOG_H
