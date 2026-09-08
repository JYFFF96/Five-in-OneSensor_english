#ifndef BROWSERDIALOG_H
#define BROWSERDIALOG_H

#include <QDialog>
#include <QUrl>

class QWebEngineView;

class BrowserDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BrowserDialog(const QUrl &url, QWidget *parent = nullptr);

private:
    QWebEngineView *webView;
};

#endif // BROWSERDIALOG_H
