#pragma once
#include "CPacket.h"
#include "MonitorProtocol.h"

void MakeMonitorLoginPacket(CPacket* packet, int serverNo)
{
	*packet << (WORD)en_PACKET_SS_MONITOR_LOGIN << serverNo;
}

void MakeMonitorDataPacket(CPacket* packet, BYTE dataType, int dataValue, int timeStamp)
{
	*packet << (WORD)en_PACKET_SS_MONITOR_DATA_UPDATE << dataType << dataValue << timeStamp;
}