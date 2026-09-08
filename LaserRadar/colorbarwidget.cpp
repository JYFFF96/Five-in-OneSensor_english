#include "ColorBarWidget.h"
#include <QPainter>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QColorDialog>
#include <cmath>

ColorBarWidget::ColorBarWidget(QWidget *parent)
    : QWidget(parent),
    title("intensity"),
    numLabels(5),
    labelFormat("%.2e"),
    aspectRatio(20)
{
    setMinimumWidth(100);

    colorStops = {
        {0.0, Qt::darkBlue},
        {0.2, Qt::blue},
        {0.4, Qt::cyan},
        {0.6, Qt::green},
        {0.8, Qt::yellow},
        {1.0, Qt::red}
    };

    // 初始化关闭按钮
    closeButton = new QPushButton("×", this);
    closeButton->setFixedSize(20, 20);
    closeButton->setStyleSheet("QPushButton { background-color: red; color: white; border: none; border-radius: 10px; }"
                               "QPushButton:hover { background-color: darkred; }");

    connect(closeButton, &QPushButton::clicked, this, &ColorBarWidget::slotHideWin);

}

void ColorBarWidget::setTitle(const QString &t)
{
    title = t;
    update();
}

void ColorBarWidget::setNumLabels(int num)
{
    numLabels = num;
    update();
}

void ColorBarWidget::setLabelFormat(const QString &format)
{
    labelFormat = format;
    update();
}

void ColorBarWidget::setAspectRatio(int ratio)
{
    aspectRatio = ratio;
    update();
}

void ColorBarWidget::setColorStops(const QVector<QPair<double, QColor>> &stops)
{
    colorStops = stops;
    update();
}

QColor ColorBarWidget::getColorForValue(double value) const
{
    double norm = value / 256.0;
    norm = std::clamp(norm, 0.0, 1.0);

    for (int i = 1; i < colorStops.size(); ++i)
    {
        if (norm <= colorStops[i].first)
        {
            double t1 = colorStops[i-1].first;
            double t2 = colorStops[i].first;
            QColor c1 = colorStops[i-1].second;
            QColor c2 = colorStops[i].second;

            double factor = (norm - t1) / (t2 - t1);

            int r = c1.red()   + factor * (c2.red()   - c1.red());
            int g = c1.green() + factor * (c2.green() - c1.green());
            int b = c1.blue()  + factor * (c2.blue()  - c1.blue());

            return QColor(r, g, b);
        }
    }

    return colorStops.last().second;
}

void ColorBarWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int barHeight = height() - 40;
    int barWidth = width() / aspectRatio;
    if (barWidth < 20) barWidth = 20;

    QRect gradientRect(40, 40, barWidth, barHeight);

    QLinearGradient gradient(gradientRect.bottomLeft(), gradientRect.topLeft());
    for (const auto &stop : colorStops)
        gradient.setColorAt(stop.first, stop.second);

    painter.setBrush(gradient);
    painter.setPen(Qt::NoPen);
    painter.drawRect(gradientRect);

    // Labels
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPointSize(10);
    painter.setFont(font);

    for (int i = 0; i <= numLabels; ++i)
    {
        double ratio = (double)i / numLabels; // 正向 0~1

        int y = gradientRect.bottom() - ratio * gradientRect.height();

        double value = ratio * 256.0;

        QString label = QString::asprintf(labelFormat.toStdString().c_str(), value);
        painter.drawLine(gradientRect.right() + 5, y, gradientRect.right() + 15, y);
        painter.drawText(gradientRect.right() + 20, y + 5, label);
    }

    // Title
    painter.setFont(QFont("Arial", 14, QFont::Bold));
    painter.drawText(10, 15, title);

    closeButton->move(width() - 20, 0); // 右上角
}

void ColorBarWidget::enterEvent(QEnterEvent  *)
{
    setCursor(Qt::SizeAllCursor); // 十字箭头
}

void ColorBarWidget::leaveEvent(QEvent *)
{
    setCursor(Qt::ArrowCursor); // 恢复箭头
}

void ColorBarWidget::slotHideWin()
{
    emit hideWin();
    hide();
}

void ColorBarWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        dragging = true;
        dragStartPos = event->globalPos() - frameGeometry().topLeft();
    }
}

void ColorBarWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (dragging)
    {
        move(event->globalPos() - dragStartPos);
    }
}

void ColorBarWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        dragging = false;
    }
}
