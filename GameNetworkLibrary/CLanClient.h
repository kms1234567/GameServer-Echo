#pragma once
#include <WinSock2.h>
#include <process.h>
#include "CRingBuffer.h"
#include "LockFreeQueue.h"
#include "CPacket.h"

class CLanClient
{
private:
	enum enNetwork
	{
		m_kWSABUFlen = 200,
	};

	enum enPQCS_JOB
	{
		THREAD_EXIT = CRingBuffer::MAX_BUFFER_SIZE + 1,
		SENDPOST_ZERO = CRingBuffer::MAX_BUFFER_SIZE + 2,
	};

#pragma pack(push,1)
	typedef struct st_lanHeader
	{
		short len;
	} LanHeader;
#pragma pack(pop)

	typedef struct st_myOverlapped
	{
		WSAOVERLAPPED overlapped;
		bool isRecv;
		int packetNum;
	} MYOVERLAPPED;

	SOCKET m_clientSocket;
	CRingBuffer m_recvQ;
	CQueue<CPacket*> m_sendQ;
	SOCKADDR_IN m_serverAddr;

	MYOVERLAPPED m_sendOverlapped;
	MYOVERLAPPED m_recvOverlapped;
	bool m_sendFlag;

	HANDLE m_hCompletionPort;

	int m_createThreadNum;
	HANDLE* m_hWorkerThreads;
	unsigned int* m_workerThreadIDs;

	bool m_bShutdown;
private:
	void RecvProc(DWORD recvBytes);
	void SendProc(int sendPacketNum);
	void RecvPost();
	void SendPost();

	static unsigned WINAPI WorkProc(PVOID params);
public:
	CLanClient();
	virtual ~CLanClient();

	bool Start(const WCHAR* ip, u_short port, DWORD m_createThreadNum, DWORD runThreadNum);
	void Stop();

	void SendPacket(CPacket* packet);

	virtual void OnRecv(CPacket* packet) = 0;
};