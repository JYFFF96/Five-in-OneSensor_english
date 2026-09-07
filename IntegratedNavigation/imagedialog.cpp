#include "imagedialog.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QFileInfo>
#include <QMessageBox>
#include <QKeyEvent>

static constexpr int kMargin = 16;   // 让四周留一点空隙

QSize ImageDialog::fitWithinScreen(const QSize& want, QWidget* ref)
{
    // 取当前屏幕可用区域（排除任务栏/菜单栏）
    QScreen* scr = ref ? ref->screen() : QGuiApplication::primaryScreen();
    QRect avail = scr ? scr->availableGeometry() : QRect(0,0,1280,800);
    QSize max = avail.size() - QSize(kMargin*2, kMargin*2);
    if (want.width() <= 0 || want.height() <= 0) return max;
    return want.boundedTo(max);
}

ImageDialog::ImageDialog(const QPixmap& pm, QWidget* parent)
    : QDialog(parent), m_src(pm)
{
    setModal(true);

    // 隐藏标题栏关闭按钮（仍保留标题与拖动）
    // setWindowFlag(Qt::WindowCloseButtonHint, false);

    // 背景与边距
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(kMargin, kMargin, kMargin, kMargin);

    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    lay->addWidget(m_label);

    setImage(pm);
}

void ImageDialog::setImage(const QPixmap& pm)
{
    m_src = pm;

    // 计算目标尺寸：在屏幕内完整显示
    QSize target = fitWithinScreen(pm.size(), this);

    QPixmap show = (target == pm.size())
                       ? pm
                       : pm.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    m_label->setPixmap(show);
    m_label->adjustSize();

    // 对话框尺寸 = 图片尺寸 + 边距
    QSize finalSize = m_label->sizeHint() + QSize(kMargin*2, kMargin*2);
    // 设定为固定大小，避免出现滚动条或拉伸导致不完整
    setFixedSize(finalSize);
}

int ImageDialog::showModal(QWidget* parent, const QString& filePath, const QString& title)
{
    QPixmap pm(filePath);
    if (pm.isNull()) {
        QMessageBox::warning(parent, QObject::tr("Open Failed"),
                             QObject::tr("Unable to load image：%1").arg(filePath));
        return QDialog::Rejected;
    }
    ImageDialog dlg(pm, parent);
    dlg.setWindowTitle(!title.isEmpty()
                           ? title
                           : QObject::tr("%1").arg(QFileInfo(filePath).fileName()));
    return dlg.exec();
}

void ImageDialog::mousePressEvent(QMouseEvent*)
{
    accept();   // 单击任意位置关闭
}

void ImageDialog::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
        accept();
    else
        QDialog::keyPressEvent(e);
}
