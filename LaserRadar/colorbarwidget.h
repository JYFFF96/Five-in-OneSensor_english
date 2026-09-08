#ifndef COLORBARWIDGET_H
#define COLORBARWIDGET_H

#include <QWidget>
#include <QVector>
#include <QPair>
#include <QColor>
#include <QPushButton>
class ColorBarWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ColorBarWidget(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setNumLabels(int num);
    void setLabelFormat(const QString &format);
    void setAspectRatio(int ratio);
    void setColorStops(const QVector<QPair<double, QColor>> &stops);
    QColor getColorForValue(double value) const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent  *event) override;
    void leaveEvent(QEvent *event) override;

signals:
    void hideWin();

private slots:
    void slotHideWin();

private:
    QString title;
    int numLabels;
    QString labelFormat;
    int aspectRatio;
    QVector<QPair<double, QColor>> colorStops;
private:
    bool dragging = false;
    QPoint dragStartPos;
    QPushButton *closeButton; // 关闭按钮
};

#endif // COLORBARWIDGET_H
