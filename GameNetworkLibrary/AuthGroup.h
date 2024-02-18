#pragma once
#include "GameServer.h"

class AuthGroup : public CGroup
{
private:
	GameServer::Info* m_pArrInfo[10000];
	int curAuthNum = 0;
public:
	virtual void OnRecv(CPacket* packet, CNetServer::SESSIONID sessionID);
	virtual void OnUpdate()
	{

	};

	virtual void OnEnter(CNetServer::SESSIONID sessionID, void* key);

	virtual void OnLeave(CNetServer::SESSIONID sessionID);

	virtual void OnRelease(CNetServer::SESSIONID sessionID, void* key);

	void loginProc(CPacket* packet, CNetServer::SESSIONID sessionID);

	void MakeLoginResPacket(CPacket* packet, BYTE status, __int64 accountNo);

	int GetPlayerNum() { return curAuthNum; }
};