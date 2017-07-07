//
// Created by wangqi on 17-6-15.
//

#include "STGPSReciever.h"
#include "BATNetSDKRawAPI.h"
#include "STGPSProtocolAPI.h"
#include "base/Logging.h"
#include <boost/bind.hpp>
#include <sstream>
#include "Base.h"


STGPSReciever::STGPSReciever(const std::string &log_path,int nDevType, const std::string& strDevSN)
                :handler(-1)
                ,startTime(muduo::TimeZone::toUtcTime(muduo::Timestamp::now().secondsSinceEpoch()))
                ,_log_path(log_path)
                ,file()
                ,mtx()
                ,con(mtx)
                ,ready(false)
                ,sending(true)
                ,sendMsg_thread(boost::bind(&STGPSReciever::sendMsg,this))

{
    BATNetSDK_Init(nDevType, const_cast<char*>(strDevSN.c_str()),true);
    sendMsg_thread.start();
}

STGPSReciever::~STGPSReciever()
{
    ready = false;
    sending = false;
    sendMsg_thread.join();
    if(handler != -1)
    {
        BATNetSDKRaw_DeleteObj(handler);
    }
    BATNetSDK_Release();
}

int STGPSReciever::start(const std::string& ip,uint16_t port)
{
    CCMS_NETADDR addr = { {0},0};
    memcpy(addr.chIP,ip.c_str(),ip.length());
    addr.nPort = port;
    handler = BATNetSDKRaw_CreateClientObj(&addr);
    BATNetSDKRaw_SetMsgCallBack(handler,RecvDatahand,this);
    BATNetSDKRaw_SetConnCallBack(handler,Connecthand,this);
    BATNetSDKRaw_Start(handler);
    return 0;
}

void STGPSReciever::writeToFile(std::string msg)
{
    struct tm currentTime = muduo::TimeZone::toUtcTime(muduo::Timestamp::now().secondsSinceEpoch());
    if(startTime.tm_year - currentTime.tm_year > 1 || startTime.tm_yday - currentTime.tm_yday > 1)
    {
        startTime = currentTime;
        file.close();
    }
    if(!file.is_open())
    {
        std::ostringstream os;
        os << startTime.tm_year + 1900 << "-" << startTime.tm_mon + 1 << "-" << startTime.tm_mday;
        file.open((_log_path + os.str()).c_str(),std::ofstream::out | std::ofstream::in | std::ofstream::app);
        os.str("");
    }
    file << msg;
    file.close();
}


void STGPSReciever::FormatMsg(Head& head,GPSInfo& gps_info,CarInfo& car_info,bool is_gps_info)
{
    std::ostringstream os;
    struct tm currentTime = muduo::TimeZone::toUtcTime(muduo::Timestamp::now().secondsSinceEpoch());
    os << currentTime.tm_year + 1900 << "-" << currentTime.tm_mon + 1 << "-" << currentTime.tm_mday << " " << currentTime.tm_hour + 8 << ":" << currentTime.tm_min << ":" << currentTime.tm_sec
           << "," << head.cityCode << "," << std::string(head.vehileId,8) << "," << (int)head.vehileColor << ",";
    if(is_gps_info)
    {
        double latitude = double(gps_info.latitude / 100000) + ((double(gps_info.latitude / 100000) - int(gps_info.latitude / 100000)) * 100 / 60);
        double longitude = double(gps_info.longitude / 100000) + ((double(gps_info.longitude / 100000) - int(gps_info.longitude / 100000)) * 100 / 60);
        os << gps_info.time.nYear << "-" << (int)gps_info.time.nMonth << "-" << (int)gps_info.time.nDay << " " << (int)gps_info.time.nHour << ":" << (int)gps_info.time.nMinute << ":"
           << (int)gps_info.time.nSecond<< "," << latitude << "," << longitude << "," << gps_info.speed << "," << gps_info.direction << "," << gps_info.altitude << "," << (int)gps_info.mileage
           << "," << ToString(gps_info.driverName,12) << "," << ToString(gps_info.driverId,18) << "," << (int)gps_info.valid << "," << ToHexStr(gps_info.state,8);
    }
    else
    {
        os << std::string(car_info.vendorId) << "," << std::string(car_info.factoryId) << "," << std::string(car_info.deviceId) << "," << std::string(car_info.commNo);
    }
    os << "\r\n";
    writeToFile(os.str());
    os.str("");
}


int STGPSReciever::RecvDatahand(int sessionId, const char *buf, int len, void *userdata)
{
    STGPSReciever* that = reinterpret_cast<STGPSReciever*>(userdata);
    return that->OnRecvDatahandler(sessionId,buf,len,userdata);
}


int STGPSReciever::OnRecvDatahandler(int sessionId, const char *buf, int len, void *userdata)
{
    Head head;
    GPSInfo gps_info;
    CarInfo car_info;
    std::size_t ret;
    std::ostringstream os;
    int buflen = len;
    STGPS_ParseHead(const_cast<char*>(buf),&buflen,&head);
//    LOG_INFO << "head.vehileId = " << ToString(head.vehileId,8);
//    LOG_INFO << "head.vehileColor = " << head.vehileColor;
//    LOG_INFO << "head.packLen = " << head.packLen;
//    LOG_INFO << "head.nonsense = " << head.nonsense;
//    LOG_INFO << "head.check = " << head.check;
//    LOG_INFO << "head.cityCode = " << head.cityCode;
//    LOG_INFO << "head.cmdId = " << head.cmdId;
//    LOG_INFO << "head.cmdLength = " << head.cmdLength;
    int leave = len - buflen;
    if(head.packLen > len)
    {
        LOG_WARN << "invalid buf,size too small";
        return 0;
    }
    if(head.check != 0x0001 || head.cityCode > 'W' || head.cityCode < 'A')
    {
        LOG_WARN << " MSG ERROR!";
        return head.packLen;
    }
    if(head.cmdLength > leave)
    {
        LOG_WARN << "Package not complete,cmdLength = " << head.cmdLength << " > buff len = " << len - buflen;
        return 0;
    }
    switch (head.cmdId)
    {
        case 1:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        {
            if(CCMS_RETURN_OK != STGPS_ParseGPSInfo(const_cast<char*>(buf + buflen),&leave,head.cmdLength,&gps_info))
            {
                ret = 0;
            }
            else
            {
                FormatMsg(head,gps_info,car_info,true);
                ret = leave + buflen;
            }
        }
            break;
        case 2:
        {
            if(CCMS_RETURN_OK != STGPS_ParseCarBasicInfo(const_cast<char*>(buf + buflen),&leave,&car_info))
            {
                ret = 0;
            }
            else
            {
                FormatMsg(head,gps_info,car_info,false);
                ret = leave + buflen;
            }
        }
            break;
        default:
            break;
    }
    return ret;
}

int STGPSReciever::Connecthand(int sessionId, int status, const char* ip, unsigned short port, void* userdata)
{
    STGPSReciever* that = reinterpret_cast<STGPSReciever*>(userdata);
    return that->OnConnecthandler(sessionId,status,ip,port,userdata);
}

int STGPSReciever::OnConnecthandler(int sessionId, int status, const char* ip, unsigned short port, void* userdata)
{
    LOG_INFO << "onConnectCallback   sessionId => "<< sessionId<<"\tip => " << ip << "\tport => " << port <<"\tstatus => "<< status;
    if(!status)
    {
        maps[ip] = std::make_pair(sessionId,status);
        muduo::MutexLockGuard lock(mtx);
        ready = true;
        con.notify();
    }
    else
    {
        LOG_INFO << "IP => " << ip << "\tPort => " << port << "\tDisconnected";
    }
}

void STGPSReciever::sendMsg()
{
    while(sending)
    {
        {
            muduo::MutexLockGuard lock(mtx);
            if(!ready)
            {
                con.wait();
            }
        }
        std::map<std::string,std::pair<int,int> >::iterator it  = maps.begin();
        if(!it->second.second)
        {
            std::string str("siatxdata\r\n");
            BATNetSDKRaw_Send(handler,it->second.first,str.c_str(),str.length());
            ready = false;
            sending = false;
        }
    }
}











































