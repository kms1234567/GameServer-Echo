#include "GameServer.h"
#include "MonitorClient.h"
#include "Parser.h"
//#include <conio.h>
GameServer server;
MonitorClient client;

void InitializeServer();

int main()
{
	InitializeServer();

	while (1)
	{
		// 입력 확인 후 서버 종료
		/*if (_kbhit()) {      
			char key = _getch();
			if (key == 'q' || key == 'Q')
			{
				server.Stop();
				client.Stop();
			}
		}*/

		Sleep(INFINITE);
	}

	return 0;
}

void InitializeServer()
{
	Parser parser("config.ini");
	u_short port;

	parser.Load("GAME_SERVER");
	parser.GetValue("PORT", (char*)&port, sizeof(port));

	parser.Load("MONITOR");
	char szMonitorIP[16] = { '\0', };
	WCHAR wszMonitorIP[16] = { '\0', };
	u_short monitorPort;

	parser.GetValue("IP", (char*)szMonitorIP);
	parser.GetValue("PORT", (char*)&monitorPort, sizeof(monitorPort));
	MultiByteToWideChar(CP_ACP, 0, szMonitorIP, 16, wszMonitorIP, 16);

	server.Start(L"0.0.0.0", port, 6, 3, true, false, 10000, 20000);

	client.Start(wszMonitorIP, monitorPort, 1, 1);
	client.MonitorStart(&server);
}