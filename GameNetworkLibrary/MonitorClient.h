#pragma once
#include "CLanClient.h"
#include "CMonitorCounter.h"

class CNetServer;

class MonitorClient : public CLanClient
{
private:
	#pragma pack(push,1)
	typedef struct st_clientHeader
	{
		WORD type;
	} ClientHeader;
	#pragma pack(pop)

	DWORD m_prevTime;

	HANDLE m_hUpdateThread;
	unsigned int m_updateThreadID;

	CNetServer* monitorServer;

	CMonitorCounter m_monitor;

	BYTE m_serverNo;
	bool m_bShutdown;
private:
	static unsigned WINAPI UpdateProc(PVOID params);
public:
	MonitorClient();
	~MonitorClient();

	void MonitorStart(CNetServer* server);
	//void MonitorStart();

	virtual void OnRecv(CPacket* packet);
};