#ifndef CANTHREAD_H
#define CANTHREAD_H

#include <QThread>
#include "ControlCAN.h"
#include <QDebug>

class CANThread:public QThread
{
    Q_OBJECT
public:
    CANThread();

    void stop();

    //1.打开设备
    bool openDevice(UINT deviceType,UINT debicIndex,UINT baundRate,int canIndex, int mode);

    //2.初始化CAN
    bool initCAN();

    //3.启动CAN
    bool startCAN();

    //4.发送数据
    bool sendData(UINT channel,UINT ID,BYTE remoteFlag,BYTE externFlag,const unsigned char *data,BYTE len);
    void TransmitCANThread(VCI_CAN_OBJ data);
    //5.关闭设备
    void closeDevice();

    //0.复位设备，  复位后回到3
    bool reSetCAN();

    int getDeviceCount();

    UINT m_deviceType;
    UINT m_debicIndex;
    UINT m_baundRate;
    UINT m_debicCom;
    int canind=0; //第几路can
    int mode = 0;
    bool stopped;

signals:
    void getProtocolData(VCI_CAN_OBJ *vci,unsigned int dwRel,unsigned int channel);
    void boardInfo(VCI_BOARD_INFO vbi);

    void send1data(VCI_CAN_OBJ data);
    void sendSignal(bool flag, VCI_CAN_OBJ data);
private:
    void run();
    void sleep(int msec);

};

#endif // CANTHREAD_H
