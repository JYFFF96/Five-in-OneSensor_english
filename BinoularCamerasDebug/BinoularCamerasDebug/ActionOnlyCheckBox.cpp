// 示例：在你的窗口/控制类里
#include "ActionOnlyCheckBox.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QSignalBlocker>

static QString lastDir;


void ActionOnlyCheckBox::nextCheckState()
{
	const QString path = QFileDialog::getOpenFileName(
		this,
		QStringLiteral("Select Image"),
		lastDir,
		QStringLiteral("Image Files (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All Files (*.*)")
	);
	if (path.isEmpty()) return;

	lastDir = QFileInfo(path).absolutePath();

	// 取不带扩展名的文件名（需要带扩展名的话改为 fileName()）
	const QString base = QFileInfo(path).completeBaseName();
	const QString key = this->text();

	const bool match = base.contains(key, Qt::CaseInsensitive);
	this->setChecked(match);
	emit nextStep(match);
}
