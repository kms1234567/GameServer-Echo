#pragma once
#include <WinSock2.h>
#include <process.h>
#include <unordered_map>
#include "CRingBuffer.h"
#include "CPacket.h"
#include "LockFreeStack.h"
#include "LockFreeQueue.h"

class CGroup;

class CNetServer
{
public:
	typedef unsigned long long SESSIONID;
	const unsigned long long MAXSESSIONID = 0x0000FFFFFFFFFFFF;

private:
	static const int m_kWSABUFlen = 100;
	const int m_kMaxSessionNums = 10000;
	const int m_kMaxPacketNums = 1000;
	const int m_kMaxMessageLen = 300;

	enum en_PQCS_PACKET {
		THREAD_EXIT = CRingBuffer::MAX_BUFFER_SIZE + 1,
		SENDPOST_ZERO,
		SESSION_RELEASE,
		SEND_PACKET,
	};
private:
	typedef struct st_myoverlapped
	{
		WSAOVERLAPPED overlapped;
		bool isRecv;
		int packetNum;
	} MYOVERLAPPED;
protected:
	class Session
	{
	public:
		SOCKET socket;
		SESSIONID sessionID;
		CRingBuffer recvQ;
		CQueue<CPacket*> sendQ;
		MYOVERLAPPED recvOverlapped;
		MYOVERLAPPED sendOverlapped;
		SOCKADDR_IN addr;

		struct st_ioCntReleaseFlag
		{
			alignas(4)
				short ioCnt = 0;
			short releaseFlag = true;
		} ioCntReleaseFlag;

		CGroup* group;
		CQueue<CPacket*> completePacketQ;
		void* completionKey;

		alignas(64)
		bool sendFlag;
	};
private:
#pragma pack(push,1)
	typedef struct _header
	{
		unsigned char code;
		unsigned short len;
		unsigned char randKey;
	} NetHeader, NETHEADER;
#pragma pack(pop)
private:
	static CNetServer* g_server;

	unsigned char m_packetCode;
	unsigned char m_packetKey;

	HANDLE m_hCompletionPort;
	HANDLE* m_hThreads;

	unsigned int* m_pThreadIDs;
	int m_createThreadNum = 0;

	Session* m_pArrSession;
	SOCKET m_listenSocket;
	SESSIONID m_baseSessionID = 0;
	CStack<SESSIONID> m_stkFreeIdx;

	unsigned int m_timeoutInterval;
	// 모니터링
	int m_limitPlayer;
	int m_curPlayer = 0;
	int m_acceptTotalCnt = 0;
	int m_acceptCnt = 0;
	int m_sessionCnt = 0;
	int m_recvMsgCnt = 0;
	int m_sendMsgCnt = 0;
	int m_ioPendingCnt = 0;
public:
	static SESSIONID GetSessionIdx(SESSIONID sessionIdx)
	{
		return (sessionIdx & 0x000000000000FFFF);
	}

	int GetSessionNums() { return m_sessionCnt; }
	int GetPlayerNums() { return m_curPlayer; }

	int GetAcceptCnt() { return InterlockedExchange((long*)&m_acceptCnt, 0); }
	int GetAcceptTotalCnt() { return m_acceptTotalCnt; }
	int GetRecvMsgCnt() { return InterlockedExchange((long*)&m_recvMsgCnt, 0); }
	int GetSendMsgCnt() { return InterlockedExchange((long*)&m_sendMsgCnt, 0); }
	int GetIOPendingCnt() { return InterlockedExchange((long*)&m_ioPendingCnt, 0); }
protected:
	Session* GetSession(SESSIONID id);
private:
	static unsigned WINAPI AcceptProc(PVOID param);
	static unsigned WINAPI WorkProc(PVOID param);
	static unsigned WINAPI TimeOutProc(PVOID param);

	void InitialSession(Session* session);

	void SendPost(Session* session);
	void RecvPost(Session* session);
	void RecvProc(Session* session, DWORD recvBytes);
	void SendProc(Session* session, int sendPacketNum);

	void Release(SESSIONID);
	void ReleaseSession(Session*);
	void ClientJoin(Session* session);

	unsigned char GetRandkey();
	void SendPostLogic(SESSIONID sessionID);

	void EncodePacket(CPacket* packet, unsigned char randKey);
	bool DecodePacket(CPacket* packet, unsigned char randKey);

	void EnterPlayer() { InterlockedIncrement((long*)&m_curPlayer); }
	void LeavePlayer() { InterlockedDecrement((long*)&m_curPlayer); }

	SESSIONID GetSessionID(SESSIONID sessionID)
	{
		return sessionID >> 16;
	}

public:
	CNetServer();
	virtual ~CNetServer();

	void ServerInspect();
	bool Start(const WCHAR* ip, u_short port, DWORD m_createThreadNum, DWORD runThreadNum, bool isNagle, bool isTimeOut, unsigned int timeoutInterval, int maxConnectionNum);
	void Stop();

	bool Disconnect(SESSIONID sessionID);
	bool SendPacket(SESSIONID sessionID, CPacket* packet);
	bool SendPacketBroadcast(list<SESSIONID>* sessionidList, CPacket* packet);
	
	void SendPacketProc(SESSIONID sessionID, CPacket* packet);

	virtual bool OnConnectionRequest(u_long ip, u_short port) = 0;
	virtual void OnClientJoin(SESSIONID sessionID, WCHAR* ip, u_short port) = 0;
	virtual void OnClientLeave(SESSIONID sessionID) = 0;
	virtual void OnRecv(CPacket* packet, SESSIONID sessionID) = 0;
	virtual void OnError(int errorcode, PWCHAR msg) = 0;
	virtual void OnTimeout() = 0;
private:
	friend class CGroup;
};