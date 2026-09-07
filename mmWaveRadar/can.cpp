#include "can.h"
#include <QThread>
bool  IsOpenFlag=false;
long mrec=0;
Thread::Thread()//构造函数//初始化变量
{
    QLibrary lib("ECanVci64.dll");
    if(true==lib.load())
       qDebug()<<"ECanVci.dll load ok";
    else
        qDebug() << "Load failed:" << lib.errorString();

    pOpenDevice = (OpenDevice *)lib.resolve("OpenDevice");
    pCloseDevice = (CloseDevice *)lib.resolve("CloseDevice");
    pInitCAN = (InitCAN *)lib.resolve("InitCAN");
    pStartCAN = (StartCAN *)lib.resolve("StartCAN");
    pTransmitCAN = (Transmit *)lib.resolve("Transmit");
    pReceive = (Receive *)lib.resolve("Receive");
    pGetReceiveNum = (GetReceiveNum *)lib.resolve("GetReceiveNum");
    pClearBuffer = (ClearBuffer *)lib.resolve("ClearBuffer");
    pReadErrInfoCAN = (ReadErrInfo*)lib.resolve("ReadErrInfo");
    pResetCAN=(ResetCAN *)lib.resolve("ResetCAN");
    pSetReference=(SetReference *)lib.resolve("SetReference");
    pReadBoardInfo=(ReadBoardInfo *)lib.resolve("ReadBoardInfo");
    pReadCANStatus=(ReadCANStatus *)lib.resolve("ReadCANStatus");
}
void Thread::run()
{
    while (musbcanstart) {
        if (!stopped) {
            ReceiveCANThread();//接收数据
        }
    }

}

void Thread::stop()
{
    stopped = true;
}

void Thread::ReceiveCANThread()//接收数据线程
{
    bool ok;
    ERR_INFO vei;
    CAN_OBJ preceive[1000];
    CAN_OBJ psend;
//    int baud=0x10000000;//参数有关数据缓冲区地址首指针

    //接收
    ULONG res = 10;




        res=pReceive(devtype,devind,canind,preceive,50,0);

        //qDebug()<<"读取帧数："<<res;
        for(int i=0;i<res;i++)
        {
            if(res==4294967295)
            {
                if(pReadErrInfoCAN(devtype,devind,canind,&vei)!=STATUS_ERR)
                {
                    qDebug()<<"Failed to read data!"<<"Error code:："<<QString::number(vei.ErrCode,16);
                }
            }
            emit send1data(preceive[i]);
            // parseData(preceive[i]);
            QString receive_str = "Receive";
            mrec++;
            receive_str.append(QString::number(mrec,16));
            receive_str.append("FrameID:");
            receive_str.append(QString::number(preceive[i].ID,16));
            receive_str.append("    Data：");
            for (int a=0;a<preceive[i].DataLen;a++)
            {
                receive_str.append(QString::number(preceive[i].Data[a],16));
                receive_str.append(" ");
            }
            emit my_signal(receive_str);//子线程处理完毕//触发自定义的信号
        }//for


    sleep(1);
}

void Thread::TransmitCANThread(CAN_OBJ psend)//发送数据线程
{
    ULONG Tr;
     Tr=pTransmitCAN(devtype,devind,canind,&psend,1);//返回实际发送的帧数
    //devtype设备类型号//devind设备索引号//canind第几路CAN//&psend要发送的数据帧数组的首指针//1要发送的数据帧数组的长度//
    if(Tr==STATUS_ERR){
         qDebug() << "send err";
    }
    sendSignal(Tr, psend);
}

void Thread::OpenCANThread(int devType, int canIndex, int baud, int mode)//打开设备
{
    devtype = devType;
    canind = canIndex;
    mrec=0;
    bool ok;
    ERR_INFO vei;
    CAN_OBJ preceive[100];
//    CAN_OBJ psend;
//    int baud=0x10000000;//参数有关数据缓冲区地址首指针
//      int pdata=0x00;//参数有关数据缓冲区地址首指针
   //打开设备
    musbcanstart=false;
    if(pOpenDevice(devtype,devind,res)==STATUS_ERR )//为1表示操作成功，0表示操作失败。
    {
        QThread::msleep(500);
        if(pReadErrInfoCAN(devtype,devind,canind,&vei)!=STATUS_ERR)
        {
          qDebug()<<"Open Failed："<<QString::number(vei.ErrCode,16);
        }


        IsOpenFlag=false;
        return;
    }
    else
    {

      IsOpenFlag=true;
      qDebug()<<"open successed";
    }
    //初始化
    INIT_CONFIG init_config;//INIT_CONFIG
    //init_config.Mode=0;//0正常模式//1为只听模式//2为自发自收模式//
    switch(mode)
    {
    case 0:
        init_config.Mode=0;
        break;
    case 1:
        init_config.Mode=1;
        break;
    case 2:
        init_config.Mode=2;
        break;
    }

    init_config.Filter=0;//滤波方式，单滤波
    switch(baud)
    {
        case 1000: //1000

                init_config.Timing0 = 0;
                init_config.Timing1 =0x14;
                break;
        case 800: //800

                        init_config.Timing0 = 0;
                        init_config.Timing1 = 0x16;
                        break;
                    case 666: //666

                        init_config.Timing0 = 0x80;
                        init_config.Timing1 = 0xb6;
                        break;
                    case 500: //500

                        init_config.Timing0 = 0;
                        init_config.Timing1 = 0x1c;
                        break;
                    case 400://400

                        init_config.Timing0 = 0x80;
                        init_config.Timing1 = 0xfa;
                        break;
                    case 250://250

                        init_config.Timing0 = 0x01;
                        init_config.Timing1 = 0x1c;
                        break;
                    case 200://200

                        init_config.Timing0 = 0x81;
                        init_config.Timing1 = 0xfa;
                        break;
                    case 125://125

                        init_config.Timing0 = 0x03;
                        init_config.Timing1 = 0x1c;
                        break;
                    case 100://100

                        init_config.Timing0 = 0x04;
                        init_config.Timing1 = 0x1c;
                        break;
                    case 80://80

                        init_config.Timing0 = 0x83;
                        init_config.Timing1 = 0xff;
                        break;
                    case 50://50

                        init_config.Timing0 = 0x09;
                        init_config.Timing1 = 0x1c;
                        break;
    }

    init_config.AccCode=0x000000;//验收码
    init_config.AccMask=0xFFFFFF;//屏蔽码
    QThread::msleep(100);
    if(pInitCAN(devtype,devind,canind,&init_config)==STATUS_ERR)
    {
        qDebug("Init Error");
        pCloseDevice(devtype,devind);
        return;
    }
    else
      {
        qDebug()<<"Init successed";
        qDebug()<<"Timer0："<<init_config.Timing0;
        qDebug()<<"Timer1："<<init_config.Timing1;
    }

    if(pStartCAN(devtype,devind,canind)==STATUS_ERR){
        qDebug()<<"start fail";
        pCloseDevice(devtype,devind);
        return;
    }else
    {
        qDebug()<<"start successed";
        musbcanstart=true;
        stopped = false;
    }
}

void Thread::CloseCANThread()
{
    pCloseDevice(devtype,devind);
    IsOpenFlag=false;
    qDebug()<<"closed";
}

void Thread::parseData(CAN_OBJ data)
{
    if (data.ID == 0x060a) {
        obsCount = data.Data[0];
        obsList.clear();
    } else if (data.ID == 0x060b) {
        ObstacleInfo obj;

        // 目标 ID
        obj.id = data.Data[0];

        // 目标纵向距离
        obj.distLong = ((data.Data[1] * 32 + (data.Data[2] >> 3)) * 0.2) - 500;

        // 目标横向距离
        obj.distLat = (((data.Data[2] & 0x07) * 256 + data.Data[3]) * 0.2) - 204.6;

        // 目标纵向速度
        obj.vrelLong = ((data.Data[4] * 4 + (data.Data[5] >> 6)) * 0.25) - 128;

        // 目标横向速度
        obj.vrelLat = (((data.Data[5] & 0x3F) * 8 + (data.Data[6] >> 5)) * 0.25) - 64;

        // 目标动态属性
        obj.dynProp = data.Data[6] & 0x07;

        // RCS
        obj.rcs = (data.Data[7] * 0.5) - 64;

        // 距离 角度 速度
        obj.distance = sqrt(obj.distLong* obj.distLong+ obj.distLat*obj.distLat);
        double angle_rad = atan2(obj.distLat, obj.distLong);
        obj.angle = angle_rad * 180.0 / M_PI;
        // double v =
        obj.speed = obj.vrelLong * std::cos(angle_rad) + obj.vrelLat * std::sin(angle_rad);
        obsList.append(obj);
        if (obsList.count() == obsCount) {
            // 绘制到界面中
            emit sendObstacleInfoList(obsList);
        }
    }
}
