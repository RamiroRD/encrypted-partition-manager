#include "gui/CreateDialog.h"
#include <QPushButton>

CreateDialog::CreateDialog(QWidget * parent)
    : QDialog(parent),
      formLayout(new QFormLayout()),
      passwordField(new QLineEdit()),
      slotSelector(new QSpinBox()),
      buttons(new QDialogButtonBox())
{
    QVBoxLayout * mainLayout = new QVBoxLayout();
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(buttons);

    this->setWindowModality(Qt::WindowModal);
    this->setLayout(mainLayout);

    acceptButton = buttons->addButton(QDialogButtonBox::Ok);
    cancelButton = buttons->addButton(QDialogButtonBox::Cancel);

    slotSelector->setMinimum(0);
    slotSelector->setMaximum(8192-1);

    passwordField->setEchoMode(QLineEdit::Password);

    formLayout->addRow(tr("Offset:"), slotSelector);
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
