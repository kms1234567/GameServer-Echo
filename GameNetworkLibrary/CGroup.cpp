#pragma comment(lib, "Synchronization.lib")

#include "CGroup.h"

unsigned WINAPI CGroup::GroupProc(PVOID param)
{
	CGroup* group = (CGroup*)param;
	while (!group->bShutdown)
	{
		if (group->bSleep)
		{
			WaitOnAddress(&group->bSleep, &group->bSleep, sizeof(group->bSleep), INFINITE);
			continue;
		}
		
		// Àâ Ã³¸®
		group->GroupJobProc();

		for (auto &sessionIter : group->umSession)
		{
			CNetServer::Session* session = sessionIter.second;

			CPacket* packet;

			while (session->completePacketQ.Dequeue(packet))
			{
				group->OnRecv(packet, session->sessionID);

				CPacket::Free(packet);
			}
		}

		group->OnUpdate();

		group->FrameCheck();
	}

	return 0;
}

CGroup::CGroup()
{
	bShutdown = false;
	bSleep = true;

	m_groupThreadHandle = (HANDLE)_beginthreadex(nullptr, 0, GroupProc, (void*)this, 0, &m_groupThreadId);
}

CGroup::~CGroup()
{
	bShutdown = true;

	WakeByAddressSingle(&bSleep);
}

void CGroup::InitializeGroup(DWORD frame)
{
	curFrame = 0;
	tmpFrame = 0;
	oldTick = timeGetTime();

	this->frame = frame;

	if (frame != 0) frameInterval = (1000 / frame);
	else frameInterval = 0;

	prevTime = timeGetTime();

	bSleep = false;
	WakeByAddressSingle(&bSleep);
}

void CGroup::SuspendGroup()
{
	bSleep = true;
}

void CGroup::ResumeGroup()
{
	bSleep = false;
	WakeByAddressSingle(&bSleep);
}

void CGroup::GroupJobProc()
{
	CPacket* packet;
	JobHeader header;

	while(groupJobQueue.Dequeue(packet))
	{
		packet->GetData((char*)&header, sizeof(header));

		CNetServer::Session* session = CNetServer::g_server->GetSession(header.sessionID);

		switch (header.type)
		{
		case en_GROUP_JOB_PACKET::ENTER_GROUP:
			jobEnterGroup(packet, session);
			break;
		case en_GROUP_JOB_PACKET::MOVE_GROUP:
			jobMoveGroup(packet, session);
			break;
		case en_GROUP_JOB_PACKET::RELEASE_SESSION:
			jobReleaseGroup(packet, session);
			break;
		}

		CPacket::Free(packet);
	}
}

void CGroup::jobEnterGroup(CPacket* packet, CNetServer::Session* session)
{
	session->group = this;
	umSession[session->sessionID] = session;

	OnEnter(session->sessionID, session->completionKey);

	if (InterlockedDecrement16((short*)&session->ioCntReleaseFlag.ioCnt) == 0)
		CNetServer::g_server->Release(session->sessionID);
}

void CGroup::jobMoveGroup(CPacket* packet, CNetServer::Session* session)
{
	CGroup* dstGroup = nullptr;
	packet->GetData((char*)&dstGroup, sizeof(dstGroup));

	umSession.erase(session->sessionID);
	OnLeave(session->sessionID);

	CPacket* enterJobpacket = CPacket::Alloc();
	enterJobpacket->AddRef();
	
	JobHeader header;
	header.sessionID = session->sessionID;
	header.type = en_GROUP_JOB_PACKET::ENTER_GROUP;

	enterJobpacket->PutData((char*)&header, sizeof(header));

	dstGroup->groupJobQueue.Enqueue(enterJobpacket);
}

void CGroup::jobReleaseGroup(CPacket* packet, CNetServer::Session* session)
{
	umSession.erase(session->sessionID);
	OnRelease(session->sessionID, session->completionKey);

	PostQueuedCompletionStatus(CNetServer::g_server->m_hCompletionPort, 
		CNetServer::en_PQCS_PACKET::SESSION_RELEASE, (ULONG_PTR)session, nullptr);
}

void CGroup::FrameCheck()
{
	DWORD curTime = timeGetTime();

	int diffTime = curTime - prevTime;
	int sleepTime = (frameInterval - diffTime);

	if (sleepTime > 0)
	{
		Sleep(sleepTime);
	}
	
	tmpFrame++;
	prevTime += frameInterval;

	if (curTime - oldTick >= 1000)
	{
		curFrame = tmpFrame;
		tmpFrame = 0;
		oldTick += 1000;
	}
}

void CGroup::ReleaseSessionGroup(CNetServer::SESSIONID sessionID)
{
	CPacket* packet = CPacket::Alloc();
	packet->AddRef();

	JobHeader header;
	header.sessionID = sessionID;
	header.type = en_GROUP_JOB_PACKET::RELEASE_SESSION;

	packet->PutData((char*)&header, sizeof(header));

	groupJobQueue.Enqueue(packet);
}

void CGroup::EnterGroup(CNetServer::SESSIONID sessionID, void* key, CGroup* dstGroup)
{
	CNetServer::Session* session = CNetServer::g_server->GetSession(sessionID);

	if ((InterlockedIncrement((long*)&session->ioCntReleaseFlag.ioCnt) >> 16) == true)
	{
		if (InterlockedDecrement16((short*)&session->ioCntReleaseFlag.ioCnt) == 0)
		{
			CNetServer::g_server->Release(session->sessionID);
		}
		return;
	}

	if (session->sessionID != sessionID)
	{
		if (InterlockedDecrement16((short*)&session->ioCntReleaseFlag.ioCnt) == 0)
		{
			CNetServer::g_server->Release(session->sessionID);
		}
		return;
	}

	CPacket* packet = CPacket::Alloc();
	packet->AddRef();

	if (session->group == nullptr)
	{
		session->completionKey = key;
		session->group = this;

		JobHeader header;
		header.sessionID = sessionID;
		header.type = en_GROUP_JOB_PACKET::ENTER_GROUP;

		packet->PutData((char*)&header, sizeof(header));
	}
	else
	{
		session->completionKey = key;
		session->group = dstGroup;

		JobHeader header;
		header.sessionID = sessionID;
		header.type = en_GROUP_JOB_PACKET::MOVE_GROUP;

		packet->PutData((char*)&header, sizeof(header));
		packet->PutData((char*)&dstGroup, sizeof(dstGroup));
	}

	groupJobQueue.Enqueue(packet);
}

bool CGroup::SendPacket(CNetServer::SESSIONID sessionID, CPacket* packet)
{
	return CNetServer::g_server->SendPacket(sessionID, packet);
}

void CGroup::EnterPlayer()
{
	CNetServer::g_server->EnterPlayer();
}

void CGroup::LeavePlayer()
{
	CNetServer::g_server->LeavePlayer();
}