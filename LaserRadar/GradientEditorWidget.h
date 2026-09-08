#ifndef GRADIENTEDITORWIDGET_H
#define GRADIENTEDITORWIDGET_H

#include <QWidget>
#include <QVector>
#include <QPair>
#include <QColor>

class GradientEditorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GradientEditorWidget(QWidget *parent = nullptr);

    QVector<QPair<double, QColor>> getStops() const;
    void setStops(const QVector<QPair<double, QColor>> &stops);

signals:
    void stopsChanged(const QVector<QPair<double, QColor>> &stops);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QVector<QPair<double, QColor>> stops;
    int draggingIndex = -1;

    QRect gradientRect() const;
    QRect stopRect(int index) const;
    int hitTestStop(const QPoint &pos) const;
};

#endif // GRADIENTEDITORWIDGET_H
