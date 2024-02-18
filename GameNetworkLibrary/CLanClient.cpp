#include "CLanClient.h"
#include <WS2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

CLanClient::CLanClient()
{
	m_bShutdown = false;
	m_sendFlag = false;
	
	memset(&m_sendOverlapped, 0, sizeof(m_sendOverlapped));
	memset(&m_recvOverlapped, 0, sizeof(m_recvOverlapped));

	m_sendOverlapped.isRecv = false;
	m_recvOverlapped.isRecv = true;
}

CLanClient::~CLanClient()
{

}

bool CLanClient::Start(const WCHAR* ip, u_short port, DWORD m_createThreadNum, DWORD runThreadNum)
{
	int ret_connect;

	WSAData wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, runThreadNum);

	m_clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	InetPton(AF_INET, ip, &m_serverAddr.sin_addr);
	m_serverAddr.sin_family = AF_INET;
	m_serverAddr.sin_port = htons(port);

	ret_connect = connect(m_clientSocket, (SOCKADDR*)&m_serverAddr, sizeof(m_serverAddr));
	while (ret_connect)
	{
		printf("client connect error : %d\n", WSAGetLastError());
		Sleep(2000);
		ret_connect = connect(m_clientSocket, (SOCKADDR*)&m_serverAddr, sizeof(m_serverAddr));
	}

	CreateIoCompletionPort((HANDLE)m_clientSocket, m_hCompletionPort, NULL, NULL);

	m_createThreadNum = m_createThreadNum;

	m_hWorkerThreads = (HANDLE*)malloc(sizeof(HANDLE) * m_createThreadNum);
	m_workerThreadIDs = (unsigned int*)malloc(sizeof(unsigned int) * m_createThreadNum);

	for (int idx = 0; idx < m_createThreadNum; ++idx)
	{
		m_hWorkerThreads[idx] = (HANDLE)_beginthreadex(nullptr, 0, WorkProc, 
			this, 0, &m_workerThreadIDs[idx]);
	}

	printf("Server Up\n");

	return true;
}

void CLanClient::Stop()
{
	PostQueuedCompletionStatus(m_hCompletionPort, CLanClient::THREAD_EXIT, 0, 0);

	closesocket(m_clientSocket);

	WaitForMultipleObjects(m_createThreadNum, m_hWorkerThreads, TRUE, INFINITE);
	for (int idx = 0; idx < m_createThreadNum; ++idx)
	{
		CloseHandle(m_hWorkerThreads[idx]);
	}

	CloseHandle(m_hCompletionPort);

	WSACleanup();
}

unsigned WINAPI CLanClient::WorkProc(PVOID params)
{
	CLanClient* client = (CLanClient*)params;

	BOOL ret_gqcs;
	MYOVERLAPPED* myOverlapped;

	DWORD numBytes;
	ULONG_PTR completionKey;
	LPOVERLAPPED overlapped;

	while (!client->m_bShutdown)
	{
		numBytes = 0;

		ret_gqcs = GetQueuedCompletionStatus(client->m_hCompletionPort, &numBytes, &completionKey, &overlapped, INFINITE);
		
		if (numBytes > CRingBuffer::MAX_BUFFER_SIZE)
		{
			switch (numBytes)
			{
			case enPQCS_JOB::SENDPOST_ZERO:
			{
				client->SendPost();
			}
			}
			continue;
		}

		if (ret_gqcs == TRUE)
		{
			myOverlapped = (MYOVERLAPPED*)overlapped;
			if (myOverlapped->isRecv)
			{
				client->RecvProc(numBytes);
			}
			else
			{
				client->SendProc(myOverlapped->packetNum);
			}
		}
	}

	return 0;
}

void CLanClient::RecvProc(DWORD recvBytes)
{
	m_recvQ.MoveRear(recvBytes);

	while (true)
	{
		LanHeader header;
		int useSize = m_recvQ.GetUseSize();

		if (useSize < sizeof(header)) break;
		m_recvQ.Peek((char*)&header, sizeof(header));

		if (useSize < sizeof(header) + header.len) break;

		m_recvQ.MoveFront(sizeof(header));

		CPacket* packet = CPacket::Alloc();
		packet->AddRef();

		packet->MoveWritePos(m_recvQ.Dequeue(packet->GetBufferPtr(), header.len));

		// recv 메시지 수 증가 (모니터링)

		OnRecv(packet);

		CPacket::Free(packet);
	}

	RecvPost();
}

void CLanClient::SendProc(int sendPacketNum)
{
	for (int num = 0; num < sendPacketNum; ++num)
	{
		CPacket* freePacket;
		m_sendQ.Dequeue(freePacket);
		CPacket::Free(freePacket);
	}

	// 메시지 수(모니터링) 증가

	InterlockedExchange8((char*)&m_sendFlag, false);

	SendPost();
}

void CLanClient::RecvPost()
{
	int ret_recv;
	DWORD flag = 0;

	WSABUF wsaBuf[2];
	int freeSize = m_recvQ.GetFreeSize();

	if (freeSize == 0)
	{

	}
	else
	{
		wsaBuf[0].buf = m_recvQ.GetRearBufferPtr();
		wsaBuf[0].len = m_recvQ.DirectEnqueueSize();
		wsaBuf[1].buf = m_recvQ.GetBufferPtr();
		wsaBuf[1].len = 0;
		if (freeSize > wsaBuf[0].len)
			wsaBuf[1].len = freeSize - wsaBuf[0].len;

		memset(&m_recvOverlapped.overlapped, 0, sizeof(m_recvOverlapped.overlapped));
		ret_recv = WSARecv(m_clientSocket, wsaBuf, 2, NULL, &flag, &m_recvOverlapped.overlapped, NULL);
	}
}

void CLanClient::SendPost()
{
	if (m_sendQ.GetUseSize() == 0) return;
	if (InterlockedExchange8((char*)&m_sendFlag, true) == true) return;
	
	int ret_send;

	WSABUF wsaBuf[enNetwork::m_kWSABUFlen];
	int packetNums = m_sendQ.GetUseSize();
	if (packetNums != 0)
	{
		CPacket* frontPacket[m_kWSABUFlen];
		if (packetNums >= m_kWSABUFlen) packetNums = m_kWSABUFlen;

		m_sendQ.Peek(frontPacket, packetNums);

		for (int num = 0; num < packetNums; ++num)
		{
			wsaBuf[num].buf = frontPacket[num]->GetHeaderPtr();
			wsaBuf[num].len = frontPacket[num]->GetHeaderSize() + 
							  frontPacket[num]->GetDataSize();
		}

		memset(&m_sendOverlapped.overlapped, 0, sizeof(m_sendOverlapped.overlapped));
		m_sendOverlapped.packetNum = packetNums;
		ret_send = WSASend(m_clientSocket, wsaBuf, (DWORD)packetNums, NULL, 0,
			&m_sendOverlapped.overlapped, NULL);
	}
	else
	{
		InterlockedExchange8((char*)&m_sendFlag, false);
		PostQueuedCompletionStatus(m_hCompletionPort, enPQCS_JOB::SENDPOST_ZERO, NULL, NULL);
	}
}

void CLanClient::SendPacket(CPacket* packet)
{
	packet->AddRef();

	LanHeader header;
	header.len = packet->GetDataSize();

	packet->PutHeader((char*)&header, sizeof(header));
	m_sendQ.Enqueue(packet);

	SendPost();
}