#include "mainwindow.h"
#include "propertydialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QKeySequence>
#include <QMessageBox>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Dynamic Menu Test App");
    resize(600, 400);

    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *layout = new QVBoxLayout(central);

    // Selection UI
    QHBoxLayout *selectionLayout = new QHBoxLayout();
    selectionLayout->addWidget(new QLabel("Menu:"));
    m_menusCombo = new QComboBox(this);
    selectionLayout->addWidget(m_menusCombo);
    
    selectionLayout->addWidget(new QLabel("Action:"));
    m_actionsCombo = new QComboBox(this);
    selectionLayout->addWidget(m_actionsCombo);
    layout->addLayout(selectionLayout);

    connect(m_menusCombo, &QComboBox::currentIndexChanged, this, &MainWindow::updateActionsCombo);

    // Buttons
    auto addButton = [&](const QString &text, auto slot) {
        QPushButton *btn = new QPushButton(text, this);
        layout->addWidget(btn);
        connect(btn, &QPushButton::clicked, this, slot);
        return btn;
    };

    addButton("Add Top-level Menu", &MainWindow::addTopLevelMenu);
    addButton("Add Action to Selected Menu", &MainWindow::addAction);
    addButton("Modify Selected Top-level Menu", &MainWindow::modifyTopLevelMenu);
    addButton("Modify Selected Action", &MainWindow::modifyAction);
    addButton("Add Checkable Action", &MainWindow::addCheckableAction);
    addButton("Add Radio Action", &MainWindow::addRadioAction);
    addButton("Remove Selected Top-level Menu", &MainWindow::removeTopLevelMenu);
    addButton("Remove Selected Action", &MainWindow::removeAction);

    layout->addStretch();

    updateMenusCombo();
}

void MainWindow::updateMenusCombo()
{
    QMenu *prev = currentMenu();
    m_menusCombo->clear();
    
    auto addMenus = [&](auto self, QMenu *parentMenu, const QString &prefix) -> void {
        for (QAction *action : parentMenu->actions()) {
            if (action->menu()) {
                QString title = prefix + action->menu()->title();
                m_menusCombo->addItem(title, QVariant::fromValue((void*)action->menu()));
                self(self, action->menu(), title + " > ");
            }
        }
    };

    for (QAction *action : menuBar()->actions()) {
        if (action->menu()) {
            m_menusCombo->addItem(action->menu()->title(), QVariant::fromValue((void*)action->menu()));
            addMenus(addMenus, action->menu(), action->menu()->title() + " > ");
        }
    }
    
    if (prev) {
        int index = m_menusCombo->findData(QVariant::fromValue((void*)prev));
        if (index != -1) m_menusCombo->setCurrentIndex(index);
    }
    updateActionsCombo();
}

void MainWindow::updateActionsCombo()
{
    QAction *prev = currentAction();
    m_actionsCombo->clear();
    
    QMenu *menu = currentMenu();
    if (menu) {
        for (QAction *action : menu->actions()) {
            if (action->menu()) continue; // Skip submenus
            QString text = action->text();
            if (text.isEmpty() && action->isSeparator()) text = "--- Separator ---";
            m_actionsCombo->addItem(text, QVariant::fromValue((void*)action));
        }
    }

    if (prev) {
        int index = m_actionsCombo->findData(QVariant::fromValue((void*)prev));
        if (index != -1) m_actionsCombo->setCurrentIndex(index);
    }
}

QMenu *MainWindow::currentMenu() const
{
    int idx = m_menusCombo->currentIndex();
    if (idx == -1) return nullptr;
    return (QMenu*)m_menusCombo->itemData(idx).value<void*>();
}

QAction *MainWindow::currentAction() const
{
    int idx = m_actionsCombo->currentIndex();
    if (idx == -1) return nullptr;
    return (QAction*)m_actionsCombo->itemData(idx).value<void*>();
}

void MainWindow::addTopLevelMenu()
{
    PropertyDialog dlg("Add Top-level Menu", true, this);
    if (dlg.exec() == QDialog::Accepted) {
        PropertyDialog::Properties props = dlg.properties();
        QMenu *menu = menuBar()->addMenu(props.text);
        menu->setEnabled(props.enabled);
        updateMenusCombo();
    }
}

void MainWindow::addAction()
{
    QMenu *menu = currentMenu();
    if (!menu) {
        QMessageBox::warning(this, "Error", "No menu selected");
        return;
    }

    PropertyDialog dlg("Add Action", false, this);
    if (dlg.exec() == QDialog::Accepted) {
        PropertyDialog::Properties props = dlg.properties();
        if (props.isSubmenu) {
            menu->addMenu(props.text);
            updateMenusCombo();
        } else if (props.isSeparator) {
            menu->addSeparator();
            updateActionsCombo();
        } else {
            QAction *action = menu->addAction(props.text);
            action->setIcon(QIcon::fromTheme(props.icon));
            action->setShortcut(QKeySequence(props.shortcut));
            action->setCheckable(props.checkable);
            action->setEnabled(props.enabled);
            
            if (props.radio) {
                if (!m_menuRadioGroups.contains(menu)) {
                    m_menuRadioGroups[menu] = new QActionGroup(menu);
                }
                action->setActionGroup(m_menuRadioGroups[menu]);
                action->setCheckable(true);
            }
            updateActionsCombo();
        }
    }
}

void MainWindow::modifyTopLevelMenu()
{
    QMenu *menu = currentMenu();
    if (!menu) return;

    PropertyDialog dlg("Modify Top-level Menu", true, this);
    PropertyDialog::Properties props;
    props.text = menu->title();
    props.enabled = menu->isEnabled();
    dlg.setProperties(props);

    if (dlg.exec() == QDialog::Accepted) {
        props = dlg.properties();
        menu->setTitle(props.text);
        menu->setEnabled(props.enabled);
        updateMenusCombo();
    }
}

void MainWindow::modifyAction()
{
    QAction *action = currentAction();
    if (!action) return;

    PropertyDialog dlg("Modify Action", false, this);
    PropertyDialog::Properties props;
    props.text = action->text();
    props.icon = action->icon().name();
    props.shortcut = action->shortcut().toString();
    props.checkable = action->isCheckable();
    props.radio = (action->actionGroup() != nullptr);
    props.enabled = action->isEnabled();
    dlg.setProperties(props);

    if (dlg.exec() == QDialog::Accepted) {
        props = dlg.properties();
        action->setText(props.text);
        action->setIcon(QIcon::fromTheme(props.icon));
        action->setShortcut(QKeySequence(props.shortcut));
        action->setCheckable(props.checkable);
        action->setEnabled(props.enabled);

        if (props.radio) {
            QMenu *menu = currentMenu();
            if (!m_menuRadioGroups.contains(menu)) {
                m_menuRadioGroups[menu] = new QActionGroup(menu);
            }
            action->setActionGroup(m_menuRadioGroups[menu]);
            action->setCheckable(true);
        } else {
            action->setActionGroup(nullptr);
        }
        
        updateActionsCombo();
    }
}

void MainWindow::addCheckableAction()
{
    QMenu *menu = currentMenu();
    if (!menu) return;

    PropertyDialog dlg("Add Checkable Action", false, this);
    PropertyDialog::Properties props;
    props.checkable = true;
    dlg.setProperties(props);

    if (dlg.exec() == QDialog::Accepted) {
        props = dlg.properties();
        QAction *action = menu->addAction(props.text);
        action->setCheckable(true);
        action->setIcon(QIcon::fromTheme(props.icon));
        action->setShortcut(QKeySequence(props.shortcut));
        action->setEnabled(props.enabled);
        updateActionsCombo();
    }
}

void MainWindow::addRadioAction()
{
    QMenu *menu = currentMenu();
    if (!menu) return;

    PropertyDialog dlg("Add Radio Action", false, this);
    PropertyDialog::Properties props;
    props.radio = true;
    props.checkable = true;
    dlg.setProperties(props);

    if (dlg.exec() == QDialog::Accepted) {
        props = dlg.properties();
        QAction *action = menu->addAction(props.text);
        if (!m_menuRadioGroups.contains(menu)) {
            m_menuRadioGroups[menu] = new QActionGroup(menu);
        }
        action->setActionGroup(m_menuRadioGroups[menu]);
        action->setCheckable(true);
        action->setIcon(QIcon::fromTheme(props.icon));
        action->setShortcut(QKeySequence(props.shortcut));
        action->setEnabled(props.enabled);
        updateActionsCombo();
    }
}

void MainWindow::removeTopLevelMenu()
{
    QMenu *menu = currentMenu();
    if (!menu) return;

    if (QMessageBox::question(this, "Confirm", "Remove menu " + menu->title() + "?") == QMessageBox::Yes) {
        m_menuRadioGroups.remove(menu);
        menu->deleteLater();
        // Use a timer or single shot to update combo because deleteLater is deferred
        QMetaObject::invokeMethod(this, "updateMenusCombo", Qt::QueuedConnection);
    }
}

void MainWindow::removeAction()
{
    QAction *action = currentAction();
    if (!action) return;

    if (QMessageBox::question(this, "Confirm", "Remove action " + action->text() + "?") == QMessageBox::Yes) {
        action->deleteLater();
        QMetaObject::invokeMethod(this, "updateActionsCombo", Qt::QueuedConnection);
    }
}

