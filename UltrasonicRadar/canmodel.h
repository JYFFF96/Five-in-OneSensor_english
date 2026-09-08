#ifndef CANMODEL_H
#define CANMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QMutex>
#include "ECanVci.h"
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

    void appendData(CanMsgType type, const VCI_CAN_OBJ &obj);
    void appendBufferedData(const QList<QPair<CanMsgType, VCI_CAN_OBJ>> &buffer);
    void clearData();
    void updateParsing(bool enabled); // 开启/关闭解析

    void enableAutoSave(bool flag, const QString filePath);
    // 保存文件
    void saveToBinary(CanMsgType type, const VCI_CAN_OBJ &obj);
    void saveToTxt(CanMsgType type, const VCI_CAN_OBJ &obj);
    void saveToAsc(CanMsgType type, const VCI_CAN_OBJ &obj);
    void saveToBlf(CanMsgType type, const VCI_CAN_OBJ &obj);
    void saveToExcel(CanMsgType type, const VCI_CAN_OBJ &obj);
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

private:
    QList<QPair<CanMsgType, VCI_CAN_OBJ>> dataList;  // 存储 (CanMsgType, CAN_OBJ)
    mutable QMutex mutex;
    bool parseEnabled = false; // 默认不解析

    QString savePath;
    bool autoSaveEnabled = false;
    bool savaBin = false;
    bool savaTxt = false;
    bool savaAsc = false;
    bool savaBlf = false;
    bool savaExcel = false;
    int saveLine = 1000;
    int currentLine = 0;
    bool sendFinishSignal = false;
};

#endif // CANMODEL_H
