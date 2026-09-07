#include "LoginDialog.h"
#include <qmessagebox.h>
#include <QHostAddress>
#include "stereocamera.h"
#include<thread>
#include<cstdio>
#include<fstream>
using namespace std::chrono;
LoginDialog::LoginDialog(QWidget *parent)
	: QDialog(parent)
{
	ui.setupUi(this);
	setWindowTitle("Log In");
	QIcon icon(":/images/images/logo.png");
	this->setWindowIcon(icon);
	//connect(ui.btnConnect, &QPushButton::clicked, this, &LoginDialog::on_btnConnect_clicked);
}

LoginDialog::~LoginDialog()
{}

QString LoginDialog::getIpAddress() const
{
	return ipAddress;
}

void LoginDialog::on_btnConnect_clicked()
{
	QString ip = ui.lineEdit->text().trimmed();
	if (ip.isEmpty()) {
		QMessageBox::warning(this, "Error", "Enter IP Address");
		return;
	}

	if (!tryConnect(ip)) {
		QMessageBox::critical(this, "Connection Failed", "Unable to connect to device");
		return;
	}

	ipAddress = ip;
	accept();  // 关闭对话框，返回 QDialog::Accepted
}

void LoginDialog::on_btnCancle_clicked()
{
	close();
}

bool LoginDialog::tryConnect(const QString &ip)
{

	StereoCamera* camera = StereoCamera::connect(ip.toUtf8());
	auto deadline = steady_clock::now() + 1s;
	while (!camera->isConnected() && steady_clock::now() < deadline) {
		std::puts("connecting...");
		std::this_thread::sleep_for(1s);
	}
	if (camera->isConnected()) {
		camera->disconnectFromServer();
	}
	else {
		return false;
	}
	// 简化处理：仅测试 IP 格式和连通性，真实使用应调用 SDK 的 connect 测试
	//QHostAddress addr;
	//if (!addr.setAddress(ip))
		//return false;

	// 你可以在这里调用 StereoCamera::connect(ip.toStdString()) 做测试连接
	// 并立即 disconnect，确认设备在线。
	return true;
}