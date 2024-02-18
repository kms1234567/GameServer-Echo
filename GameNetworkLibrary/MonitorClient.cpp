#include "MonitorClient.h"
#include "GameServer.h"
#include "MakeMonitorPacket.h"
#include "Parser.h"

MonitorClient::MonitorClient()
{

}

MonitorClient::~MonitorClient()
{

}

unsigned WINAPI MonitorClient::UpdateProc(PVOID params)
{
	MonitorClient* client = (MonitorClient*)params;
	GameServer* server = (GameServer*)client->monitorServer;
	time_t ltime;

	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);

	int processorNums = systemInfo.dwNumberOfProcessors;

	CPacket* packet = CPacket::Alloc();
	MakeMonitorLoginPacket(packet, client->m_serverNo);
	client->SendPacket(packet);

	while (!client->m_bShutdown)
	{
		DWORD curTime = timeGetTime();
		DWORD diffTime = curTime - client->m_prevTime;
		if (diffTime < 1000)
		{
			Sleep(1000 - diffTime);
		}
		client->m_prevTime += 1000;

		time(&ltime);

		packet = CPacket::Alloc();
		MakeMonitorDataPacket(packet, dfMONITOR_DATA_TYPE_GAME_SERVER_RUN, true, ltime);
		client->SendPacket(packet);

		CMonitorCounter::MONITOR_LIST listType;
		/*
		dfMONITOR_DATA_TYPE_GAME_SERVER_RUN = 10,		// GameServer 실행 여부 ON / OFF
		dfMONITOR_DATA_TYPE_GAME_SERVER_CPU = 11,		// GameServer CPU 사용률
		dfMONITOR_DATA_TYPE_GAME_SERVER_MEM = 12,		// GameServer 메모리 사용 MByte
		dfMONITOR_DATA_TYPE_GAME_SESSION = 13,		// 게임서버 세션 수 (컨넥션 수)
		dfMONITOR_DATA_TYPE_GAME_AUTH_PLAYER = 14,		// 게임서버 AUTH MODE 플레이어 수
		dfMONITOR_DATA_TYPE_GAME_GAME_PLAYER = 15,		// 게임서버 GAME MODE 플레이어 수
		dfMONITOR_DATA_TYPE_GAME_ACCEPT_TPS = 16,		// 게임서버 Accept 처리 초당 횟수
		dfMONITOR_DATA_TYPE_GAME_PACKET_RECV_TPS = 17,		// 게임서버 패킷처리 초당 횟수
		dfMONITOR_DATA_TYPE_GAME_PACKET_SEND_TPS = 18,		// 게임서버 패킷 보내기 초당 완료 횟수
		dfMONITOR_DATA_TYPE_GAME_DB_WRITE_TPS = 19,		// 게임서버 DB 저장 메시지 초당 처리 횟수
		dfMONITOR_DATA_TYPE_GAME_DB_WRITE_MSG = 20,		// 게임서버 DB 저장 메시지 큐 개수 (남은 수)
		dfMONITOR_DATA_TYPE_GAME_AUTH_THREAD_FPS = 21,		// 게임서버 AUTH 스레드 초당 프레임 수 (루프 수)
		dfMONITOR_DATA_TYPE_GAME_GAME_THREAD_FPS = 22,		// 게임서버 GAME 스레드 초당 프레임 수 (루프 수)
		dfMONITOR_DATA_TYPE_GAME_PACKET_POOL = 23,		// 게임서버 패킷풀 사용량
		*/
		
		CMonitorCounter::MonitorValue value;
		long processCPUTimeValue;
		long long processPrivateMemoryValue;
		long sessionNumsValue;
		long authPlayerNumsValue;
		long gamePlayerNumsValue;
		long packetNumsValue;
		long packetCapacityValue = CPacket::GetPacketCapacity();
		long sendTPS = server->GetSendMsgCnt();
		long recvTPS = server->GetRecvMsgCnt();
		long acceptTPS = server->GetAcceptCnt();
		long authFrame = server->GetAuthGroupFrame();
		long gameFrame = server->GetEchoGroupFrame();

		packet = CPacket::Alloc();
		listType = CMonitorCounter::MONITOR_LIST::PROCESS_CPU_TIME;
		client->m_monitor.GetMonitorValue(listType,
			CMonitorCounter::MONITOR_VALUE_TYPE::LONG_VALUE, &value);
		processCPUTimeValue = (value.longValue / processorNums);
		MakeMonitorDataPacket(packet, dfMONITOR_DATA_TYPE_GAME_SERVER_CPU, processCPUTimeValue, ltime);
		client->SendPacket(packet);

		packet = CPacket::Alloc();
		listType = CMonitorCounter::MONITOR_LIST::PROCESS_PRIVATE_BYTES;
		client->m_monitor.GetMonitorValue(listType,
			CMonitorCounter::MONITOR_VALUE_TYPE::LARGE_VALUE, &value);
		processPrivateMemoryValue = (value.largeValue / (10 * 10 * 10 * 10 * 10 * 10));
		MakeMonitorDataPacket(packet, dfMONITOR_DATA_TYPE_GAME_SERVER_MEM, processPrivateMemoryValue, ltime);
		client->SendPacket(packet);

		packet = CPacket::Alloc();
		sessionNumsValue = server->GetSessionNums();
		MakeMonitorDataPacket(packet, dfMONITOR_DATA_TYPE_GAME_SESSION, sessionNumsValue, ltime);
		client->SendPacket(packet);

		packet = CPacket::Alloc();
		authPlayerNumsValue = server->GetAuthGroupPlayer();
		MakeMonitorDataPacket(packet, dfMONITOR_DATA_TYPE_GAME_AUTH_PLAYER, authPlayerNumsValue, ltime);
		client->SendPacket(packet);

		packet = CPacket::Alloc();
		gamePlayerNumsValue = server->GetEchoGroupPlayer();
		MakeMonitorDataPacket(packet, dfMONITOR_DATA_TYPE_GAME_GAME_PLAYER, gamePlayerNumsValue, ltime);
		client->SendPacket(packet);
		
		packet = CPacket::Alloc();
		MakeMonitorDataPacket(packet, dfMONITOR_DATA_TYPE_GAME_ACCEPT_TPS, acceptTPS, ltime);
		client->SendPacket(packet);

		packet = CPacket::Alloc();
		MakeMonitorDataPacket(packet, dfMONITOR_DATA_TYPE_GAME_PACKET_RECV_TPS, recvTPS, ltime);
		client->SendPacket(packet);

		packet = CPacket::Alloc();
		MakeMonitorDataPacket(packet, dfMONITOR_DATA_TYPE_GAME_PACKET_SEND_TPS, sendTPS, ltime);
		client->SendPacket(packet);

		packet = CPacket::Alloc();
		MakeMonitorDataPacket(packet, dfMONITOR_DATA_TYPE_GAME_AUTH_THREAD_FPS, authFrame, ltime);
		client->SendPacket(packet);

		packet = CPacket::Alloc();
		MakeMonitorDataPacket(packet, dfMONITOR_DATA_TYPE_GAME_GAME_THREAD_FPS, gameFrame, ltime);
		client->SendPacket(packet);

		packet = CPacket::Alloc();
		packetNumsValue = CPacket::GetPacketUse();
		MakeMonitorDataPacket(packet, dfMONITOR_DATA_TYPE_GAME_PACKET_POOL, packetNumsValue, ltime);
		client->SendPacket(packet);

		printf(
			"\n--------------------------------------------------------------\n"
			"Process CPU Time     : %6d | Process Private MB   : %6d\n"
			"Send Msg TPS         : %10d\n"
			"Recv Msg TPS         : %10d\n"
			"Session Nums         : %6d | Accept TPS           : %6d\n"
			"Auth Player          : %6d | Echo Player          : %6d\n"
			"Auth FPS             : %6d | Echo FPS             : %6d\n"
			"Packet Pool Capacity : %10d\n"
			"Packet Pool Use      : %10d\n"
			"Accept Total Cnt     : %10d\n"
			"--------------------------------------------------------------\n"
			, processCPUTimeValue, processPrivateMemoryValue
			, sendTPS , recvTPS
			, sessionNumsValue, acceptTPS
			, authPlayerNumsValue, gamePlayerNumsValue
			, authFrame, gameFrame
			, packetCapacityValue, packetNumsValue
			, server->GetAcceptTotalCnt()
		);
	}

	return 0;
}

void MonitorClient::MonitorStart(CNetServer* server)
{
	monitorServer = server;
	m_prevTime = timeGetTime();
	m_bShutdown = false;

	Parser parser("config.ini");
	parser.Load("SERVER");

	parser.GetValue("ID", (char*)&m_serverNo);

	m_hUpdateThread = (HANDLE)_beginthreadex(nullptr, 0, UpdateProc, this, 0, &m_updateThreadID);
}

void MonitorClient::OnRecv(CPacket* packet)
{

}