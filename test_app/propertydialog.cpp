#include "propertydialog.h"
#include <QVBoxLayout>

PropertyDialog::PropertyDialog(const QString &title, bool isMenu, QWidget *parent)
    : QDialog(parent)
    , m_textEdit(nullptr)
    , m_iconEdit(nullptr)
    , m_shortcutEdit(nullptr)
    , m_checkableCheck(nullptr)
    , m_radioCheck(nullptr)
    , m_enabledCheck(nullptr)
    , m_isSubmenuCheck(nullptr)
    , m_isSeparatorCheck(nullptr)
{
    setWindowTitle(title);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QFormLayout *formLayout = new QFormLayout();

    m_textEdit = new QLineEdit(this);
    formLayout->addRow("Text:", m_textEdit);

    if (!isMenu) {
        m_iconEdit = new QLineEdit(this);
        formLayout->addRow("Icon Name:", m_iconEdit);

        m_shortcutEdit = new QLineEdit(this);
        formLayout->addRow("Shortcut:", m_shortcutEdit);

        m_checkableCheck = new QCheckBox("Checkable", this);
        formLayout->addRow(m_checkableCheck);

        m_radioCheck = new QCheckBox("Part of Radio Group", this);
        formLayout->addRow(m_radioCheck);

        m_isSubmenuCheck = new QCheckBox("Is Submenu", this);
        formLayout->addRow(m_isSubmenuCheck);

        m_isSeparatorCheck = new QCheckBox("Is Separator", this);
        formLayout->addRow(m_isSeparatorCheck);
    }

    m_enabledCheck = new QCheckBox("Enabled", this);
    m_enabledCheck->setChecked(true);
    formLayout->addRow(m_enabledCheck);

    mainLayout->addLayout(formLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void PropertyDialog::setProperties(const Properties &props)
{
    m_textEdit->setText(props.text);
    if (m_iconEdit) m_iconEdit->setText(props.icon);
    if (m_shortcutEdit) m_shortcutEdit->setText(props.shortcut);
    if (m_checkableCheck) m_checkableCheck->setChecked(props.checkable);
    if (m_radioCheck) m_radioCheck->setChecked(props.radio);
    if (m_isSubmenuCheck) m_isSubmenuCheck->setChecked(props.isSubmenu);
    if (m_isSeparatorCheck) m_isSeparatorCheck->setChecked(props.isSeparator);
    m_enabledCheck->setChecked(props.enabled);
}

PropertyDialog::Properties PropertyDialog::properties() const
{
    Properties props;
    props.text = m_textEdit->text();
    if (m_iconEdit) props.icon = m_iconEdit->text();
    if (m_shortcutEdit) props.shortcut = m_shortcutEdit->text();
    if (m_checkableCheck) props.checkable = m_checkableCheck->isChecked();
    if (m_radioCheck) props.radio = m_radioCheck->isChecked();
    if (m_isSubmenuCheck) props.isSubmenu = m_isSubmenuCheck->isChecked();
    if (m_isSeparatorCheck) props.isSeparator = m_isSeparatorCheck->isChecked();
    props.enabled = m_enabledCheck->isChecked();
    return props;
}
