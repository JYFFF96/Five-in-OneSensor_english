#include "FastCommandDialog.h"

#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>

static constexpr int COL_NAME   = 0;
static constexpr int COL_CMD    = 1;
static constexpr int COL_SEND   = 2;
static constexpr int COL_DELETE = 3;

FastCommandDialog::FastCommandDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Quick Commands"));
    setModal(true);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    QString bgPath = QCoreApplication::applicationDirPath() + "/images/" + "background.png";
    this->setStyleSheet(QString(
                            "QDialog {"
                            "  background-image: url(:/images/background.png);"
                            "}"
                            ));
    setupUi();
    loadFromIni();

}

void FastCommandDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("Command Name"),
        QStringLiteral("Command Content"),
        QStringLiteral("Send"),
        QStringLiteral("Delete")
    });

    m_table->horizontalHeader()->setSectionResizeMode(COL_NAME, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(COL_CMD,  QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(COL_SEND, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(COL_DELETE, QHeaderView::ResizeToContents);

    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    mainLayout->addWidget(m_table);

    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_btnAdd  = new QPushButton(QStringLiteral("Add"), this);
    m_btnExit = new QPushButton(QStringLiteral("Exit"), this);

    btnLayout->addWidget(m_btnAdd);
    btnLayout->addWidget(m_btnExit);
    mainLayout->addLayout(btnLayout);

    connect(m_btnAdd,  &QPushButton::clicked, this, &FastCommandDialog::onAddRow);
    connect(m_btnExit, &QPushButton::clicked, this, &FastCommandDialog::onExit);
    connect(m_table,   &QTableWidget::cellChanged, this, &FastCommandDialog::onCellChanged);
    m_table->setStyleSheet(
        "QTableWidget {"
        "  background: transparent;"
        "  gridline-color: rgba(255,255,255,80);"
        "}"

        "QTableWidget::item {"
        "  background: transparent;"
        "}"

        // 表头 section 必须单独指定
        "QHeaderView::section {"
        "  background-color: rgba(0, 0, 0, 80);"
        "  color: white;"
        "  border: 1px solid rgba(255,255,255,40);"
        "}"

        // 表头整体（非常关键）
        "QHeaderView {"
        "  background: transparent;"
        "}"
        );
    resize(780, 440);
}

QString FastCommandDialog::iniFilePath() const
{
    // ✅ 按你要求：固定到程序目录/config/FastCommand.ini
    return QCoreApplication::applicationDirPath() + "/config/" + "FastCommand.ini";
}

void FastCommandDialog::ensureConfigDir() const
{
    const QString dirPath = QCoreApplication::applicationDirPath() + "/config";
    QDir dir(dirPath);
    if (!dir.exists())
        dir.mkpath(".");
}

bool FastCommandDialog::parseIniLine(const QString& line, QString& name, QString& content)
{
    QString s = line.trimmed();
    if (s.isEmpty()) return false;
    if (s.startsWith('#') || s.startsWith(';')) return false;

    const int eq = s.indexOf('=');
    if (eq <= 0) return false;

    name = s.left(eq).trimmed();
    content = s.mid(eq + 1); // 内容允许空格，不 trim，原样保存

    // 支持把 \r \n 写在文件里（可选）
    content.replace("\\r", "\r");
    content.replace("\\n", "\n");

    return !name.isEmpty();
}

void FastCommandDialog::loadFromIni()
{
    ensureConfigDir();

    m_loading = true;
    m_table->setRowCount(0);

    QFile f(iniFilePath());
    if (f.open(QIODevice::ReadOnly))
    {
        QTextStream ts(&f);
        ts.setEncoding(QStringConverter::Utf8);

        // 如果文件有 UTF-8 BOM，QTextStream 也能正确处理
        while (!ts.atEnd())
        {
            const QString line = ts.readLine();
            QString name, cmd;
            if (parseIniLine(line, name, cmd))
                appendRow(name, cmd);
        }
        f.close();
    }

    rebuildRowButtonProperties();
    m_loading = false;
}

void FastCommandDialog::saveToIni() const
{
    ensureConfigDir();

    QFile f(iniFilePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    // 写 UTF-8 BOM：用 Windows 记事本也不容易乱码
    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    f.write(reinterpret_cast<const char*>(bom), 3);

    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);

    for (int r = 0; r < m_table->rowCount(); ++r)
    {
        auto *nameItem = m_table->item(r, COL_NAME);
        auto *cmdItem  = m_table->item(r, COL_CMD);
        if (!nameItem || !cmdItem) continue;

        QString name = nameItem->text().trimmed();
        QString cmd  = cmdItem->text();

        if (name.isEmpty()) continue;

        // 把真实换行转成 \r \n，保证一行一个指令
        cmd.replace("\r", "\\r");
        cmd.replace("\n", "\\n");

        ts << name << "=" << cmd << "\n";
    }

    ts.flush();
    f.close();
}

void FastCommandDialog::appendRow(const QString& name, const QString& content)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);

    auto *itemName = new QTableWidgetItem(name);
    itemName->setFlags(itemName->flags() | Qt::ItemIsEditable);
    m_table->setItem(row, COL_NAME, itemName);

    auto *itemCmd = new QTableWidgetItem(content);
    itemCmd->setFlags(itemCmd->flags() | Qt::ItemIsEditable);
    m_table->setItem(row, COL_CMD, itemCmd);

    auto *btnSend = new QPushButton(QStringLiteral("Send"), m_table);
    auto *btnDel  = new QPushButton(QStringLiteral("Delete"), m_table);
    m_table->setCellWidget(row, COL_SEND, btnSend);
    m_table->setCellWidget(row, COL_DELETE, btnDel);

    connect(btnSend, &QPushButton::clicked, this, [this, btnSend]() {
        const int r = btnSend->property("row").toInt();
        if (r < 0 || r >= m_table->rowCount()) return;
        auto *cmdItem = m_table->item(r, COL_CMD);
        if (cmdItem->text().isEmpty()) {
            return;
        }
        emit sendCommand(cmdItem ? cmdItem->text() : QString());
    });

    connect(btnDel, &QPushButton::clicked, this, [this, btnDel]() {
        const int r = btnDel->property("row").toInt();
        if (r < 0 || r >= m_table->rowCount()) return;

        m_table->removeRow(r);
        rebuildRowButtonProperties();
        saveToIni(); // 删除立即落盘
    });
}

void FastCommandDialog::rebuildRowButtonProperties()
{
    for (int r = 0; r < m_table->rowCount(); ++r)
    {
        if (auto *w = m_table->cellWidget(r, COL_SEND))
            w->setProperty("row", r);
        if (auto *w = m_table->cellWidget(r, COL_DELETE))
            w->setProperty("row", r);
    }
}

bool FastCommandDialog::isNameUnique(const QString& name, int exceptRow) const
{
    for (int r = 0; r < m_table->rowCount(); ++r)
    {
        if (r == exceptRow) continue;
        auto *it = m_table->item(r, COL_NAME);
        if (it && it->text().trimmed() == name)
            return false;
    }
    return true;
}

QString FastCommandDialog::makeUniqueDefaultName() const
{
    const QString base = QStringLiteral("New Command");
    QString candidate = base;
    int idx = 1;
    while (!isNameUnique(candidate))
        candidate = base + QString::number(idx++);
    return candidate;
}

void FastCommandDialog::onAddRow()
{
    const QString name = makeUniqueDefaultName();

    m_loading = true;
    appendRow(name, QString());
    rebuildRowButtonProperties();
    m_loading = false;

    saveToIni(); // 添加立即落盘

    const int r = m_table->rowCount() - 1;
    m_table->setCurrentCell(r, COL_CMD);
    m_table->editItem(m_table->item(r, COL_CMD));
}

void FastCommandDialog::onExit()
{
    close();
}

void FastCommandDialog::onCellChanged(int row, int col)
{
    if (m_loading) return;

    auto *nameItem = m_table->item(row, COL_NAME);
    auto *cmdItem  = m_table->item(row, COL_CMD);
    if (!nameItem || !cmdItem) return;

    const QString name = nameItem->text().trimmed();

    if (name.isEmpty())
        return;

    if (col == COL_NAME)
    {
        if (!isNameUnique(name, row))
        {
        QMessageBox::warning(this, QStringLiteral("Information"), QStringLiteral("Command name already exists. Choose another name."));
            m_loading = true;
            nameItem->setText(makeUniqueDefaultName());
            m_loading = false;
        }
    }

    saveToIni(); // 编辑立即落盘（改名/改内容都支持）
}
