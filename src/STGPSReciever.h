//
// Created by wangqi on 17-6-15.
//

#ifndef PROROADVEHICLE_PROROADVEHICLESERVER_H
#define PROROADVEHICLE_PROROADVEHICLESERVER_H

#include <string>
#include "base/Timestamp.h"
#include "base/TimeZone.h"
#include <fstream>
#include "Common.h"
#include <map>
#include <vector>
#include "base/Mutex.h"
#include "base/Condition.h"
#include "base/Thread.h"
#include "utility"
#include "STGPSProtocol.h"


class STGPSReciever {
public:
    STGPSReciever(const std::string &log_path,int nDevType, const std::string& strDevSN);
    ~STGPSReciever();
    int start(const std::string& ip,uint16_t port);

private:
    int OnRecvDatahandler(int sessionId, const char *buf, int len, void *userdata);
    static int RecvDatahand(int sessionId, const char *buf, int len, void *userdata);
    int OnConnecthandler(int sessionId, int status, const char* ip, unsigned short port, void* userdata);
    static int Connecthand(int sessionId, int status, const char* ip, unsigned short port, void* userdata);
    void writeToFile(std::string msg);
    void FormatMsg(Head& head,GPSInfo& gps_info,CarInfo& car_info,bool is_gps_info);
    void sendMsg();

private:
    int handler;
    struct tm startTime;
    const std::string& _log_path;
    std::ofstream file;
    muduo::MutexLock mtx;
    muduo::Condition con;
    bool ready;
    bool sending;
    std::map<std::string,std::pair<int,int> > maps;
    muduo::Thread sendMsg_thread;
};


#endif //PROROADVEHICLE_PROROADVEHICLESERVER_H
