#include <QMessageBox>
#include <QInputDialog>
#include <QProgressDialog>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStringList>

#include "gui/PartitionManagerWindow.h"
#include "gui/CreateDialog.h"
#include "logic/PartitionManager.h"


PartitionManagerWindow::PartitionManagerWindow(QWidget *parent)
    : QMainWindow(parent),
      pma               (new PartitionManagerAdapter()), // Tiene que no tener padre!
      centralWidget     (new QWidget()),
      deviceLabel       (new QLabel(tr("Device:"))),
      refreshButton     (new QToolButton()),
      deviceSelector    (new QListWidget()),
      wipeButton        (new QPushButton(tr("Erase device"))),
      createButton      (new QPushButton(tr("Create partition"))),
      mountButton       (new QPushButton(tr("Mount partition"))),
      unmountButton     (new QPushButton(tr("Unmount partition"))),
      ejectButton       (new QPushButton(tr("Eject device"))),
      statusBar         (new QStatusBar())
{
    this->setupUI();
    this->setupPMA();

    auto currentDevice = PartitionManager::currentDevice();
    if(!currentDevice.empty())
        setDevicePath(QString::fromStdString(currentDevice));
    else
        populateDeviceSelector();
}

void PartitionManagerWindow::setupUI()
{
    this->setWindowTitle(tr("Partition Manager"));
    this->layout()->setSizeConstraint(QLayout::SetFixedSize);
    QVBoxLayout * mainLayout = new QVBoxLayout();
    QHBoxLayout * buttonsRow = new QHBoxLayout();
    QHBoxLayout * selectorRow = new QHBoxLayout();

    selectorRow->addWidget(deviceSelector);
    selectorRow->addWidget(refreshButton);

    buttonsRow->addWidget(wipeButton);
    buttonsRow->addWidget(createButton);
    buttonsRow->addWidget(mountButton);
    buttonsRow->addWidget(unmountButton);
    buttonsRow->addWidget(ejectButton);

    mainLayout->addWidget(deviceLabel);
    mainLayout->addLayout(selectorRow);
    mainLayout->addLayout(buttonsRow);

    refreshButton->setIcon(QIcon::fromTheme(QString("view-refresh")));

    deviceLabel->setSizePolicy(QSizePolicy());
    /*
     * Al hacer esto, el parentesco queda:
     * this <- centralWidget -< x
     * donde x son los widgets que se terminan mostrando.
     * NO es necesario definir el parentesco al crear cada widget.
     * setLayout hace que centralWidget los "adopte".
     * setCentralWidget hace lo mismo con centralWidget.
     */
    centralWidget->setLayout(mainLayout);
    this->setCentralWidget(centralWidget);
    this->setStatusBar(statusBar);
    statusBar->showMessage(tr("Select a device."));

    wipeButton->setDisabled(true);
    createButton->setDisabled(true);
    mountButton->setDisabled(true);
    unmountButton->setDisabled(true);
    ejectButton->setDisabled(true);
    wipeButton->setAutoDefault(true);
    createButton->setAutoDefault(true);
    mountButton->setAutoDefault(true);
    unmountButton->setAutoDefault(true);
    ejectButton->setAutoDefault(true);


    /*
     * Conectamos los botones del esta GUI a los métodos que toman las primeras
     * acciones, como pedir entrada y verificarlas.
     */
    //connect(deviceSelector,&QComboBox::currentTextChanged,
     //       this,&PartitionManagerWindow::setDevice);
    connect(deviceSelector, &QListWidget::itemActivated,
            this, &PartitionManagerWindow::setDevice);
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
    connect(refreshButton,&QToolButton::clicked,
            this,&PartitionManagerWindow::populateDeviceSelector);

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
     * Conectamos las señales que se emiten al momento de necesitar métodos de
     * PMA. Los métodos públicos en PMA son todos slots y por eso necesitamos
     * señales locales.
     */
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


    // Ante errores mostramos un QMessageBox
    connect(pma,&PartitionManagerAdapter::errorOccurred,
            this,&PartitionManagerWindow::showErrorMessage);

    // Arrancamos el PMA
    pmaThread.start();

}

void PartitionManagerWindow::setDevicePath(const QString &dev)
{
    emit devChangeRequested(dev);
}

void PartitionManagerWindow::setDevice(const QListWidgetItem * item)
{
    emit devChangeRequested(item->text());
}

void PartitionManagerWindow::wipeDevice()
{
    auto reply = QMessageBox::question(this,
                                       tr("Confirm"),
                                       tr("Do you want to erase all contents from this device? This can be a lengthy operation."),
                                       QMessageBox::Yes | QMessageBox::No,
                                       QMessageBox::No);
    if(reply == QMessageBox::Yes) {
        QProgressDialog pd (tr("Erasing volume."), tr("Cancel"), 0, 100,this);
        connect(pma,&PartitionManagerAdapter::finished,
                &pd,&QProgressDialog::accept);
        // DirectConnection para poder "interrumpir" el otro thread.
        // Esto hace que se imprima un error molesto pero no parece pasar nada.
        connect(&pd,&QProgressDialog::canceled,
                pma,&PartitionManagerAdapter::abortOperation,Qt::DirectConnection);
        connect(pma,&PartitionManagerAdapter::progressChanged,
                &pd,&QProgressDialog::setValue);
        emit wipeRequested();
        pd.exec();
    }
}

void PartitionManagerWindow::createPartition()
{
    CreateDialog dialog(this);
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
        // DirectConnection: ver wipeDevice() de esta clase
        connect(&pd,&QProgressDialog::canceled,
                pma,&PartitionManagerAdapter::abortOperation,Qt::DirectConnection);
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
    ejectButton     ->setEnabled(state == State::PartitionUnmounted ||
                                 state == State::PartitionMounted);
    refreshButton   ->setEnabled(state == State::NoDeviceSet);
    switch(state)
    {
    case State::NoDeviceSet:
        populateDeviceSelector();
        this->statusBar->showMessage(tr("No device selected."));
        break;
    case State::PartitionMounted:
        populateDeviceSelector();
        this->statusBar->showMessage(tr("Partition mounted on ") +
                                     QString::fromStdString(PartitionManager::MOUNTPOINT));
        break;
    case State::PartitionUnmounted:
        populateDeviceSelector();
        this->statusBar->showMessage(tr("No partition mounted."));
        break;
    case State::Busy:
        this->statusBar->showMessage(tr("Busy..."));
        break;
    }
}

void PartitionManagerWindow::showErrorMessage(QString msg)
{
    QMessageBox::warning(this,
                         QObject::tr("An error occurred."),
                         msg);
}

void PartitionManagerWindow::populateDeviceSelector()
{

    QListWidgetItem * previous = deviceSelector->currentItem();
    QString previousDev;
    if(previous != nullptr)
        QString previousDev = previous->text();

    deviceSelector->clear();
    for(auto &devicePath : PartitionManager::findAllDevices())
        deviceSelector->addItem(QString::fromStdString(devicePath));

    auto currentDevice = PartitionManager::currentDevice();
    if(!currentDevice.empty())
        previousDev = QString::fromStdString(currentDevice);

    auto list = deviceSelector->findItems(previousDev, Qt::MatchExactly);
    if(!list.empty())
        deviceSelector->setCurrentItem(list.first());

}

PartitionManagerWindow::~PartitionManagerWindow()
{
    pmaThread.quit();
    pmaThread.wait();
}

