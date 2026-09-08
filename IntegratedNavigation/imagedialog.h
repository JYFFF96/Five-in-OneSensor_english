#pragma once
#include <QDialog>
#include <QPixmap>

class QLabel;

class ImageDialog : public QDialog {
    Q_OBJECT
public:
    explicit ImageDialog(const QPixmap& pm, QWidget* parent = nullptr);

    // 便捷函数：直接给文件路径
    static int showModal(QWidget* parent, const QString& filePath,
                         const QString& title = QString());

protected:
    void mousePressEvent(QMouseEvent*) override;   // 点击关闭
    void keyPressEvent(QKeyEvent* e) override;     // Esc/Enter关闭

private:
    static QSize fitWithinScreen(const QSize& want, QWidget* ref);
    void setImage(const QPixmap& pm);

    QLabel*  m_label = nullptr;
    QPixmap  m_src;   // 原图（用于缩放）
};
