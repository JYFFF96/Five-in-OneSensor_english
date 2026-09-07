#include "LegendBar.h"
#include <QPainter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>

ColorSwatch::ColorSwatch(QWidget* parent) : QWidget(parent) {
	setAttribute(Qt::WA_TranslucentBackground); // 透明
}

void ColorSwatch::paintEvent(QPaintEvent*) {
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing, true);
	QRectF r = rect().adjusted(2, 2, -2, -2);
	if (m_hollow) {
		QPen pen(m_color, 2.0); pen.setCosmetic(true);
		p.setPen(pen); p.setBrush(Qt::NoBrush);
		p.drawRect(r);
	}
	else {
		p.setPen(Qt::NoPen);
		p.setBrush(m_color);
		p.drawRect(r);
	}
}

LegendBar::LegendBar(QWidget* parent) : QWidget(parent) {
	setContentsMargins(0, 0, 0, 0);
	setAttribute(Qt::WA_TranslucentBackground, true); // 透明
	setAutoFillBackground(false);                     // 不填充背景
}

void LegendBar::setSpacing(int px) { m_spacing = px; update(); }
void LegendBar::setOrientation(Qt::Orientation o) { m_orient = o; setItems(m_items); }
void LegendBar::setTextColor(const QColor& c) { m_textColor = c; setItems(m_items); }

void LegendBar::setItems(const QVector<LegendEntry>& items) {
	m_items = items;

	// 清空旧布局
	if (auto old = layout()) {
		QLayoutItem* it;
		while ((it = old->takeAt(0))) { delete it->widget(); delete it; }
		delete old;
	}

	// 外层布局：竖排用 VBox，横排用 HBox
	QLayout* lay = (m_orient == Qt::Vertical) ? static_cast<QLayout*>(new QVBoxLayout(this))
		: static_cast<QLayout*>(new QHBoxLayout(this));
	lay->setContentsMargins(10, 8, 10, 8);
	lay->setSpacing(m_spacing);

	auto addRow = [&](const LegendEntry& e) {
		QWidget* row = new QWidget(this);
		row->setAttribute(Qt::WA_TranslucentBackground, true);
		QHBoxLayout* r = new QHBoxLayout(row);
		r->setContentsMargins(0, 0, 0, 0);
		r->setSpacing(8);

		ColorSwatch* sw = new ColorSwatch(row);
		sw->setFixedSize(18, 18);
		sw->setColor(e.color);
		sw->setHollow(e.hollow);

		QLabel* lbl = new QLabel(e.text, row);
		// 透明背景下显式设文字色
		QPalette pal = lbl->palette();
		pal.setColor(QPalette::WindowText, m_textColor);
		lbl->setPalette(pal);

		r->addWidget(sw, 0, Qt::AlignVCenter);
		r->addWidget(lbl, 0, Qt::AlignVCenter);
		r->addStretch();

		lay->addWidget(row);
	};

	for (const auto& it : m_items) addRow(it);

	setLayout(lay);
}
