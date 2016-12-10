#ifndef PARTITIONMANAGERWINDOW_H
#define PARTITIONMANAGERWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QProgressBar>
#include <QStatusBar>
#include <QThread>

#include "gui/PartitionManagerAdapter.h"



class PartitionManagerWindow : public QMainWindow
{
    Q_OBJECT
    PartitionManagerAdapter * pma;

    QWidget * centralWidget;
    QLabel          * deviceLabel;
    QComboBox       * deviceSelector;
    QToolButton     * refreshButton;
    QPushButton     * wipeButton;
    QPushButton     * createButton;
    QPushButton     * mountButton;
    QPushButton     * unmountButton;
    QPushButton     * ejectButton;
    QStatusBar      * statusBar;

    QThread pmaThread;
    void setupUI();
    void setupPMA();



public:
    explicit PartitionManagerWindow(QWidget *parent = 0);
    ~PartitionManagerWindow();
signals:
    void devChangeRequested(const QString& devName);
    void wipeRequested();
    void createRequested(const unsigned short slot,
                         const QString &password);
    void mountRequested(const QString &password);
    void unmountRequested();
    void ejectRequested();
public slots:
    void setDevice(const QString& devName);
    void wipeDevice();
    void createPartition();
    void mountPartition();
    void unmountPartition();
    void ejectDevice();
    void updateUI(const State state);
    void showErrorMessage(QString msg);
    void populateDeviceSelector();
};

#endif // PARTITIONMANAGERWINDOW_H
