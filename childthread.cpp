#include "childthread.h"
#include "flagfile.h"


ChildThread::ChildThread(QObject *parent) : QObject(parent)
{

}

ChildThread::ChildThread(qintptr str,QMutex *ThreadMutex,QObject *parent) : QObject(parent)
{
    this->SocketPtr=str;
    this->ThreadQmutex=ThreadMutex;
    this->init();
}

void ChildThread::SocketDeal()
{
    if(this->ThreadRunFlag==true)
    {
        qDebug()<<"开始读取一包数据"
                <<endl;
        QVector<QVariantList> timeList(4),
                         EquipmentTypeList(4),
                         InstallLocationList(4),
                         EquipmentNumList(4),
                         DataList(4);
        QDateTime current_date_time =QDateTime::currentDateTime();
        QString current_date =current_date_time.toString("yyyy.MM.dd hh:mm:ss.zzz");
        float Datatemp=0;
        char Datatemp1[4];
        unsigned int i=0;
        QByteArray arraytemp=this->MyTcpScoket->readAll();
        array.append(arraytemp);
        unsigned int size=array.size();
        qDebug()<<"本次数据包字节长度："
                <<size
                <<'\n'
                <<"本次数据包十六进制长度"
                <<array.toHex().size()
                <<endl;
        /*************************************************************/
        //提取数据帧
        for(i=0;i<array.size();)
        {
            if((uchar)array.at(i)==0x55&&
               (uchar)array.at(i+1)==0xaa&&
               (uchar)array.at(i+2)==0x55&&
               (uchar)array.at(i+3)==0xaa)
            {
                if(((size-1)-(i+3))>=7)//检测到帧头后，后面至少还有一帧数据
                {
                    equipmentType=static_cast<EquipmentType>(array.at(i+4));
                    switch (equipmentType)
                    {//判断设备类型
                    case EquipmentType::Oxygen:
                        equipmentTypeIndex=Oxygen;
                        break;
                    case EquipmentType::PH:
                        equipmentTypeIndex=PH;
                        break;
                    case EquipmentType::Temperature:
                        equipmentTypeIndex=Temperature;
                        break;
                    case EquipmentType::Turbidity:
                        equipmentTypeIndex=Turbidity;
                        break;
                    default:
                        qDebug()<<"设备类型字节读取错误"<<endl;
                        break;
                    }
                    timeList[equipmentTypeIndex]<<current_date;
                    EquipmentTypeList[equipmentTypeIndex]<<array.at(i+4);
                    InstallLocationList[equipmentTypeIndex]<<array.at(i+5);
                    EquipmentNumList[equipmentTypeIndex]<<array.at(i+6);
                    for(int j=0;j<4;j++)
                    {Datatemp1[j]=array.at(i+7+j);}
                    memcpy(&Datatemp,Datatemp1,sizeof(Datatemp1));
                    DataList[equipmentTypeIndex]<<Datatemp;
                    i+=11;
                }
                else{break;}//检测到帧头后，后面不足一帧数据
            }
            else//没有检测到帧头
            {   if(((size-1)-(i+3))<7)//剩余数据少于一个数据帧，退出循环，将剩余数据留给下一次readyread
                {break;}
                else{i++;}//剩余数据多于或等于一个数据帧，指针向后拨动
            }
        }
        qDebug()<<"一包数据读取并解析完毕"
                <<endl;
        //提取数据帧结束
        /*********************************************************/

        /*********************************************************/
        //向数据库中写入数据
        QString ConnectName=QString::number(quintptr(QThread::currentThreadId()));
        {
            this->ThreadQmutex->lock();//加入锁，防止多线程同时访问数据库
            QSqlDatabase Mydatabase=QSqlDatabase::addDatabase("SQLITECIPHER",ConnectName);
            if(!OpenDataBase(Mydatabase,
                            "../SensorDatabase.db",
                            NULL,
                            "QSQLITE_CREATE_KEY"))
            {qDebug()<<"打开数据库出错"
                     <<endl;
            }else{qDebug()<<"打开数据库成功"
                         <<endl;}
            qDebug()<<"开始写入"
                    <<endl;
            for(int n=0;n<4;n++)
            {if(timeList[n].size()!=0)
                {
                AddDataToDataTable(Mydatabase,this->DataTableName.at(n),
                                   timeList[n],
                                   EquipmentTypeList[n],
                                   InstallLocationList[n],
                                   EquipmentNumList[n],
                                   DataList[n]);
                }
            }
            qDebug()<<"写入完成"
                    <<endl;
            this->ThreadQmutex->unlock();
        }
        QSqlDatabase::removeDatabase(ConnectName);
        //向数据库中写入数据结束
        /****************************************************/
        //这里还要把原来array数组中已经提取并写入数据库的数据剔除掉
        array.remove(0,i-1);
        qDebug()<<"一包数据读取加写入处理结束  "
                <<"剩余数据长度:"
                <<array.size()
                <<endl;
        /*****************************************************/
    }
}

void ChildThread::init()
{
    qDebug()<<"子线程初始化开始:"
            <<this->SocketPtr
            <<endl;
    bool flag=false;
    this->MyTcpScoket=new QTcpSocket(this);
    flag=this->MyTcpScoket->setSocketDescriptor(this->SocketPtr);
    if(flag==false)
    {
        qDebug()<<"通信套接字建立出错:"
                <<endl;
        return;
    }else{
                qDebug()<<"通信套接字建立成功:"
                        <<endl;
    }
    connect(this->MyTcpScoket,&QTcpSocket::readyRead,
            this,&ChildThread::SocketDeal);
    qDebug()<<"子线程信号槽连接成功:"
            <<endl;
}

void ChildThread::SetThreadRunflag(bool flag)
{
    this->ThreadRunFlag=flag;
}

