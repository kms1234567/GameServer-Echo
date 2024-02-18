#include "EchoGroup.h"
#include "CommonProtocol.h"

void EchoGroup::OnRecv(CPacket* packet, CNetServer::SESSIONID sessionID)
{
	WORD type;
	*packet >> type;

	if (type == en_PACKET_CS_GAME_REQ_ECHO)
	{
		GameServer::User* user = m_pArrUser[CNetServer::GetSessionIdx(sessionID)];

		INT64 accountNo;
		long long sendTick;

		*packet >> accountNo >> sendTick;

		CPacket* echoPacket = CPacket::Alloc();

		*echoPacket << (WORD)en_PACKET_CS_GAME_RES_ECHO << accountNo << sendTick;

		SendPacket(sessionID, echoPacket);
	}
};

void EchoGroup::OnEnter(CNetServer::SESSIONID sessionID, void* key)
{
	m_pArrUser[CNetServer::GetSessionIdx(sessionID)] = (GameServer::User*)key;
	key = nullptr;
	curUserNum++;
	EnterPlayer();
};

void EchoGroup::OnRelease(CNetServer::SESSIONID sessionID, void* key)
{
	GameServer::User* user = m_pArrUser[CNetServer::GetSessionIdx(sessionID)];

	GameServer::m_infoPool.Free(user->info);
	GameServer::m_userPool.Free(user);

	curUserNum--;
	LeavePlayer();
}