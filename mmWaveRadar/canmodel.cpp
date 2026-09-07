#include "canmodel.h"
#include <QDateTime>
#include <QDir>
#include <QScrollBar>
#include <QtCore/QStringConverter> // Qt6 的 QTextStream 编码

CanModel::CanModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    // 50ms 一次，把缓冲里的数据批量搬到 model 并顺手保存
    _flushTimer.setInterval(1000);
    _flushTimer.setSingleShot(false);
    connect(&_flushTimer, &QTimer::timeout, this, &CanModel::flushBuffer);
}

int CanModel::rowCount(const QModelIndex &) const
{
    return dataList.size();
}

int CanModel::columnCount(const QModelIndex &) const
{
    return 7;  // 序号/名称/帧ID/帧类型/帧格式/原始数据/数据解析
}

QVariant CanModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
        return QVariant();

    QMutexLocker locker(&mutex);
    const auto &entry = dataList.at(index.row());
    CanMsgType type = entry.first;
    const VCI_CAN_OBJ &obj = entry.second;

    switch (index.column()) {
    case 0: return index.row() + 1;
    case 1:
        switch (type) {
        case CanMsgType::Receive:     return "Receive";
        case CanMsgType::SendSuccess: return "Sent";
        case CanMsgType::SendFail:    return "Send Failed";
        }
        break;
    case 2: return QString::number(obj.ID, 16).toUpper();
    case 3: return obj.RemoteFlag ? "Remote Frame" : "Data Frame";
    case 4: return obj.ExternFlag ? "Extended Frame" : "Standard Frame";
    case 5: {
        QString s;
        for (int i = 0; i < obj.DataLen; ++i)
            s += QString("%1 ").arg(obj.Data[i], 2, 16, QChar('0')).toUpper();
        return s.trimmed();
    }
    case 6:
        if (!parseEnabled) return "";
        return parseBbstacleData(obj);
    }
    return QVariant();
}

QVariant CanModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) return QVariant();
    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return "No.";
        case 1: return "Name";
        case 2: return "FrameID";
        case 3: return "Frame Type";
        case 4: return "Frame Format";
        case 5: return "Raw Data";
        case 6: return "Parsed Data";
        }
    }
    return QVariant();
}

/*======================== 关键改动：缓冲 + 定时批量插入 ========================*/

void CanModel::appendData(CanMsgType type, const VCI_CAN_OBJ &obj)
{
    // 只入队，不直接触发视图更新
    {
        QMutexLocker locker(&mutex);
        _buffer.push_back(qMakePair(type, obj));
    }
    if (!_flushTimer.isActive())
        _flushTimer.start();
}

void CanModel::appendBufferedData(const QList<QPair<CanMsgType, VCI_CAN_OBJ>> &buffer)
{
    if (buffer.isEmpty()) return;
    {
        QMutexLocker locker(&mutex);
        _buffer.reserve(_buffer.size() + buffer.size());
        for (const auto &e : buffer) _buffer.push_back(e);
    }
    if (!_flushTimer.isActive())
        _flushTimer.start();
}

void CanModel::flushBuffer()
{
    QVector<QPair<CanMsgType, VCI_CAN_OBJ>> batch;
    {
        QMutexLocker locker(&mutex);
        if (_buffer.isEmpty()) {
            _flushTimer.stop();
            return;
        }
        const int n = qMin(_flushBatch, _buffer.size());
        batch = QVector<QPair<CanMsgType, VCI_CAN_OBJ>>(_buffer.begin(), _buffer.begin() + n);
        _buffer.erase(_buffer.begin(), _buffer.begin() + n);
    }

    // 先处理裁剪（保持 MAX_ROWS）
    int removeCount = 0;
    {
        QMutexLocker locker(&mutex);
        if (dataList.size() + batch.size() > MAX_ROWS)
            removeCount = dataList.size() + batch.size() - MAX_ROWS;
        if (removeCount > 0) {
            beginRemoveRows(QModelIndex(), 0, removeCount - 1);
            dataList.erase(dataList.begin(), dataList.begin() + removeCount);
            endRemoveRows();
        }

        // 批量插入
        const int first = dataList.size();
        const int last  = first + batch.size() - 1;
        beginInsertRows(QModelIndex(), first, last);
        for (const auto &e : batch) dataList.append(e);
        endInsertRows();
    }

    emit newDataAdded();

    // —— 同步保存“本批新增行” —— //
    if (autoSaveEnabled && !savePath.isEmpty()) {
        for (const auto &entry : batch) {
            if (currentLine >= saveLine) {
                if (!sendFinishSignal) {
                    sendFinishSignal = true;
                    emit saveFinished();
                }
                break;
            }
            const auto &type = entry.first;
            const auto &obj  = entry.second;

            if (savaBin)   saveToBinary(type, obj);
            if (savaTxt)   saveToTxt(type, obj);
            if (savaAsc)   saveToAsc(type, obj);
            if (savaBlf)   saveToBlf(type, obj);
            if (savaExcel) saveToExcel(type, obj);

            ++currentLine;
        }
    }
}

/*===============================================================================*/

void CanModel::clearData()
{
    QMutexLocker locker(&mutex);
    beginResetModel();
    dataList.clear();
    _buffer.clear();
    endResetModel();
}

void CanModel::updateParsing(bool enabled)
{
    QMutexLocker locker(&mutex);
    parseEnabled = enabled;
}

void CanModel::enableAutoSave(bool flag, const QString filePath)
{
    autoSaveEnabled   = flag;
    savePath          = filePath;
    currentLine       = 0;
    sendFinishSignal  = false;
}

/*============================== 单条写入实现 ==============================*/

void CanModel::saveToBinary(CanMsgType, const VCI_CAN_OBJ &obj)
{
    if (savePath.isEmpty()) return;
    if (!QDir(savePath).exists()) QDir().mkpath(savePath);
    QFile f(savePath + "/can_data.bin");
    if (!f.open(QIODevice::Append)) return;
    f.write(reinterpret_cast<const char*>(&obj), sizeof(VCI_CAN_OBJ));
    f.close();
}

void CanModel::saveToTxt(CanMsgType type, const VCI_CAN_OBJ &obj)
{
    if (savePath.isEmpty()) return;
    QString fileName = savePath + "/can_data.txt";
    QFile file(fileName);
    const bool existed = file.exists();
    if (!file.open(QIODevice::Append | QIODevice::Text)) return;

    QTextStream out(&file);
#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
    out.setEncoding(QStringConverter::Utf8);
#endif
    if (!existed) {
        out << "No.        Name        FrameID        Frame Type    Frame Format    Raw Data        Parsed Data\n";
    }

    // 当前行号：用 dataList.size() 作为显示序号（已插入后再保存的话也成立）
    int index = dataList.size();

    QString dataStr;
    for (int i = 0; i < obj.DataLen; ++i)
        dataStr += QString("%1 ").arg(obj.Data[i], 2, 16, QChar('0')).toUpper();

    auto msgType = (type == CanMsgType::Receive) ? "Receive" :
                       (type == CanMsgType::SendSuccess) ? "Sent" : "Send Failed";
    QString parsed = parseEnabled ? parseBbstacleData(obj) : "";

    out << QString("%1        %2        %3        %4    %5    %6        %7\n")
               .arg(index, 4)
               .arg(msgType, 8)
               .arg(QString::number(obj.ID, 16).toUpper(), 8)
               .arg(obj.RemoteFlag ? "Remote" : "Data", 8)
               .arg(obj.ExternFlag ? "Extended" : "Standard", 8)
               .arg(dataStr.trimmed(), 12)
               .arg(parsed);
    file.close();
}

void CanModel::saveToAsc(CanMsgType type, const VCI_CAN_OBJ &obj)
{
    if (savePath.isEmpty()) return;
    QString fileName = savePath + "/can_data.asc";
    QFile file(fileName);
    const bool existed = file.exists();
    if (!file.open(QIODevice::Append | QIODevice::Text)) return;

    QTextStream out(&file);
#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
    out.setEncoding(QStringConverter::Utf8);
#endif
    if (!existed) {
        QString dateStr = QDateTime::currentDateTime().toString("ddd MMM dd HH:mm:ss.zzz yyyy");
        out << "date " << dateStr << "\n";
        out << "base hex  timestamps absolute\n";
    }

    static QDateTime startTime = QDateTime::currentDateTime();
    qint64 elapsedMs = startTime.msecsTo(QDateTime::currentDateTime());
    double ts = elapsedMs / 1000.0;

    QString frameID = QString::number(obj.ID, 16).toUpper();
    QString dataStr;
    for (int i = 0; i < obj.DataLen; ++i)
        dataStr += QString("%1 ").arg(obj.Data[i], 2, 16, QChar('0')).toUpper();
    dataStr = dataStr.trimmed();

    QString direction = (type == CanMsgType::SendSuccess) ? "Tx" : "Rx";
    out << QString("%1\t1\t%2\t[%3]\t%4\t%5\n")
               .arg(QString::number(ts, 'f', 3))
               .arg(frameID)
               .arg(obj.DataLen)
               .arg(dataStr)
               .arg(direction);
    file.close();
}

void CanModel::saveToBlf(CanMsgType type, const VCI_CAN_OBJ &obj)
{
    if (savePath.isEmpty()) return;
    QString fileName = savePath + "/can_data.blf";
    QFile file(fileName);
    const bool existed = file.exists();
    if (!file.open(QIODevice::Append | QIODevice::Text)) return;

    QTextStream out(&file);
#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
    out.setEncoding(QStringConverter::Utf8);
#endif
    if (!existed) {
        out << "No.\tName\tFrameID\tFrame Type\tFrame Format\tRaw Data\tParsed Data\n";
    }

    int index = dataList.size() + 1;
    QString msgType = (type == CanMsgType::Receive) ? "Receive" :
                          (type == CanMsgType::SendSuccess) ? "Sent" : "Send Failed";
    QString frameType   = obj.RemoteFlag ? "Remote Frame" : "Data Frame";
    QString frameFormat = obj.ExternFlag ? "Extended Frame" : "Standard Frame";

    QString dataStr;
    for (int i = 0; i < obj.DataLen; ++i)
        dataStr += QString("%1 ").arg(obj.Data[i], 2, 16, QChar('0')).toUpper();
    dataStr = dataStr.trimmed();

    QString parsed = parseEnabled ? parseBbstacleData(obj) : "";

    out << index << "\t"
        << msgType << "\t"
        << QString::number(obj.ID, 16).toUpper() << "\t"
        << frameType << "\t"
        << frameFormat << "\t"
        << dataStr << "\t"
        << parsed << "\n";
    file.close();
}

void CanModel::saveToExcel(CanMsgType type, const VCI_CAN_OBJ &obj)
{
    // 注意：这里写 CSV（Excel 可直接打开），只写“当前这一行”
    if (savePath.isEmpty()) return;

    QString fileName = savePath + "/can_data.csv";
    QFile file(fileName);
    const bool existed = file.exists();
    if (!file.open(QIODevice::Append | QIODevice::Text)) return;

    QTextStream out(&file);
#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
    out.setEncoding(QStringConverter::Utf8);
#endif
    if (!existed) {
        // UTF-8 BOM + 表头
        file.write("\xEF\xBB\xBF");
        out << "No.,Name,FrameID,Frame Type,Frame Format,Raw Data,Parsed Data\n";
    }

    int index = dataList.size(); // 以当前行数作为序号
    QString msgType = (type == CanMsgType::Receive) ? "Receive" :
                          (type == CanMsgType::SendSuccess) ? "Sent" : "Send Failed";
    QString frameType   = obj.RemoteFlag ? "Remote Frame" : "Data Frame";
    QString frameFormat = obj.ExternFlag ? "Extended Frame" : "Standard Frame";

    QString dataStr;
    for (int i = 0; i < obj.DataLen; ++i)
        dataStr += QString("%1 ").arg(obj.Data[i], 2, 16, QChar('0')).toUpper();
    dataStr = dataStr.trimmed();

    QString parsed = parseEnabled ? parseBbstacleData(obj) : "";

    out << index << ","
        << msgType << ","
        << QString::number(obj.ID, 16).toUpper() << ","
        << frameType << ","
        << frameFormat << ","
        << dataStr << ","
        << parsed << "\n";
    file.close();
}

/*=============================== 其  他  ===============================*/

void CanModel::saveSaveLine(int line)  { saveLine = line; }
void CanModel::setSaveBinary(bool f)   { savaBin = f; }
void CanModel::setSaveTxt(bool f)      { savaTxt = f; }
void CanModel::setSaveAsc(bool f)      { savaAsc = f; }
void CanModel::setSaveBlf(bool f)      { savaBlf = f; }
void CanModel::setSaveExcel(bool f)    { savaExcel = f; }
