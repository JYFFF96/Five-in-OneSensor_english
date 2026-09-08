#ifndef HEXINPUTFILTER_H
#define HEXINPUTFILTER_H
#include <QLineEdit>
#include <QRegularExpression>
#include <QKeyEvent>
#include <QTimer>


class HexInputFilter : public QObject {
    Q_OBJECT
public:
    explicit HexInputFilter(int maxBytes, QLineEdit *lineEdit, QObject *parent = nullptr)
        : QObject(parent), maxBytes(maxBytes), lineEdit(lineEdit) {}

    void setMaxBytes(int newMaxBytes) {
        maxBytes = newMaxBytes;
        enforceMaxLength(); // 当修改长度时，自动截取多余内容
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            QString newChar = keyEvent->text().toUpper();
            QString text = lineEdit->text();

            // 允许的按键：0-9, A-F, 退格, 删除, 左右箭头
            if (!newChar.isEmpty() && !newChar[0].isSpace()) {
                if (!newChar.contains(QRegularExpression("[0-9A-F]")) &&
                    keyEvent->key() != Qt::Key_Backspace && keyEvent->key() != Qt::Key_Delete &&
                    keyEvent->key() != Qt::Key_Left && keyEvent->key() != Qt::Key_Right) {
                    return true; // 阻止无效输入
                }
            }

            // 延迟处理，确保 UI 先接受输入
            QTimer::singleShot(0, this, &HexInputFilter::enforceMaxLength);
            return false;
        }
        return QObject::eventFilter(obj, event);
    }

private:
    int maxBytes;
    QLineEdit *lineEdit;

    void enforceMaxLength() {
        if (!lineEdit) return;

        QString cleanText = lineEdit->text();
        cleanText.remove(' '); // 移除所有空格

        // 计算当前最大字符数（不含空格）
        int maxChars = maxBytes * 2;
        if (cleanText.length() > maxChars) {
            cleanText = cleanText.left(maxChars); // 截取超出部分
        }

        // 重新格式化文本（每 2 个字符插入 1 个空格）
        QString formattedText;
        for (int i = 0; i < cleanText.length(); ++i) {
            formattedText.append(cleanText[i]);
            if ((i + 1) % 2 == 0 && (i + 1) != cleanText.length()) {
                formattedText.append(' '); // 每 2 个字符插入空格
            }
        }

        // 更新 QLineEdit
        lineEdit->blockSignals(true);
        lineEdit->setText(formattedText);
        lineEdit->setCursorPosition(formattedText.length());
        lineEdit->blockSignals(false);
    }
};
#endif // HEXINPUTFILTER_H
