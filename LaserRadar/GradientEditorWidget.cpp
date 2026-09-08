#include "GradientEditorWidget.h"
#include <QPainter>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QColorDialog>
#include <cmath>

GradientEditorWidget::GradientEditorWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(80);
    setMinimumWidth(200);

    // 初始化 stops
    stops = {
        {0.0, Qt::blue},
        {0.25, Qt::cyan},
        {0.5, Qt::green},
        {0.75, Qt::yellow},
        {1.0, Qt::red}
    };
}

QVector<QPair<double, QColor>> GradientEditorWidget::getStops() const
{
    return stops;
}

void GradientEditorWidget::setStops(const QVector<QPair<double, QColor>> &newStops)
{
    stops = newStops;
    update();
    emit stopsChanged(stops);
}

QRect GradientEditorWidget::gradientRect() const
{
    return QRect(10, 20, width() - 20, height() - 50);
}

QRect GradientEditorWidget::stopRect(int index) const
{
    QRect gRect = gradientRect();
    int x = gRect.left() + stops[index].first * gRect.width();
    int y = gRect.bottom() + 10;
    return QRect(x - 5, y - 5, 10, 10);
}

int GradientEditorWidget::hitTestStop(const QPoint &pos) const
{
    for (int i = 0; i < stops.size(); ++i)
    {
        if (stopRect(i).contains(pos))
            return i;
    }
    return -1;
}

void GradientEditorWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw gradient
    QRect gRect = gradientRect();
    QLinearGradient gradient(gRect.left(), gRect.top(), gRect.right(), gRect.top());
    for (const auto &stop : stops)
    {
        gradient.setColorAt(stop.first, stop.second);
    }

    painter.setBrush(gradient);
    painter.setPen(Qt::black);
    painter.drawRect(gRect);

    // Draw stops (as small circles)
    for (int i = 0; i < stops.size(); ++i)
    {
        QRect r = stopRect(i);
        painter.setBrush(stops[i].second);
        painter.setPen(Qt::black);
        painter.drawEllipse(r);
    }
}

void GradientEditorWidget::mousePressEvent(QMouseEvent *event)
{
    int hit = hitTestStop(event->pos());
    if (hit >= 0)
    {
        if (event->button() == Qt::LeftButton)
        {
            draggingIndex = hit;
        }
        else if (event->button() == Qt::RightButton)
        {
            QColor newColor = QColorDialog::getColor(stops[hit].second, this, "Select Color");
            if (newColor.isValid())
            {
                stops[hit].second = newColor;
                update();
                emit stopsChanged(stops);
            }
        }
    }
}

void GradientEditorWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (draggingIndex >= 0)
    {
        QRect gRect = gradientRect();
        double pos = (event->pos().x() - gRect.left()) / (double)gRect.width();
        pos = std::clamp(pos, 0.0, 1.0);

        stops[draggingIndex].first = pos;
        std::sort(stops.begin(), stops.end(), [](const auto &a, const auto &b) {
            return a.first < b.first;
        });

        update();
        emit stopsChanged(stops);
    }
}

void GradientEditorWidget::mouseReleaseEvent(QMouseEvent *)
{
    if (draggingIndex >= 0)
    {
        draggingIndex = -1;
    }
}
