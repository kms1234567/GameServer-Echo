#include "AuthGroup.h"
#include "CommonProtocol.h"

void AuthGroup::OnRecv(CPacket* packet, CNetServer::SESSIONID sessionID)
{
	WORD type;
	*packet >> type;

	switch (type)
	{
	case en_PACKET_CS_GAME_REQ_LOGIN:
		loginProc(packet, sessionID);
		break;
	}
};

void AuthGroup::OnEnter(CNetServer::SESSIONID sessionID, void* key)
{
	curAuthNum++;
	m_pArrInfo[CNetServer::GetSessionIdx(sessionID)] = (GameServer::Info*)key;
};

void AuthGroup::OnLeave(CNetServer::SESSIONID sessionID)
{
	curAuthNum--;

	GameServer::m_infoPool.Free(m_pArrInfo[CNetServer::GetSessionIdx(sessionID)]);
};

void AuthGroup::OnRelease(CNetServer::SESSIONID sessionID, void* key)
{
	if (key != nullptr) GameServer::m_infoPool.Free((GameServer::Info*)key);
	curAuthNum--;
}

void AuthGroup::loginProc(CPacket* packet, CNetServer::SESSIONID sessionID)
{
	INT64 accountNo;

	*packet >> accountNo;

	GameServer::User* user = GameServer::m_userPool.Alloc();
	packet->GetData((char*)user->sessionKey, sizeof(user->sessionKey));

	user->info = m_pArrInfo[CNetServer::GetSessionIdx(sessionID)];
	user->accountNo = accountNo;

	EnterGroup(sessionID, user, (CGroup*)GameServer::gameServer->echoGroup);

	CPacket* loginPacket = CPacket::Alloc();
	MakeLoginResPacket(loginPacket, true, accountNo);
	SendPacket(sessionID, loginPacket);
}

void AuthGroup::MakeLoginResPacket(CPacket* packet, BYTE status, __int64 accountNo)
{
	*packet << (WORD)en_PACKET_CS_GAME_RES_LOGIN << status << accountNo;
}