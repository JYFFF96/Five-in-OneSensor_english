#include "custommessagebox.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>

CustomMessageBox::CustomMessageBox(const QString &title, const QString &message, QWidget *parent)
    : QDialog(parent) {
    // 设置无边框窗口（但不透明）
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setFixedSize(360, 200);  // 你可以按需修改尺寸
    setStyleSheet(R"(
        QDialog {
            background-color: #2b2b2b;
            border-radius: 8px;
        }
        QLabel {
            color: white;
        }
        QPushButton {
            background-color: #555;
            color: white;
            padding: 6px 16px;
            border-radius: 4px;
        }
        QPushButton:hover {
            background-color: #777;
        }
    )");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // 标题栏
    titleLabel = new QLabel(title);
    QFont titleFont;
    titleFont.setBold(true);
    titleFont.setPointSize(13);
    titleLabel->setFont(titleFont);

    // 消息文本
    messageLabel = new QLabel(message);
    messageLabel->setWordWrap(true);

    // 按钮
    okButton = new QPushButton("OK");
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);

    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(messageLabel);
    mainLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
}

void CustomMessageBox::showMessage(const QString &title, const QString &message, QWidget *parent) {
    CustomMessageBox box(title, message, parent);
    box.exec();  // 阻塞显示
}
