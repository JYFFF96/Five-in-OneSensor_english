#include "browserdialog.h"

#include <QVBoxLayout>
#include <QWebEngineView>

BrowserDialog::BrowserDialog(const QUrl &url, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("LiDAR Settings");

    QVBoxLayout *layout = new QVBoxLayout(this);
    webView = new QWebEngineView(this);
    webView->load(url);
    webView->setMinimumSize(800, 600);

    layout->addWidget(webView);
    setLayout(layout);
}
