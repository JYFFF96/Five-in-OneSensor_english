#include "laserradar.h"

#include <QApplication>
#include <QFile>
#include <QStyleFactory>
#include <QTranslator>
#include <QLibraryInfo>
void loadStyleSheet(QApplication &app) {
    QFile file(":./qss/style.qss");  // 直接从磁盘加载
    if (file.open(QFile::ReadOnly)) {
        QString styleSheet = file.readAll();
        app.setStyleSheet(styleSheet);
        file.close();
    } else {
        qDebug() << "Failed to load QSS file!";
    }
}
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::WindowText, Qt::white); // 设置窗口文本颜色
    palette.setColor(QPalette::Text, Qt::white);       // 设置文本输入框的文本颜色
    palette.setColor(QPalette::ButtonText, Qt::white); // 设置按钮文本颜色
    qApp->setPalette(palette);
    // 加载 QSS
    loadStyleSheet(a);
    QTranslator qtTrans;
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    qtTrans.load("qtbase_zh_CN", QLibraryInfo::path(QLibraryInfo::TranslationsPath));
#else  // Qt5
    qtTrans.load("qtbase_zh_CN", QLibraryInfo::location(QLibraryInfo::TranslationsPath));
#endif
    a.installTranslator(&qtTrans);
    LaserRadar w;
    w.show();
    return a.exec();
}
