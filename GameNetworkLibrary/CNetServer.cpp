#include "CGroup.h"
#include <WS2tcpip.h>
#include "Parser.h"

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "winmm.lib")

int CPacket::allocPacketNum = 0;
int CPacket::freePacketNum = 0;

MemoryPoolTLS<CPacket> CPacket::m_pool;

unsigned long long CQueue<CPacket*>::m_staticEndValue = -1;

template <typename T>
unsigned long long CQueue<T>::m_staticEndValue = -1;

CNetServer* CNetServer::g_server = nullptr;

CNetServer::CNetServer()
{
	g_server = this;

	timeBeginPeriod(1);

	srand(time(NULL));

	m_pArrSession = new Session[m_kMaxSessionNums];

	for (int idx = m_kMaxSessionNums - 1; idx >= 0; --idx)
	{
		m_stkFreeIdx.push(idx);
	}

	Parser parser("config.ini");
	parser.Load("SERVER");

	parser.GetValue("PACKET_CODE", (char*)&m_packetCode, sizeof(m_packetCode));
	parser.GetValue("PACKET_KEY", (char*)&m_packetKey, sizeof(m_packetKey));
}

CNetServer::~CNetServer()
{
	delete[] m_pArrSession;
}

bool CNetServer::Start(const WCHAR* ip, u_short port, DWORD m_createThreadNum, DWORD runThreadNum, bool isNagle, bool isTimeOut, unsigned int timeoutInterval, int maxConnectionNum)
{
	int ret_startup;
	int ret_bind;
	int ret_listen;
	int ret_cq;

	ServerInspect();

	m_limitPlayer = maxConnectionNum;
	m_curPlayer = 0;

	WSAData wsaData;
	ret_startup = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (ret_startup != 0)
	{
		return false;
	}

	m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, runThreadNum);
	if (m_hCompletionPort == FALSE)
	{
		return false;
	}

	m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_listenSocket == INVALID_SOCKET)
	{
		return false;
	}

	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET;

	InetPtonW(AF_INET, ip, &serverAddr.sin_addr);
	serverAddr.sin_port = htons(port);
	ret_bind = bind(m_listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
	if (ret_bind == SOCKET_ERROR)
	{
		return false;
	}

	ret_listen = listen(m_listenSocket, SOMAXCONN);
	if (ret_listen == SOCKET_ERROR)
	{
		return false;
	}

	int sendBufferLen = 0;
	setsockopt(m_listenSocket, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufferLen, sizeof(sendBufferLen));

	linger linger;
	linger.l_onoff = true;
	linger.l_linger = 0;
	setsockopt(m_listenSocket, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(linger));

	if (!isNagle)
	{
		DWORD isNoDelay = true;
		setsockopt(m_listenSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&isNoDelay, sizeof(isNoDelay));
	}

	int createThreadNumVal = m_createThreadNum + 1;

	if (isTimeOut) {
		createThreadNumVal++;
		m_timeoutInterval = timeoutInterval;
	}

	m_hThreads = (HANDLE*)malloc(sizeof(HANDLE) * (createThreadNumVal));
	m_pThreadIDs = (unsigned int*)malloc(sizeof(unsigned int) * (createThreadNumVal));

	for (int idx = 0; idx < m_createThreadNum; ++idx)
	{
		m_hThreads[idx] = (HANDLE)_beginthreadex(NULL, 0, WorkProc, this, 0, &m_pThreadIDs[idx]);
	}

	m_hThreads[m_createThreadNum] = (HANDLE)_beginthreadex(NULL, 0, AcceptProc, this, 0, &m_pThreadIDs[m_createThreadNum]);
	if (isTimeOut)
	{
		m_hThreads[m_createThreadNum + 1] = (HANDLE)_beginthreadex(NULL, 0, TimeOutProc, this, 0, &m_pThreadIDs[m_createThreadNum + 1]);
	}

	this->m_createThreadNum = (m_createThreadNum + 1);

	printf("Server Up\n");

	return true;
}

void CNetServer::ServerInspect()
{
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	
	const int remainBits = 17;

	unsigned long long val = 0x8000000000000000;
	long long userMaxAddress = (long long)systemInfo.lpMaximumApplicationAddress;

	for (int cnt = 0; cnt < remainBits; ++cnt)
	{
		long long res = (val & userMaxAddress);
		if (res != 0)
		{
			exit(-1);
		}
		val = (val >> 1);
	}
}

void CNetServer::Stop()
{
	PostQueuedCompletionStatus(m_hCompletionPort, CNetServer::THREAD_EXIT, 0, 0);

	closesocket(m_listenSocket);

	WaitForMultipleObjects(m_createThreadNum, m_hThreads, TRUE, INFINITE);
	for (int idx = 0; idx < m_createThreadNum; ++idx)
	{
		CloseHandle(m_hThreads[idx]);
	}

	CloseHandle(m_hCompletionPort);

	WSACleanup();
}

bool CNetServer::Disconnect(SESSIONID sessionID)
{
	Session* session = GetSession(sessionID);
	
	if ((InterlockedIncrement((long*)&session->ioCntReleaseFlag.ioCnt) >> 16) == true)
	{
		if (InterlockedDecrement16((short*)&session->ioCntReleaseFlag.ioCnt) == 0)
		{
			Release(session->sessionID);
		}
		return false;
	}

	if (session->sessionID != sessionID)
	{
		if (InterlockedDecrement16((short*)&session->ioCntReleaseFlag.ioCnt) == 0)
		{
			Release(session->sessionID);
		}
		return false;
	}

	CancelIoEx((HANDLE)session->socket, NULL);

	if (InterlockedDecrement16((short*)&session->ioCntReleaseFlag.ioCnt) == 0)
		Release(sessionID);

	return true;
}

bool CNetServer::SendPacket(SESSIONID sessionID, CPacket* packet)
{
	Session* session = GetSession(sessionID);

	packet->AddRef();
	if ((InterlockedIncrement((long*)&session->ioCntReleaseFlag.ioCnt) >> 16) == true)
	{
		if (InterlockedDecrement16((short*)&session->ioCntReleaseFlag.ioCnt) == 0)
		{
			Release(session->sessionID);
		}
		CPacket::Free(packet);
		return false;
	}

	if (session->sessionID != sessionID)
	{
		CPacket::Free(packet);
		if (InterlockedDecrement16((short*)&session->ioCntReleaseFlag.ioCnt) == 0)
		{
			Release(session->sessionID);
		}
		return false;
	}

	EncodePacket(packet, GetRandkey());

	session->sendQ.Enqueue(packet);
	
	if (session->sendFlag == false)
	{
		PostQueuedCompletionStatus(m_hCompletionPort, CNetServer::SENDPOST_ZERO, session->sessionID, 0);
	}

	if (InterlockedDecrement16((short*)&session->ioCntReleaseFlag.ioCnt) == 0)
		Release(sessionID);

	return true;
}

bool CNetServer::SendPacketBroadcast(list<SESSIONID>* sessionidList, CPacket* packet)
{
	packet->OnReusePacket();
	for (auto idIter = sessionidList->begin(); idIter != sessionidList->end(); ++idIter)
	{
		SESSIONID sessionID = *idIter;
		SendPacket(sessionID, packet);
	}
	packet->OffReusePacket();

	return true;
}

void CNetServer::SendPacketProc(SESSIONID sessionID, CPacket* packet)
{
	Session* session = GetSession(sessionID);

	packet->AddRef();
	if ((InterlockedIncrement((long*)&session->ioCntReleaseFlag.ioCnt) >> 16) == true)
	{
		if (InterlockedDecrement16((short*)&session->ioCntReleaseFlag.ioCnt) == 0)
		{
			Release(session->sessionID);
		}
		CPacket::Free(packet);
		return;
	}

	if (session->sessionID != sessionID)
	{
		CPacket::Free(packet);
		if (InterlockedDecrement16((short*)&session->ioCntReleaseFlag.ioCnt) == 0)
		{
			Release(session->sessionID);
		}
		return;
	}

	EncodePacket(packet, GetRandkey());

	session->sendQ.Enqueue(packet);
	if (session->sendQ.GetUseSize() >= m_kMaxPacketNums)
		Disconnect(sessionID);
	
	SendPost(session);

	if (InterlockedDecrement16((short*)&session->ioCntReleaseFlag.ioCnt) == 0)
		Release(sessionID);
}

void CNetServer::ReleaseSession(Session* session)
{
	SESSIONID releaseSessionID = session->sessionID;

	closesocket(session->socket);
	session->socket = (SOCKET)INVALID_HANDLE_VALUE;

	CPacket* freePacket;
	while (session->sendQ.Dequeue(freePacket))
	{
		CPacket::Free(freePacket);
	}

	// 완성패킷 큐
	while (session->completePacketQ.Dequeue(freePacket))
	{
		CPacket::Free(freePacket);
	}

	m_stkFreeIdx.push(GetSessionIdx(releaseSessionID));

	InterlockedDecrement((long*)&m_sessionCnt);

	OnClientLeave(releaseSessionID);
}

void CNetServer::Release(SESSIONID sessionID)
{
	Session* session = GetSession(sessionID);

	if (InterlockedCompareExchange((long*)&session->ioCntReleaseFlag.ioCnt, (1 << 16), 0) != 0) return;

	if (session->group == nullptr)
	{
		PostQueuedCompletionStatus(m_hCompletionPort, en_PQCS_PACKET::SESSION_RELEASE, (ULONG_PTR)session, nullptr);
	}
	else
	{
		session->group->ReleaseSessionGroup(sessionID);
	}
}

CNetServer::Session* CNetServer::GetSession(SESSIONID id)
{
	return &m_pArrSession[GetSessionIdx(id)];
}

void CNetServer::RecvProc(Session* session, DWORD recvBytes)
{
	int recvMsgCnt = 0;

	session->recvQ.MoveRear(recvBytes);

	// 완성된 패킷 만들기 //
	while (1)
	{
		NetHeader header;
		int useSize = session->recvQ.GetUseSize();

		if (useSize < sizeof(header)) break;
		session->recvQ.Peek((char*)&header, sizeof(header));

		if (useSize < (sizeof(header) + header.len + 1)) break;

		session->recvQ.MoveFront(sizeof(header));

		CPacket* packet = CPacket::Alloc();
		packet->AddRef();

		packet->MoveWritePos(session->recvQ.Dequeue(packet->GetBufferPtr(), header.len + sizeof(unsigned char)));

		if (!DecodePacket(packet, header.randKey))
		{
			CPacket::Free(packet);
			Disconnect(session->sessionID);
			return;
		}
		
		if (session->group != nullptr)
		{
			session->completePacketQ.Enqueue(packet);
		}
		else
		{
			try {
				OnRecv(packet, session->sessionID);
			}
			catch (exception& e)
			{
				CPacket::Free(packet);
				Disconnect(session->sessionID);
				return;
			}
			CPacket::Free(packet);
		}

		recvMsgCnt++;
	}

	InterlockedAdd((long*)&m_recvMsgCnt, recvMsgCnt);

	RecvPost(session);
}

void CNetServer::RecvPost(Session* session)
{
	int ret_recv;

	WSABUF wsaBuf[2];
	int freeSize = session->recvQ.GetFreeSize();

	DWORD flag = 0;

	InterlockedIncrement16((short*)&session->ioCntReleaseFlag.ioCnt);

	wsaBuf[0].buf = session->recvQ.GetRearBufferPtr();
	wsaBuf[0].len = session->recvQ.DirectEnqueueSize();
	wsaBuf[1].buf = session->recvQ.GetBufferPtr();
	wsaBuf[1].len = 0;
	if (freeSize > wsaBuf[0].len)
		wsaBuf[1].len = freeSize - wsaBuf[0].len;

	ret_recv = WSARecv(session->socket, wsaBuf, 2, NULL, &flag, &session->recvOverlapped.overlapped, NULL);
	if (ret_recv == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING)
		{
			if (InterlockedDecrement16((short*)&session->ioCntReleaseFlag.ioCnt) == 0) Release(session->sessionID);
		}
	}
}

void CNetServer::SendProc(Session* session, int sendPacketNum)
{
	for (int num = 0; num < sendPacketNum; ++num)
	{
		CPacket* freePacket;
		session->sendQ.Dequeue(freePacket);
		CPacket::Free(freePacket);
	}

	InterlockedAdd((long*)&m_sendMsgCnt, sendPacketNum);

	InterlockedExchange8((char*)&session->sendFlag, false);

	SendPost(session);
}

void CNetServer::SendPost(Session* session)
{
	if (session->sendQ.GetUseSize() == 0) return;
	if (InterlockedExchange8((char*)&session->sendFlag, true) == true) return;

	int ret_send;

	WSABUF wsaBuf[m_kWSABUFlen];

	int packetNums = session->sendQ.GetUseSize();

	if (packetNums != 0)
	{
		InterlockedIncrement16((short*)&session->ioCntReleaseFlag.ioCnt);

		CPacket* frontPacket[m_kWSABUFlen];
		if (packetNums >= m_kWSABUFlen) packetNums = m_kWSABUFlen;

		session->sendQ.Peek(frontPacket, packetNums);

		for (int num = 0; num < packetNums; ++num)
		{
			wsaBuf[num].buf = frontPacket[num]->GetHeaderPtr();
			wsaBuf[num].len = frontPacket[num]->GetHeaderSize() + frontPacket[num]->GetDataSize();
		}

		session->sendOverlapped.packetNum = packetNums;
		ret_send = WSASend(session->socket, wsaBuf, (DWORD)packetNums, NULL, 0, &session->sendOverlapped.overlapped, NULL);
		if (ret_send == SOCKET_ERROR)
		{
			int err = WSAGetLastError();
			if (err != WSA_IO_PENDING)
			{
				if (InterlockedDecrement16((short*)&session->ioCntReleaseFlag.ioCnt) == 0) Release(session->sessionID);
			}
		}
	}
	else
	{
		InterlockedExchange8((char*)&session->sendFlag, false);
		PostQueuedCompletionStatus(m_hCompletionPort, CNetServer::SENDPOST_ZERO, (ULONG_PTR)session->sessionID, 0);
	}
}

unsigned WINAPI CNetServer::AcceptProc(PVOID param)
{
	CNetServer* g_server = (CNetServer*)param;

	while (1)
	{
		int ret_iocp;
		int ret_recv;

		DWORD err_recv;
		DWORD flag = 0;
		WSABUF lpBuffer;

		SOCKET client_socket;
		SOCKADDR_IN client_addr;

		int addrLen = sizeof(client_addr);

		client_socket = accept(g_server->m_listenSocket, (SOCKADDR*)&client_addr, &addrLen);
		if (client_socket == INVALID_SOCKET)
		{
			int ret_err = WSAGetLastError();
			if (ret_err == WSAENOTSOCK)
			{
				break;
			}
			continue;
		}

		InterlockedIncrement((long*)&g_server->m_acceptCnt);

		if (!g_server->OnConnectionRequest(client_addr.sin_addr.S_un.S_addr, client_addr.sin_port))
		{
			closesocket(client_socket);
			continue;
		}

		if (g_server->m_curPlayer >= g_server->m_limitPlayer)
		{
			closesocket(client_socket);
			continue;
		}

		g_server->m_baseSessionID = ((g_server->m_baseSessionID + 1) % g_server->MAXSESSIONID);
		SESSIONID sessionIdx;
		if (!g_server->m_stkFreeIdx.pop(sessionIdx))
		{
			closesocket(client_socket);
			continue;
		}

		Session* pSession = g_server->GetSession(sessionIdx);
		
		pSession->sessionID = ((g_server->m_baseSessionID << 16) | sessionIdx);
		g_server->InitialSession(pSession);

		memcpy_s(&pSession->addr, sizeof(pSession->addr), &client_addr, sizeof(client_addr));
		pSession->socket = client_socket;

		if (CreateIoCompletionPort((HANDLE)client_socket, g_server->m_hCompletionPort, (LONG_PTR)pSession, NULL) == NULL)
		{
			continue;
		}

		if (g_server->m_hCompletionPort == FALSE)
		{
			g_server->Release(pSession->sessionID);
			continue;
		}

		InterlockedIncrement((long*)&g_server->m_sessionCnt);
		InterlockedIncrement((long*)&g_server->m_acceptCnt);
		g_server->m_acceptTotalCnt++;

		g_server->ClientJoin(pSession);
	}
	return 0;
}

unsigned WINAPI CNetServer::WorkProc(PVOID param)
{
	CNetServer* g_server = (CNetServer*)param;

	BOOL ret_gqcs;
	MYOVERLAPPED* myOverlapped;

	DWORD numBytes = 0;
	ULONG_PTR completionKey = 0;
	LPOVERLAPPED overlapped = NULL;

	while (1)
	{
		ret_gqcs = GetQueuedCompletionStatus(g_server->m_hCompletionPort, &numBytes, &completionKey, &overlapped, INFINITE);

		Session* session = (Session*)completionKey;

		if (numBytes >= CRingBuffer::MAX_BUFFER_SIZE)
		{
			switch (numBytes)
			{
			case CNetServer::THREAD_EXIT:
				PostQueuedCompletionStatus(g_server->m_hCompletionPort, CNetServer::THREAD_EXIT, 0, 0);
				goto exit;
				break;
			case CNetServer::SENDPOST_ZERO:
			{
				SESSIONID sessionID = (SESSIONID)completionKey;
				g_server->SendPostLogic(sessionID);
				continue;
			}
			case CNetServer::SESSION_RELEASE:
			{
				g_server->ReleaseSession(session);
				continue;
			}
			continue;
			}
		}

		if (ret_gqcs == TRUE && numBytes != 0)
		{
			myOverlapped = (MYOVERLAPPED*)overlapped;
			if (myOverlapped->isRecv)
				g_server->RecvProc(session, numBytes);
			else
				g_server->SendProc(session, myOverlapped->packetNum);
		}

		if (InterlockedDecrement16((short*)&session->ioCntReleaseFlag.ioCnt) == 0)
		{
			g_server->Release(session->sessionID);
		}
	}
exit:
	return 0;
}

unsigned WINAPI CNetServer::TimeOutProc(PVOID param)
{
	CNetServer* g_server = (CNetServer*)param;

	while (true)
	{
		g_server->OnTimeout();

		Sleep(g_server->m_timeoutInterval);
	}
}

void CNetServer::ClientJoin(Session* session)
{
	USHORT port = session->addr.sin_port;
	WCHAR clientIP[16] = { L'\0' };
	InetNtopW(AF_INET, &session->addr.sin_addr, clientIP, sizeof(clientIP) / sizeof(WCHAR));
	OnClientJoin(session->sessionID, clientIP, port);

	RecvPost(session);

	if (InterlockedDecrement16((short*)&session->ioCntReleaseFlag.ioCnt) == 0)
	{
		Release(session->sessionID);
	}
}

void CNetServer::InitialSession(Session* session)
{
	session->recvOverlapped = { {0}, true};
	session->sendOverlapped = { {0}, false};
	session->sendFlag = false;
	session->recvQ.ClearBuffer();
	InterlockedIncrement16((short*)&session->ioCntReleaseFlag.ioCnt);
	session->ioCntReleaseFlag.releaseFlag = false;
	session->group = nullptr;
}

void CNetServer::EncodePacket(CPacket* packet, unsigned char randKey)
{
	if (packet->m_encodeFlag == true) return;

	char* payload = packet->GetBufferPtr();
	unsigned short len = packet->GetDataSize();

	unsigned char data;
	unsigned char passive = 0;
	unsigned char cipher = 0;
	unsigned int checksum = 0;

	for (int idx = 0; idx < len; ++idx)
	{
		checksum += payload[idx];
	}

	unsigned char resultChecksum = (checksum % 256);
	packet->PutHeader((char*)&resultChecksum, sizeof(resultChecksum));

	len++;
	payload = packet->GetHeaderPtr();

	for (int idx = 0; idx < len; ++idx)
	{
		data = payload[idx];
		passive = data ^ (passive + randKey + idx + 1);
		cipher = passive ^ (cipher + m_packetKey + idx + 1);
		payload[idx] = cipher;
	}

	NetHeader header;
	header.code = m_packetCode;
	header.len = packet->GetDataSize();
	header.randKey = randKey;

	packet->PutHeader((char*)&header, sizeof(header));

	packet->m_encodeFlag = true;
}

bool CNetServer::DecodePacket(CPacket* packet, unsigned char randKey)
{
	char* payload = packet->GetBufferPtr();
	unsigned short len = packet->GetDataSize();

	unsigned char origin = payload[0];

	unsigned char data;

	unsigned char curCipher;
	unsigned char prvCipher = 0;
	unsigned char curPassive;
	unsigned char prvPassive = 0;

	unsigned int checksum = 0;

	for (int idx = 0; idx < len; ++idx)
	{
		curCipher = payload[idx];
		curPassive = curCipher ^ (prvCipher + m_packetKey + idx + 1);
		data = curPassive ^ (prvPassive + randKey + idx + 1);
		payload[idx] = data;

		prvCipher = curCipher;
		prvPassive = curPassive;

		if (idx >= 1)
		{
			checksum += data;
		}
	}

	unsigned char resultChecksum = (checksum % 256);

	if (resultChecksum == (unsigned char)payload[0])
	{
		packet->MoveReadPos(sizeof(resultChecksum));
		return true;
	}
	else
	{
		return false;
	}
}

unsigned char CNetServer::GetRandkey()
{
	return rand() % 256;
}

void CNetServer::SendPostLogic(SESSIONID sessionID)
{
	Session* session = GetSession(sessionID);
	
	if ((InterlockedIncrement((long*)&session->ioCntReleaseFlag.ioCnt) >> 16) == true)
	{
		if (InterlockedDecrement16((short*)&session->ioCntReleaseFlag.ioCnt) == 0)
		{
			Release(session->sessionID);
		}
		return;
	}

	if (session->sessionID != sessionID)
	{
		if (InterlockedDecrement16((short*)&session->ioCntReleaseFlag.ioCnt) == 0)
		{
			Release(session->sessionID);
		}
		return;
	}

	SendPost(session);

	if (InterlockedDecrement16((short*)&session->ioCntReleaseFlag.ioCnt) == 0)
		Release(session->sessionID);
}