#pragma once

#include <QDialog>

class QTableWidget;
class QPushButton;

class FastCommandDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FastCommandDialog(QWidget *parent = nullptr);

signals:
    void sendCommand(const QString& commandContent);

private slots:
    void onAddRow();
    void onExit();
    void onCellChanged(int row, int col);

private:
    void setupUi();

    // ini
    QString iniFilePath() const;
    void ensureConfigDir() const;

    void loadFromIni();
    void saveToIni() const;                 // 直接重写整个 ini（最稳）
    static bool parseIniLine(const QString& line, QString& name, QString& content);

    // table helpers
    void appendRow(const QString& name, const QString& content);
    void rebuildRowButtonProperties();
    bool isNameUnique(const QString& name, int exceptRow = -1) const;
    QString makeUniqueDefaultName() const;

private:
    QTableWidget* m_table = nullptr;
    QPushButton*  m_btnAdd = nullptr;
    QPushButton*  m_btnExit = nullptr;

    bool m_loading = false;
};
