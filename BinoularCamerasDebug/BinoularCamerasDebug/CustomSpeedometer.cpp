#include "customspeedometer.h"
#include <QPainter>
#include <QtMath>
#include <QConicalGradient>

CustomSpeedometer::CustomSpeedometer(QWidget *parent) : QWidget(parent) {
	setMinimumSize(200, 200);
}

void CustomSpeedometer::setSpeed(int speed) {
	m_speed = qBound(0, speed, m_maxSpeed);
	update(); // 触发重绘
}

void CustomSpeedometer::paintEvent(QPaintEvent *) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	// 坐标变换：居中 & 缩放到 200x200
	int side = qMin(width(), height());
	painter.translate(width() / 2, height() / 2);
	painter.scale(side / 200.0, side / 200.0);

	// === 角度设定（-225° 到 +45°） ===
	const int startAngle = -225;
	const int endAngle = 45;
	const int totalAngle = endAngle - startAngle; // == 270°

	// === 1. 背景圆 ===
	QRadialGradient bgGradient(0, 0, 100);
	bgGradient.setColorAt(0.0, QColor(0, 80, 160));
	bgGradient.setColorAt(1.0, QColor(0, 40, 100));
	painter.setBrush(bgGradient);
	painter.setPen(Qt::NoPen);
	painter.drawEllipse(-100, -100, 200, 200);

	// === 外部装饰圆 ===
	painter.save();
	painter.setPen(QPen(QColor(255, 255, 255, 60), 2)); // 淡白色、2px
	painter.setBrush(Qt::NoBrush);

	// 半径略大于刻度线
	int outerRadius = 82;
	painter.drawEllipse(QPointF(0, 0), outerRadius, outerRadius);
	painter.restore();

	// === 2. 刻度线 ===
	for (int i = 0; i <= m_maxSpeed; i += 10) {
		painter.save();
		qreal angle = startAngle + (i * totalAngle) / m_maxSpeed;
		painter.rotate(angle);

		QColor tickColor = (i % 30 == 0) ? Qt::white : QColor(180, 200, 255);
		int tickLen = (i % 30 == 0) ? 10 : 5;
		painter.setPen(QPen(tickColor, 2));
		painter.drawLine(80, 0, 80 - tickLen, 0);
		painter.restore();
	}

	painter.save();
	painter.setPen(Qt::white);
	QFont font("Arial", 8);
	painter.setFont(font);
	QFontMetrics fm(font);

	int labelRadius = 82 - 22;

	for (int i = 0; i <= m_maxSpeed; i += 20) {
		qreal angle = startAngle + (i * totalAngle) / m_maxSpeed;
		qreal rad = qDegreesToRadians(angle);

		QString text = QString::number(i);
		QRect rect = fm.boundingRect(text);

		QPointF pos(qCos(rad) * labelRadius - rect.width() / 2.0,
			qSin(rad) * labelRadius + rect.height() / 2.0);
		painter.drawText(pos, text);
	}

	painter.restore();


	// === 3. 同心圆（可选视觉参考）===


	// === 4. 进度环（从 135° 开始 = -225°） ===
	painter.save();
	QRectF arcRect(-75, -75, 150, 150);
	painter.setPen(QPen(QColor(120, 220, 255), 12, Qt::SolidLine, Qt::FlatCap));

	int arcStartAngle = -2160; // == -720
	int arcSpanAngle = -(m_speed * 270 * 16) / m_maxSpeed; // 顺时针绘制

	painter.drawArc(arcRect, arcStartAngle, arcSpanAngle);
	painter.restore();

	// === 5. 指针（三角形 + 圆） ===
	painter.save();

	// 计算当前速度对应的角度
	qreal pointerAngle = startAngle + (m_speed * totalAngle) / m_maxSpeed;
	painter.rotate(pointerAngle);

	// === 参数设置 ===
	qreal baseWidth = 16.0;   // 底边宽度（像素）
	qreal tipLength = 65.0;   // 指针长度（尖端距离中心）
	qreal circleRadius = 6.0; // 中心圆半径

	// === 构造三角形 ===
	QPointF p1(0, -baseWidth / 2); // 左下角
	QPointF p2(0, +baseWidth / 2); // 右下角
	QPointF tip(tipLength, 0);     // 尖端（指向速度方向）

	QPainterPath pointerPath;
	pointerPath.moveTo(p1);
	pointerPath.lineTo(p2);
	pointerPath.lineTo(tip);
	pointerPath.closeSubpath();

	// 填充指针三角形
	painter.setBrush(QColor(120, 220, 255));
	painter.setPen(Qt::NoPen);
	painter.drawPath(pointerPath);

	// === 画中心圆点 ===
	painter.setBrush(QColor(120, 220, 255));
	painter.drawEllipse(QPointF(0, 0), circleRadius, circleRadius);

	painter.restore();

	// === 6. 当前速度文本 ===
	painter.save();
	painter.setPen(Qt::white);
	painter.setFont(QFont("Arial", 12, QFont::Bold));
	painter.drawText(QRect(-60, 40, 120, 40), Qt::AlignCenter, QString("%1 km/h").arg(m_speed));
	painter.restore();
}
