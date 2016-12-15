#include <QPushButton>
#include <QLabel>

#include "include/logic/PartitionManager.h"
#include "gui/CreateDialog.h"


CreateDialog::CreateDialog(QWidget * parent)
    : QDialog(parent),
      formLayout(new QFormLayout()),
      passwordField(new QLineEdit()),
      slotSelector(new QSpinBox()),
      buttons(new QDialogButtonBox())
{
    QString msg = tr("Choose a password and position number. A position number is in the range 0-");
    msg += QString::number(PartitionManager::SLOTS_AMOUNT-1);
    msg += tr(". The size of the gap between two consecutives positions is proportional to the device size.");

    QLabel * text = new QLabel(msg);
    text->setWordWrap(true);

    QVBoxLayout * mainLayout = new QVBoxLayout();
    mainLayout->addWidget(text);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(buttons);

    this->setWindowModality(Qt::WindowModal);
    this->setLayout(mainLayout);

    acceptButton = buttons->addButton(QDialogButtonBox::Ok);
    cancelButton = buttons->addButton(QDialogButtonBox::Cancel);

    slotSelector->setMinimum(0);
    slotSelector->setMaximum(PartitionManager::SLOTS_AMOUNT-1);

    passwordField->setEchoMode(QLineEdit::Password);

    formLayout->addRow(tr("Position:"), slotSelector);
    formLayout->addRow(tr("Password:"), passwordField);

    acceptButton->setDisabled(true);

    connect(cancelButton, &QPushButton::clicked,
            this, &QDialog::reject);
    connect(acceptButton,&QPushButton::clicked,
            this, &QDialog::accept);
    connect(passwordField,&QLineEdit::textChanged,
            this, &CreateDialog::verify);
}

void CreateDialog::verify(const QString &password)
{
    acceptButton->setDisabled(password.isEmpty());
}

unsigned short CreateDialog::getSlot()
{
    return slotSelector->value();
}

const QString CreateDialog::getPassword()
{
    return passwordField->text();
}
