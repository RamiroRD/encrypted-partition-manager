#ifndef MOUNTDIALOG_H
#define MOUNTDIALOG_H

#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QDialogButtonBox>

class CreateDialog : public QDialog
{
    Q_OBJECT
    QString password;
    QFormLayout      * formLayout;
    QLineEdit        * passwordField;
    QSpinBox         * slotSelector;
    QDialogButtonBox * buttons;
    QPushButton      * acceptButton;
    QPushButton      * cancelButton;


public:
    CreateDialog(QWidget * parent);
    unsigned short getSlot();
    const QString &getPassword();

public slots:
    void verify(const QString &password);
};

#endif // MOUNTDIALOG_H
