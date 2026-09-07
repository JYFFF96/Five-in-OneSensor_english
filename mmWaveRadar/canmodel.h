#ifndef CANMODEL_H
#define CANMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QMutex>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include "ECanVci.h"
#include "common.h"
#include "ControlCAN.h"

enum class CanMsgType
{
    Receive,
    SendSuccess,
    SendFail
};

class CanModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit CanModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // 采集/业务线程调用：高频追加一条
    void appendData(CanMsgType type, const VCI_CAN_OBJ &obj);

    // 兼容你的旧接口：一次性塞一批到缓冲
    void appendBufferedData(const QList<QPair<CanMsgType, VCI_CAN_OBJ>> &buffer);

    void clearData();
    void updateParsing(bool enabled); // 开启/关闭解析

    // 开启/关闭自动保存（路径在 flag=true 时生效）
    void enableAutoSave(bool flag, const QString filePath);

    // 保存文件选项
    void saveToBinary(CanMsgType type, const VCI_CAN_OBJ &obj);
    void saveToTxt(CanMsgType type, const VCI_CAN_OBJ &obj);
    void saveToAsc(CanMsgType type, const VCI_CAN_OBJ &obj);
    void saveToBlf(CanMsgType type, const VCI_CAN_OBJ &obj);
    void saveToExcel(CanMsgType type, const VCI_CAN_OBJ &obj); // CSV

    void saveSaveLine(int line);

public slots:
    void setSaveBinary(bool flag);
    void setSaveTxt(bool flag);
    void setSaveAsc(bool flag);
    void setSaveBlf(bool flag);
    void setSaveExcel(bool flag);

signals:
    void newDataAdded();
    void saveFinished();

private slots:
    void flushBuffer(); // 定时把 _buffer 批量插入 & 保存

private:
    // 数据与并发
    QList<QPair<CanMsgType, VCI_CAN_OBJ>> dataList;  // (type, obj)
    mutable QMutex mutex;
    bool parseEnabled = false;

    // 显示性能
    static constexpr int MAX_ROWS = 200000;
    QTimer _flushTimer;
    int    _flushBatch = 500; // 每批最多插入

    // 缓冲区（生产者：appendData；消费者：flushBuffer）
    QVector<QPair<CanMsgType, VCI_CAN_OBJ>> _buffer;

    // 保存控制
    QString savePath;
    bool autoSaveEnabled = false;
    bool savaBin = false;
    bool savaTxt = false;
    bool savaAsc = false;
    bool savaBlf = false;
    bool savaExcel = false;
    int  saveLine = 1000;    // 最多保存行数
    int  currentLine = 0;    // 已保存行数
    bool sendFinishSignal = false;
};

#endif // CANMODEL_H
