//
// Created by wangqi on 17-6-16.
//

#include "STGPSReciever.h"

int main()
{
    STGPSReciever server("../../data/",0,"");
    server.start("172.20.104.184",19999);
    pause();
    return 0;
}