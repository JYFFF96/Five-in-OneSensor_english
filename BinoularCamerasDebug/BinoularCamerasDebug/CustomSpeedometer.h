#pragma once
#include <qwidget.h>
class CustomSpeedometer : public QWidget {
	Q_OBJECT

public:
	explicit CustomSpeedometer(QWidget *parent = nullptr);
	void setSpeed(int speed); // 设置速度（km/h）

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	void paintPtr(QPainter painter);

private:
	int m_speed = 40;      // 当前速度
	int m_maxSpeed = 180;  // 最大速度
};

