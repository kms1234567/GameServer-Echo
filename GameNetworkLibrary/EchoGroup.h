#pragma once
#include "GameServer.h"

class EchoGroup : public CGroup
{
private:
	GameServer::User* m_pArrUser[10000];

	int curUserNum = 0;
public:
	virtual void OnRecv(CPacket* packet, CNetServer::SESSIONID sessionID);

	virtual void OnUpdate()
	{

	};

	virtual void OnEnter(CNetServer::SESSIONID sessionID, void* key);

	virtual void OnLeave(CNetServer::SESSIONID sessionID)
	{

	};

	virtual void OnRelease(CNetServer::SESSIONID sessionID, void* key);

	int GetPlayerNum() { return curUserNum; }
};