#pragma once
#include <QWidget>
#include <QColor>
#include <QVector>

struct LegendEntry {
	QString text;
	QColor  color;
	bool    hollow = false; // 空心框
};

class ColorSwatch : public QWidget {
	Q_OBJECT
public:
	explicit ColorSwatch(QWidget* parent = nullptr);
	void setColor(const QColor& c) { m_color = c; update(); }
	void setHollow(bool h) { m_hollow = h; update(); }
	QSize sizeHint() const override { return { 18,18 }; }
	QSize minimumSizeHint() const override { return { 14,14 }; }
protected:
	void paintEvent(QPaintEvent*) override;
private:
	QColor m_color = Qt::magenta;
	bool   m_hollow = false;
};

class LegendBar : public QWidget {
	Q_OBJECT
public:
	explicit LegendBar(QWidget* parent = nullptr);
	void setItems(const QVector<LegendEntry>& items);
	void setSpacing(int px);                 // 项目间距
	void setTextColor(const QColor& c);      // 透明背景下可调文字色
	void setOrientation(Qt::Orientation o);  // Qt::Vertical(默认)/Horizontal
private:
	QVector<LegendEntry> m_items;
	int m_spacing = 8;
	Qt::Orientation m_orient = Qt::Vertical; // 默认竖排
	QColor m_textColor = QColor("#E0E0E0");  // 默认浅色字
};
