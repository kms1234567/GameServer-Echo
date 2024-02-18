#pragma once
#include "CGroup.h"

class AuthGroup;
class EchoGroup;

class GameServer : public CNetServer
{
private:
	static GameServer* gameServer;

	AuthGroup* authGroup;
	EchoGroup* echoGroup;
public:
	GameServer();
	~GameServer();
private:
	typedef struct _stUserInfo
	{
		SESSIONID sessionID;
		WCHAR ip[16];
		u_short port;
	} Info;

	typedef struct _stUser
	{
		Info* info;
		INT64 accountNo;
		char sessionKey[64];
	} User;

	static MemoryPoolTLS<Info> m_infoPool;
	static MemoryPoolTLS<User> m_userPool;
public:
	int GetAuthGroupPlayer();
	int GetEchoGroupPlayer();
	int GetAuthGroupFrame(); 
	int GetEchoGroupFrame();

	virtual bool OnConnectionRequest(u_long ip, u_short port);
	virtual void OnClientJoin(SESSIONID sessionID, WCHAR* ip, u_short port);
	virtual void OnClientLeave(SESSIONID sessionID);
	virtual void OnRecv(CPacket* packet, SESSIONID sessionID);
	virtual void OnError(int errorcode, PWCHAR msg);
	virtual void OnTimeout();

	//void MonitorData();
private:
	friend class AuthGroup;
	friend class EchoGroup;
};