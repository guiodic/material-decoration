#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QActionGroup>
#include <QMap>
#include <QMenu>
#include <QAction>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

private slots:
    void addTopLevelMenu();
    void addAction();
    void modifyTopLevelMenu();
    void modifyAction();
    void addCheckableAction();
    void addRadioAction();
    void removeTopLevelMenu();
    void removeAction();
    void updateActionsCombo();
    void updateMenusCombo();

private:
    QMenu *currentMenu() const;
    QAction *currentAction() const;

    QComboBox *m_menusCombo;
    QComboBox *m_actionsCombo;
    
    // To keep track of radio groups per menu if needed, 
    // but for simplicity, we'll use one global group or one per menu.
    // The requirement says "a radio button", let's use a single group for now.
    QMap<QMenu*, QActionGroup*> m_menuRadioGroups;
};

#endif // MAINWINDOW_H
