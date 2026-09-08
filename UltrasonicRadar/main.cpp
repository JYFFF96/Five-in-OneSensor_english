#include "ultrasonicradar.h"

#include <QApplication>
#include <QFile>
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
    // ✅ 1. 统一样式（推荐 "Fusion" 主题，保证一致性）
    // QApplication::setStyle(QStyleFactory::create("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::WindowText, Qt::white); // 设置窗口文本颜色
    palette.setColor(QPalette::Text, Qt::white);       // 设置文本输入框的文本颜色
    palette.setColor(QPalette::ButtonText, Qt::white); // 设置按钮文本颜色
    qApp->setPalette(palette);
    // 加载 QSS
    loadStyleSheet(a);
    UltrasonicRadar w;
    w.showMaximized();
    w.show();
    return a.exec();
}
