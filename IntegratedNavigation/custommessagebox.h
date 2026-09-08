#ifndef CUSTOMMESSAGEBOX_H
#define CUSTOMMESSAGEBOX_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>

class CustomMessageBox : public QDialog {
    Q_OBJECT

public:
    explicit CustomMessageBox(const QString &title,
                              const QString &message,
                              QWidget *parent = nullptr);

    static void showMessage(const QString &title,
                            const QString &message,
                            QWidget *parent = nullptr);

private:
    QLabel *titleLabel;
    QLabel *messageLabel;
    QPushButton *okButton;
};

#endif // CUSTOMMESSAGEBOX_H
