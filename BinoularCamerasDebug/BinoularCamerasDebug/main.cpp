#include "BinoularCamerasDebug.h"
#include <QtWidgets/QApplication>
#include "LoginDialog.h"

#include <QFile>
#include <QApplication>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QDir>
static void applyQss(const QString& path) {
	QFile f(path);
	if (f.open(QFile::ReadOnly | QFile::Text))
		qApp->setStyleSheet(QString::fromUtf8(f.readAll()));

}
static void loadQssSmart()
{

#ifdef QT_DEBUG 
	// 1) 开发期：源码同目录热加载（style.qss 和 main.cpp 同层） 
	const QString srcDir = QFileInfo(QString::fromUtf8(__FILE__)).absolutePath();
	const QString devQss = QDir(srcDir).filePath("style.qss");
	if (QFile::exists(devQss)) {
		applyQss(devQss);
		// 监听文件变化，保存即生效 
		static QFileSystemWatcher watcher; if (!watcher.files().contains(devQss)) {
			watcher.addPath(devQss);
			QObject::connect(&watcher, &QFileSystemWatcher::fileChanged, qApp, [devQss] { applyQss(devQss); });
		} return;
	}
#endif
	// 3) 资源兜底：发布期保证有默认皮肤
	applyQss(":/images/style.qss");
}

int main(int argc, char *argv[])
{
	Q_INIT_RESOURCE(Resource);
    QApplication a(argc, argv);
	loadQssSmart();

	QPalette palette;
	palette.setColor(QPalette::WindowText, Qt::white); // 设置窗口文本颜色
	palette.setColor(QPalette::Text, Qt::white);       // 设置文本输入框的文本颜色
	palette.setColor(QPalette::ButtonText, Qt::white); // 设置按钮文本颜色
	qApp->setPalette(palette);
	LoginDialog login;
	if (login.exec() != QDialog::Accepted)
		return 0;  // 用户取消或连接失败

	QString ip = login.getIpAddress();



	//QString ip = "192.168.1.251";
	BinoularCamerasDebug w;
	w.showMaximized();
	w.setIpAddress(ip);  // 调用我们定义的 setIpAddress
	w.startCamera();
	w.show();
    return a.exec();
}
