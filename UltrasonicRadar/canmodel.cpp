#include "canmodel.h"
#include <QFile>
#include <QDateTime>
#include <QDir>
#include "xlsxdocument.h"
#include "xlsxformat.h"

const int MAX_ROWS = 200000;

CanModel::CanModel(QObject *parent)
    : QAbstractTableModel{parent}
{}

int CanModel::rowCount(const QModelIndex &) const
{
    return dataList.size();
}

int CanModel::columnCount(const QModelIndex &) const
{
    return 7;  // 7列
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
    case 0: return index.row() + 1;  // 序号（从 1 开始）
    case 1:  // 名称
        switch (type) {
        case CanMsgType::Receive: return "Receive";
        case CanMsgType::SendSuccess: return "Sent";
        case CanMsgType::SendFail: return "Send Failed";
        }
    case 2: return QString::number(obj.ID, 16).toUpper();  // 帧ID（16进制）
    case 3: return obj.RemoteFlag ? "Remote Frame" : "Data Frame"; // 帧类型
    case 4: return obj.ExternFlag ? "Extended Frame" : "Standard Frame"; // 帧格式
    case 5: {  // 原始数据
        QString dataStr;
        for (int i = 0; i < obj.DataLen; ++i) {
            dataStr += QString("%1 ").arg(obj.Data[i], 2, 16, QChar('0')).toUpper();
        }
        return dataStr.trimmed();
    }
    case 6: {  // 数据解析
        if (!parseEnabled) return ""; // 默认不解析
        if (obj.ID == 0x0611)
            return QString("1 Sensor:%1%2mm, 2 Sensor:%3%4mm, 3 Sensor:%5%6mm, 4 Sensor:%7%8mm")
                .arg(QString::number(obj.Data[0],16))
                .arg(obj.Data[1], 2, 16, QChar('0'))
                .arg(QString::number(obj.Data[2],16))
                .arg(obj.Data[3], 2, 16, QChar('0'))
                .arg(QString::number(obj.Data[4],16))
                .arg(obj.Data[5], 2, 16, QChar('0'))
                .arg(QString::number(obj.Data[6],16))
                .arg(obj.Data[7], 2, 16, QChar('0'));
        else if (obj.ID == 0x0612)
            return QString("5 Sensor:%1%2mm, 6 Sensor:%3%4mm, 7 Sensor:%5%6mm, 8 Sensor:%7%8mm")
                .arg(QString::number(obj.Data[0],16))
                .arg(obj.Data[1], 2, 16, QChar('0'))
                .arg(QString::number(obj.Data[2],16))
                .arg(obj.Data[3], 2, 16, QChar('0'))
                .arg(QString::number(obj.Data[4],16))
                .arg(obj.Data[5], 2, 16, QChar('0'))
                .arg(QString::number(obj.Data[6],16))
                .arg(obj.Data[7], 2, 16, QChar('0'));
    }
    default: return QVariant();
    }
}

QVariant CanModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return "No.";
        case 1: return "Name";
        case 2: return "FrameID";
        case 3: return "Frame Type";
        case 4: return "Frame Format";
        case 5: return "Raw Data";
        case 6: return "Parsed Data";
        default: return QVariant();
        }
    }
    return QVariant();
}

void CanModel::appendData(CanMsgType type, const VCI_CAN_OBJ &obj)
{
    QMutexLocker locker(&mutex);
    if (dataList.size() >= MAX_ROWS) {
        beginRemoveRows(QModelIndex(), 0, 0);  // 移除最早一行（第一行）
        dataList.removeFirst();
        endRemoveRows();
    }
    beginInsertRows(QModelIndex(), dataList.size(), dataList.size());
    dataList.append(qMakePair(type, obj));
    endInsertRows();
    emit newDataAdded();  // 触发界面更新

    if (autoSaveEnabled) {
        if (currentLine < saveLine) {
            if (savaBin)
                saveToBinary(type, obj);
            if (savaTxt)
                saveToTxt(type, obj);
            if (savaAsc)
                saveToAsc(type, obj);
            if (savaBlf)
                saveToBlf(type, obj);
            if (savaExcel)
                saveToExcel(type, obj);
            currentLine++;
        } else {
            if (!sendFinishSignal) {
                emit saveFinished();
            }
        }
    }
}

void CanModel::appendBufferedData(const QList<QPair<CanMsgType, VCI_CAN_OBJ>> &buffer)
{
    if (buffer.isEmpty())
        return;

    QMutexLocker locker(&mutex);
    // 裁剪前端数据，保持总量不超过 MAX_ROWS
    while (dataList.size() + buffer.size() > MAX_ROWS) {
        dataList.removeFirst();  // 或者批量移除效率更高
    }beginInsertRows(QModelIndex(), dataList.size(), dataList.size() + buffer.size() - 1);
    dataList.append(buffer);
    endInsertRows();

    if (autoSaveEnabled) {
        for (const auto &entry : buffer) {
            if (currentLine < saveLine) {
                const VCI_CAN_OBJ &obj = entry.second;
                CanMsgType type = entry.first;
                if (savaBin)
                    saveToBinary(type, obj);
                if (savaTxt)
                    saveToTxt(type, obj);
                if (savaAsc)
                    saveToAsc(type, obj);
                if (savaBlf)
                    saveToBlf(type, obj);
                if (savaExcel)
                    saveToExcel(type, obj);
                currentLine++;
            } else {
                if (!sendFinishSignal) {
                    emit saveFinished();
                }
            }
        }
    }
}

void CanModel::clearData()
{
    QMutexLocker locker(&mutex);
    beginResetModel();
    dataList.clear();
    endResetModel();
}

void CanModel::updateParsing(bool enabled)
{
    QMutexLocker locker(&mutex);
    parseEnabled = enabled;
}

void CanModel::enableAutoSave(bool flag, const QString filePath)
{
    autoSaveEnabled = flag;
    savePath = filePath;
    currentLine = 0;
    sendFinishSignal = false;
}

void CanModel::saveToBinary(CanMsgType type, const VCI_CAN_OBJ &obj)
{
    if (savePath.isEmpty())
        return;

    // 检查路径是否存在，不存在则创建
    if (!QDir(savePath).exists()) {
        QDir().mkpath(savePath);
    }

    QFile file(savePath + "/can_data.bin");
    if (!file.open(QIODevice::Append))
        return;

    // 将 CAN_OBJ 的原始数据写入文件
    file.write(reinterpret_cast<const char*>(&obj), sizeof(VCI_CAN_OBJ));
    file.close();
}

void CanModel::saveToTxt(CanMsgType type, const VCI_CAN_OBJ &obj)
{
    if (savePath.isEmpty())
        return;

    QString fileName = savePath + "/can_data.txt";
    QFile file(fileName);

    bool fileExists = file.exists();
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qDebug() << "Unable to open TXT File：" << fileName;
        return;
    }

    QTextStream out(&file);

    // 如果是新文件，则添加表头
    if (!fileExists) {
        out << "No.        Name        FrameID        Frame Type    Frame Format    Raw Data        Parsed Data\n";
    }

    int index = dataList.size(); // 获取当前数据索引

    QString dataStr;
    for (int i = 0; i < obj.DataLen; ++i) {
        dataStr += QString("%1 ").arg(obj.Data[i], 2, 16, QChar('0')).toUpper();
    }

    QString parsedData;
    if (parseEnabled) {
        if (obj.ID == 0x0611) {
            parsedData = QString("1 Sensor:%1%2mm 2 Sensor:%3%4mm 3 Sensor:%5%6mm 4 Sensor:%7%8mm")
                             .arg(QString::number(obj.Data[0], 16))
                             .arg(obj.Data[1], 2, 16, QChar('0'))
                             .arg(QString::number(obj.Data[2], 16))
                             .arg(obj.Data[3], 2, 16, QChar('0'))
                             .arg(QString::number(obj.Data[4], 16))
                             .arg(obj.Data[5], 2, 16, QChar('0'))
                             .arg(QString::number(obj.Data[6], 16))
                             .arg(obj.Data[7], 2, 16, QChar('0'));
        }
    }

    out << QString("%1        %2        %3        %4    %5    %6        %7\n")
               .arg(index, 4)
               .arg((obj.RemoteFlag ? "Remote Frame" : "Data Frame"), 8)
               .arg(QString::number(obj.ID, 16).toUpper(), 8)
               .arg((obj.RemoteFlag ? "Remote" : "Data"), 8)
               .arg((obj.ExternFlag ? "Extended" : "Standard"), 8)
               .arg(dataStr.trimmed(), 12)
               .arg(parsedData);

    file.close();
}

void CanModel::saveToAsc(CanMsgType type, const VCI_CAN_OBJ &obj)
{
    if (savePath.isEmpty())
        return;

    QString fileName = savePath + "/can_data.asc";
    QFile file(fileName);

    bool fileExists = file.exists();

    // 追加写入
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qDebug() << "Unable to open ASC File：" << fileName;
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // 如果是新文件，写入文件头
    if (!fileExists) {
        QString dateStr = QDateTime::currentDateTime().toString("ddd MMM dd HH:mm:ss.zzz yyyy");
        out << "date " << dateStr << "\n";
        out << "base hex  timestamps absolute\n";
    }

    // 计算时间戳（相对时间，单位：秒）
    static QDateTime startTime = QDateTime::currentDateTime();
    qint64 elapsedMs = startTime.msecsTo(QDateTime::currentDateTime());
    double timestamp = elapsedMs / 1000.0; // 转换为秒

    // CAN ID（16进制格式）
    QString frameID = QString::number(obj.ID, 16).toUpper();

    // 帧类型（远程帧 / 数据帧）
    QString frameType = obj.RemoteFlag ? "r" : "";

    // 解析数据
    QString dataStr;
    for (int i = 0; i < obj.DataLen; ++i) {
        dataStr += QString("%1 ").arg(obj.Data[i], 2, 16, QChar('0')).toUpper();
    }
    dataStr = dataStr.trimmed();

    // 方向（Tx：发送, Rx：接收）
    QString direction = (obj.SendType == 1) ? "Tx" : "Rx";

    // 写入数据
    out << QString("%1\t1\t%2\t[%3]\t%4\t%5\n")
               .arg(QString::number(timestamp, 'f', 3)) // 时间戳，保留 3 位小数
               .arg(frameID)
               .arg(obj.DataLen)
               .arg(dataStr)
               .arg(direction);

    file.close();
}

void CanModel::saveToBlf(CanMsgType type, const VCI_CAN_OBJ &obj)
{
    if (savePath.isEmpty())
        return;

    QString fileName = savePath + "/can_data.blf";
    QFile file(fileName);

    bool fileExists = file.exists();

    // 以文本模式追加打开
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qDebug() << "Unable to open BLF File：" << fileName;
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8); // UTF-8 编码

    // 如果是新文件，先写入表头
    if (!fileExists) {
        out << "No.\tName\tFrameID\tFrame Type\tFrame Format\tRaw Data\tParsed Data\n";
    }

    // 获取当前时间
    QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");

    // 计算当前序号
    int index = dataList.size() + 1;

    // 解析 CAN 消息类型
    QString msgType;
    switch (type) {
    case CanMsgType::Receive:
        msgType = "Receive";
        break;
    case CanMsgType::SendSuccess:
        msgType = "Sent";
        break;
    case CanMsgType::SendFail:
        msgType = "Send Failed";
        break;
    }

    // 解析帧类型
    QString frameType = obj.RemoteFlag ? "Remote Frame" : "Data Frame";

    // 解析帧格式
    QString frameFormat = obj.ExternFlag ? "Extended Frame" : "Standard Frame";

    // 解析原始数据
    QString dataStr;
    for (int i = 0; i < obj.DataLen; ++i) {
        dataStr += QString("%1 ").arg(obj.Data[i], 2, 16, QChar('0')).toUpper();
    }
    dataStr = dataStr.trimmed();

    // 解析数据（如果启用了解析）
    QString parsedData;
    if (parseEnabled) {
        if (obj.ID == 0x0611) {
            parsedData = QString("1 Sensor:%1%2mm 2 Sensor:%3%4mm 3 Sensor:%5%6mm 4 Sensor:%7%8mm")
                             .arg(QString::number(obj.Data[0],16))
                             .arg(obj.Data[1], 2, 16, QChar('0'))
                             .arg(QString::number(obj.Data[2],16))
                             .arg(obj.Data[3], 2, 16, QChar('0'))
                             .arg(QString::number(obj.Data[4],16))
                             .arg(obj.Data[5], 2, 16, QChar('0'))
                             .arg(QString::number(obj.Data[6],16))
                             .arg(obj.Data[7], 2, 16, QChar('0'));
        } else if (obj.ID == 0x0612) {
            parsedData = QString("5 Sensor:%1%2mm 6 Sensor:%3%4mm 7 Sensor:%5%6mm 8 Sensor:%7%8mm")
                             .arg(QString::number(obj.Data[0],16))
                             .arg(obj.Data[1], 2, 16, QChar('0'))
                             .arg(QString::number(obj.Data[2],16))
                             .arg(obj.Data[3], 2, 16, QChar('0'))
                             .arg(QString::number(obj.Data[4],16))
                             .arg(obj.Data[5], 2, 16, QChar('0'))
                             .arg(QString::number(obj.Data[6],16))
                             .arg(obj.Data[7], 2, 16, QChar('0'));
        }
    }

    // 写入数据（使用制表符分隔）
    out << index << "\t"
        << msgType << "\t"
        << QString::number(obj.ID, 16).toUpper() << "\t"
        << frameType << "\t"
        << frameFormat << "\t"
        << dataStr << "\t"
        << parsedData << "\n";

    file.close();
}

void CanModel::saveToExcel(CanMsgType type, const VCI_CAN_OBJ &obj)
{
    if (savePath.isEmpty())
        return;

    QString fileName = savePath + "/can_data.csv";
    QFile file(fileName);

    bool fileExists = file.exists();
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qDebug() << "Unable to open CSV File：" << fileName;
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8); // 确保 UTF-8 编码

    // 如果是新文件，则添加 UTF-8 BOM
    if (!fileExists) {
        file.write("\xEF\xBB\xBF"); // UTF-8 BOM
        out << "No.,Name,FrameID,Frame Type,Frame Format,Raw Data,Parsed Data\n";
    }

    int startRow = fileExists ? dataList.size() + 1 : 1;

    for (int i = 0; i < dataList.size(); ++i) {
        const auto &entry = dataList.at(i);
        CanMsgType type = entry.first;
        const VCI_CAN_OBJ &obj = entry.second;

        QStringList row;
        row << QString::number(startRow + i);
        row << ((type == CanMsgType::Receive) ? "Receive" :
                    (type == CanMsgType::SendSuccess) ? "Sent" : "Send Failed");
        row << QString::number(obj.ID, 16).toUpper();
        row << (obj.RemoteFlag ? "Remote Frame" : "Data Frame");
        row << (obj.ExternFlag ? "Extended Frame" : "Standard Frame");

        QString dataStr;
        for (int j = 0; j < obj.DataLen; ++j) {
            dataStr += QString("%1 ").arg(obj.Data[j], 2, 16, QChar('0')).toUpper();
        }
        row << dataStr.trimmed();

        // 解析数据
        QString parsedData;
        if (parseEnabled) {
            if (obj.ID == 0x0611) {
                parsedData = QString("1 Sensor:%1%2mm 2 Sensor:%3%4mm 3 Sensor:%5%6mm 4 Sensor:%7%8mm")
                                 .arg(QString::number(obj.Data[0], 16))
                                 .arg(obj.Data[1], 2, 16, QChar('0'))
                                 .arg(QString::number(obj.Data[2], 16))
                                 .arg(obj.Data[3], 2, 16, QChar('0'))
                                 .arg(QString::number(obj.Data[4], 16))
                                 .arg(obj.Data[5], 2, 16, QChar('0'))
                                 .arg(QString::number(obj.Data[6], 16))
                                 .arg(obj.Data[7], 2, 16, QChar('0'));
            }
        }
        row << parsedData;

        out << row.join(",") << "\n";
    }

    file.close();
}

void CanModel::saveSaveLine(int line)
{
    saveLine = line;
}

void CanModel::setSaveBinary(bool flag)
{
    savaBin = flag;
}

void CanModel::setSaveTxt(bool flag)
{
    savaTxt = flag;
}

void CanModel::setSaveAsc(bool flag)
{
    savaAsc = flag;
}

void CanModel::setSaveBlf(bool flag)
{
    savaBlf = flag;
}

void CanModel::setSaveExcel(bool flag)
{
    savaExcel = flag;
}
