#pragma once
#include "CNetServer.h"
class CGroup
{
private:
	enum en_GROUP_JOB_PACKET
	{
		ENTER_GROUP = 1,
		MOVE_GROUP,
		RELEASE_SESSION
	};

	typedef struct _jobHeader
	{
		en_GROUP_JOB_PACKET type;
		CNetServer::SESSIONID sessionID;
	} JobHeader;
private:
	HANDLE m_groupThreadHandle;
	unsigned int m_groupThreadId;
	
	DWORD prevTime;
	DWORD frame;
	DWORD frameInterval;

	DWORD oldTick;
	DWORD tmpFrame;
	DWORD curFrame;

	CQueue<CPacket*> groupJobQueue;

	unordered_map<CNetServer::SESSIONID, CNetServer::Session*> umSession;

	bool bShutdown;
	bool bSleep;
private:
	static unsigned WINAPI GroupProc(PVOID param);
	
	void jobEnterGroup(CPacket* packet, CNetServer::Session* session);
	void jobMoveGroup(CPacket* packet, CNetServer::Session* session);
	void jobReleaseGroup(CPacket* packet, CNetServer::Session* session);

	void FrameCheck();
	void GroupJobProc();
public:
	CGroup();

	virtual ~CGroup();

	void InitializeGroup(DWORD frame);
	void SuspendGroup();
	void ResumeGroup();

	virtual void OnRecv(CPacket* packet, CNetServer::SESSIONID sessionID) = 0;
	virtual void OnUpdate() = 0;
	virtual void OnEnter(CNetServer::SESSIONID sessionID, void* key) = 0;
	virtual void OnLeave(CNetServer::SESSIONID sessionID) = 0;
	virtual void OnRelease(CNetServer::SESSIONID sessionID, void* key) = 0;

	bool SendPacket(CNetServer::SESSIONID sessionID, CPacket* packet);

	void ReleaseSessionGroup(CNetServer::SESSIONID sessionID);
	void EnterGroup(CNetServer::SESSIONID sessionID, void* key = nullptr, CGroup* dstGroup = nullptr);

	void EnterPlayer();
	void LeavePlayer();

	DWORD GetFrame() { return curFrame; }
private:
	friend class CNetServer;
};