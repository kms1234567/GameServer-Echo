#include "GameServer.h"
#include "AuthGroup.h"
#include "EchoGroup.h"

MemoryPoolTLS<GameServer::Info> GameServer::m_infoPool;
MemoryPoolTLS<GameServer::User> GameServer::m_userPool;
GameServer* GameServer::gameServer;

GameServer::GameServer()
{
	gameServer = this;

	authGroup = new AuthGroup;
	authGroup->InitializeGroup(25);

	echoGroup = new EchoGroup;
	echoGroup->InitializeGroup(50);
}

GameServer::~GameServer()
{
	delete authGroup;
	delete echoGroup;
}

bool GameServer::OnConnectionRequest(u_long ip, u_short port)
{
	return true;
}

void GameServer::OnClientJoin(SESSIONID sessionID, WCHAR* ip, u_short port)
{
	Info* info = m_infoPool.Alloc();
	info->sessionID = sessionID;
	info->port = port;
	memcpy_s(info->ip, sizeof(info->ip), ip, sizeof(ip));

	authGroup->EnterGroup(sessionID, info);
}

void GameServer::OnClientLeave(SESSIONID sessionID)
{
}

void GameServer::OnRecv(CPacket* packet, SESSIONID sessionID)
{	
}

void GameServer::OnError(int errorcode, PWCHAR msg)
{	
}

void GameServer::OnTimeout()
{
}

int GameServer::GetAuthGroupPlayer() { return authGroup->GetPlayerNum(); }
int GameServer::GetEchoGroupPlayer() { return echoGroup->GetPlayerNum(); }
int GameServer::GetAuthGroupFrame() { return authGroup->GetFrame(); }
int GameServer::GetEchoGroupFrame() { return echoGroup->GetFrame(); }