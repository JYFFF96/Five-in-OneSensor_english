#pragma once

#include <QDialog>
#include "ui_LoginDialog.h"

class LoginDialog : public QDialog
{
	Q_OBJECT

public:
	LoginDialog(QWidget *parent = nullptr);
	~LoginDialog();
	QString getIpAddress() const;

private slots:
	void on_btnConnect_clicked();
	void on_btnCancle_clicked();
private:
	Ui::LoginDialogClass ui;
	bool tryConnect(const QString &ip);
	QString ipAddress;
};
