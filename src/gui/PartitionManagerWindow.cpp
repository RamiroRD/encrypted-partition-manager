#include <QMessageBox>
#include <QInputDialog>
#include <QProgressDialog>
#include <QApplication>

#include "gui/PartitionManagerWindow.h"
#include "gui/CreateDialog.h"


PartitionManagerWindow::PartitionManagerWindow(QWidget *parent)
    : QMainWindow(parent),
      pma               (new PartitionManagerAdapter()), // Tiene que no tener padre!
      centralWidget     (new QWidget()),
      gridLayout        (new QGridLayout()),
      deviceLabel       (new QLabel(tr("Device:"))),
      deviceSelector    (new QComboBox(centralWidget)),
      wipeButton        (new QPushButton(tr("Erase device"))),
      createButton      (new QPushButton(tr("Create partition"))),
      mountButton       (new QPushButton(tr("Mount partition"))),
      unmountButton     (new QPushButton(tr("Unmount partition"))),
      ejectButton       (new QPushButton(tr("Eject device"))),
      statusBar         (new QStatusBar())
{
    this->setupUI();
    this->setupPMA();
}

void PartitionManagerWindow::setupUI()
{
    this->setWindowTitle(tr("Partition Manager"));

    gridLayout->addWidget(deviceLabel,      0,0,1,-1);
    gridLayout->addWidget(deviceSelector,   1,0,1,-1);
    gridLayout->addWidget(wipeButton,       2,0,1, 1);
    gridLayout->addWidget(createButton,     2,1,1, 1);
    gridLayout->addWidget(mountButton,      2,2,1, 1);
    gridLayout->addWidget(unmountButton,    2,3,1, 1);
    gridLayout->addWidget(ejectButton,      2,4,1, 1);

    deviceLabel->setSizePolicy(QSizePolicy());
    /*
     * Al hacer esto, el parentesco queda:
     * this <- centralWidget -< x
     * donde x son los widgets que se terminan mostrando.
     * NO es necesario definir el parentesco al crear cada widget.
     * setLayout hace que centralWidget los "adopte".
     * setCentralWidget hace lo mismo con centralWidget.
     */
    centralWidget->setLayout(gridLayout);
    this->setCentralWidget(centralWidget);
    this->setStatusBar(statusBar);
    statusBar->showMessage(tr("Select a device."));

    wipeButton->setDisabled(true);
    createButton->setDisabled(true);
    mountButton->setDisabled(true);
    unmountButton->setDisabled(true);
    ejectButton->setDisabled(true);

    // Test
    deviceSelector->addItem("/dev/sda");
    deviceSelector->addItem("/dev/sdb");
    deviceSelector->addItem("/dev/sdc");
    deviceSelector->setCurrentIndex(-1);
}

void PartitionManagerWindow::setupPMA()
{
    /*
     * Las llamadas a los métodos del PMA se hacen usando signals y slots.
     * Esta es la "forma Qt" de hacer llamadas a métodos en otro Thread;
     * El objeto "reside" en el otro thread y hacer llamadas directas no
     * cambia de threads.
     */
    pma->moveToThread(&pmaThread);


    /*
     * Conectamos los botones del esta GUI a los métodos que toman las primeras
     * acciones, como pedir entrada y verificarlas.
     */
    connect(deviceSelector,&QComboBox::currentTextChanged,
            this,&PartitionManagerWindow::setDevice);
    connect(wipeButton,&QPushButton::clicked,
            this,&PartitionManagerWindow::wipeDevice);
    connect(createButton,&QPushButton::clicked,
            this,&PartitionManagerWindow::createPartition);
    connect(mountButton,&QPushButton::clicked,
            this,&PartitionManagerWindow::mountPartition);
    connect(unmountButton,&QPushButton::clicked,
            this,&PartitionManagerWindow::unmountPartition);
    connect(ejectButton,&QPushButton::clicked,
            this,&PartitionManagerWindow::ejectDevice);

    /*
     * Conectamos las señales que se emiten al momento de necesitar métodos de
     * PMA. Los métodos públicos en PMA son todos slots y por eso necesitamos
     * señales locales.
     */
    // Habría que usar QueuedConnection?
    connect(this, &PartitionManagerWindow::devChangeRequested,
            pma, &PartitionManagerAdapter::setDevice);
    connect(this, &PartitionManagerWindow::wipeRequested,
            pma, &PartitionManagerAdapter::wipeDevice);
    connect(this, &PartitionManagerWindow::createRequested,
            pma, &PartitionManagerAdapter::createPartition);
    connect(this, &PartitionManagerWindow::mountRequested,
            pma, &PartitionManagerAdapter::mountPartition);
    connect(this, &PartitionManagerWindow::unmountRequested,
            pma, &PartitionManagerAdapter::unmountPartition);
    connect(this, &PartitionManagerWindow::ejectRequested,
            pma, &PartitionManagerAdapter::ejectDevice);

    // Ante cambios de estados actualizamos el enabled de los widgets
    connect(pma, &PartitionManagerAdapter::stateChanged,
            this, &PartitionManagerWindow::updateUI);

    connect(&pmaThread, &QThread::finished, pma, &QObject::deleteLater);


    // Arrancamos el PMA
    pmaThread.start();

}

void PartitionManagerWindow::setDevice(const QString& devName)
{
    emit devChangeRequested(devName);
}

void PartitionManagerWindow::wipeDevice()
{
    auto reply = QMessageBox::question(this,
                                       tr("Confirm"),
                                       tr("Do you want to erase all contents from this device?"),
                                       QMessageBox::Yes | QMessageBox::No,
                                       QMessageBox::No);
    if(reply == QMessageBox::Yes) {
        QProgressDialog pd (tr("Erasing volume."), "Cancel", 0, 100,this);
        connect(pma,&PartitionManagerAdapter::finished,
                &pd,&QProgressDialog::accept);
        emit wipeRequested();
        pd.exec();
    }
}

void PartitionManagerWindow::createPartition()
{
    CreateDialog dialog(this);
    dialog.show();
    if(dialog.exec() == QDialog::Accepted)
    {
        QProgressDialog pd (tr("Creating partition..."), "", 0,0,this);
        pd.setCancelButton(nullptr);
        emit createRequested(dialog.getSlot(),
                             dialog.getPassword());
        connect(pma,&PartitionManagerAdapter::finished,
                &pd,&QProgressDialog::accept);

        pd.exec();
    }

}

void PartitionManagerWindow::mountPartition()
{
    bool ok;
    QString password =
            QInputDialog::getText(this,
                                  tr("Enter password"),
                                  tr("Password:"),
                                  QLineEdit::Password, "", &ok);
    if(ok)
    {
        QProgressDialog pd (tr("Searching partition..."), tr("Cancel"), 0,0,this);
        connect(pma,&PartitionManagerAdapter::finished,
                &pd,&QProgressDialog::accept);
        emit mountRequested(password);
        pd.exec();
    }

}

void PartitionManagerWindow::unmountPartition()
{
    emit unmountRequested();
}

void PartitionManagerWindow::ejectDevice()
{
    emit ejectRequested();
}

void PartitionManagerWindow::updateUI(const State state)
{
    deviceSelector  ->setEnabled(state == State::NoDeviceSet);
    wipeButton      ->setEnabled(state == State::PartitionUnmounted);
    createButton    ->setEnabled(state == State::PartitionUnmounted);
    mountButton     ->setEnabled(state == State::PartitionUnmounted);
    unmountButton   ->setEnabled(state == State::PartitionMounted);
    ejectButton     ->setEnabled(state == State::PartitionUnmounted);
    switch(state)
    {
    case State::NoDeviceSet:
        this->deviceSelector->blockSignals(true);
        this->deviceSelector->setCurrentIndex(-1);
        this->deviceSelector->blockSignals(false);
        this->statusBar->showMessage(tr("No device selected."));
        break;
    case State::PartitionMounted:
        this->statusBar->showMessage(tr("Partition mounted."));
        break;
    case State::PartitionUnmounted:
        this->statusBar->showMessage(tr("No partition mounted."));
        break;
    case State::Mounting:
        this->statusBar->showMessage(tr("Mounting partition..."));
        break;
    case State::Unmounting:
        this->statusBar->showMessage(tr("Unmounting partition..."));
        break;
    case State::Wiping:
        this->statusBar->showMessage(tr("Erasing partition..."));
        break;
    case State::Busy:
        this->statusBar->showMessage(tr("Busy..."));
        break;
    }
}

PartitionManagerWindow::~PartitionManagerWindow()
{
    pmaThread.quit();
    pmaThread.wait();
}

