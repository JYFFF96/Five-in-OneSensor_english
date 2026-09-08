#pragma once
#include <qcheckbox.h>
class ActionOnlyCheckBox : public QCheckBox {
	Q_OBJECT
public:
	using QCheckBox::QCheckBox;  // ºÃ≥–ππ‘Ï
signals:
	void nextStep(bool flag);
protected:
	void nextCheckState() override;
};

