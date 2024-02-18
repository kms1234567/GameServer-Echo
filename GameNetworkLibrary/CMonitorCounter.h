#pragma once
#pragma comment(lib,"Pdh.lib")
#include <Pdh.h>
#include <Windows.h>
#include <iostream>
class CMonitorCounter
{
public:
    typedef struct st_monitorValue
    {
        union {
            LONG        longValue;
            double      doubleValue;
            LONGLONG    largeValue;
        };
    }MonitorValue;

    enum MONITOR_VALUE_TYPE
    {
        LONG_VALUE = PDH_FMT_LONG,
        DOUBLE_VALUE = PDH_FMT_DOUBLE,
        LARGE_VALUE = PDH_FMT_LARGE,
    };

    enum MONITOR_LIST
    {
        PROCESSOR_CPU_TIME = 0,
        PROCESS_CPU_TIME = 1,
        PROCESS_PRIVATE_BYTES = 2,
        PROCESS_NONPAGED_BYTES = 3,
        MEMORY_AVALIABLE_MBYTES = 4,
        MEMORY_NONPAGED_BYTES = 5,
        NETWORK_RECV_BYTES = 6,
        NETWORK_SEND_BYTES = 7,

        MONITOR_LIST_COUNT
    };
private:
    PDH_HQUERY m_query;
    PDH_HQUERY m_recvQuery;
    PDH_HQUERY m_sendQuery;
    PDH_HCOUNTER m_arrCounter[MONITOR_LIST::MONITOR_LIST_COUNT - 2];
    HCOUNTER m_arrRecvCounter[10];
    HCOUNTER m_arrSendCounter[10];
    int m_networkInterfaceCnt;
public:
    CMonitorCounter()
    {
        PdhOpenQuery(NULL, NULL, &m_query);
        PdhOpenQuery(NULL, NULL, &m_recvQuery);
        PdhOpenQuery(NULL, NULL, &m_sendQuery);

        WCHAR buffer[250] = { L'\0', };
        WCHAR* token = NULL;
        WCHAR* nextToken = NULL;
        WCHAR seps[10] = { L"\\." };
        DWORD bufferSize = 250;

        QueryFullProcessImageName(GetCurrentProcess(), 0, buffer, &bufferSize);

        token = wcstok_s(buffer, seps, &nextToken);
        while (wcscmp(nextToken, L"exe"))
        {
            token = wcstok_s(NULL, seps, &nextToken);
        }

        WCHAR queryString[250];

        memset(queryString, 0, sizeof(queryString));
        swprintf_s(queryString, sizeof(queryString) / sizeof(WCHAR), L"\\Processor(_Total)\\%% Processor Time");
        PdhAddCounter(m_query, queryString, NULL, &m_arrCounter[MONITOR_LIST::PROCESSOR_CPU_TIME]);
        PdhCollectQueryData(m_query);
        memset(queryString, 0, sizeof(queryString));
        swprintf_s(queryString, sizeof(queryString) / sizeof(WCHAR), L"\\Process(%s)\\%% Processor Time", token);
        PdhAddCounter(m_query, queryString, NULL, &m_arrCounter[MONITOR_LIST::PROCESS_CPU_TIME]);

        memset(queryString, 0, sizeof(queryString));
        swprintf_s(queryString, sizeof(queryString) / sizeof(WCHAR), L"\\Process(%s)\\Private Bytes", token);
        PdhAddCounter(m_query, queryString, NULL, &m_arrCounter[MONITOR_LIST::PROCESS_PRIVATE_BYTES]);

        memset(queryString, 0, sizeof(queryString));
        swprintf_s(queryString, sizeof(queryString) / sizeof(WCHAR), L"\\Process(%s)\\Pool Nonpaged Bytes", token);
        PdhAddCounter(m_query, queryString, NULL, &m_arrCounter[MONITOR_LIST::PROCESS_NONPAGED_BYTES])
            ;
        memset(queryString, 0, sizeof(queryString));
        swprintf_s(queryString, sizeof(queryString) / sizeof(WCHAR), L"\\Memory\\Available MBytes");
        PdhAddCounter(m_query, queryString, NULL, &m_arrCounter[MONITOR_LIST::MEMORY_AVALIABLE_MBYTES]);

        memset(queryString, 0, sizeof(queryString));
        swprintf_s(queryString, sizeof(queryString) / sizeof(WCHAR), L"\\Memory\\Pool Nonpaged Bytes");
        PdhAddCounter(m_query, queryString, NULL, &m_arrCounter[MONITOR_LIST::MEMORY_NONPAGED_BYTES]);

        WCHAR* counterListBuffer = nullptr;
        WCHAR* instanceListBuffer = nullptr;
        DWORD counterListLen = 0;
        DWORD instanceListLen = 0;
        PdhEnumObjectItems(NULL, NULL, L"Network Interface", counterListBuffer, &counterListLen, instanceListBuffer, &instanceListLen, PERF_DETAIL_WIZARD, NULL);
        counterListBuffer = new WCHAR[counterListLen];
        instanceListBuffer = new WCHAR[instanceListLen];

        PdhEnumObjectItems(NULL, NULL, L"Network Interface", counterListBuffer, &counterListLen, instanceListBuffer, &instanceListLen, PERF_DETAIL_WIZARD, NULL);

        int networkInterfaceIdx = 0;

        for (WCHAR* pTemp = instanceListBuffer; *pTemp != 0; pTemp += wcslen(pTemp) + 1)
        {
            memset(queryString, 0, sizeof(queryString));
            swprintf_s(queryString, sizeof(queryString) / sizeof(WCHAR), L"\\Network Interface(%s)\\Bytes Sent/sec", pTemp);
            wprintf(L"%s\n", queryString);
            PdhAddCounter(m_sendQuery, queryString, NULL, &m_arrSendCounter[networkInterfaceIdx]);
            networkInterfaceIdx++;
        }

        networkInterfaceIdx = 0;
        for (WCHAR* pTemp = instanceListBuffer; *pTemp != 0; pTemp += wcslen(pTemp) + 1)
        {
            memset(queryString, 0, sizeof(queryString));
            swprintf_s(queryString, sizeof(queryString) / sizeof(WCHAR), L"\\Network Interface(%s)\\Bytes Received/sec", pTemp);
            wprintf(L"%s\n", queryString);
            PdhAddCounter(m_recvQuery, queryString, NULL, &m_arrRecvCounter[networkInterfaceIdx]);
            networkInterfaceIdx++;
        }

        m_networkInterfaceCnt = networkInterfaceIdx;

        PdhCollectQueryData(m_query);
        PdhCollectQueryData(m_sendQuery);
        PdhCollectQueryData(m_recvQuery);

        delete[] counterListBuffer;
        delete[] instanceListBuffer;
    }

    ~CMonitorCounter() {}

    void GetMonitorValue(MONITOR_LIST monitorType, MONITOR_VALUE_TYPE valueType, MonitorValue* value)
    {
        PDH_FMT_COUNTERVALUE counterVal;
        counterVal.largeValue = 0;

        if (monitorType == NETWORK_RECV_BYTES)
        {
            PdhCollectQueryData(m_recvQuery);

            long long recvBytesValue = 0;
            for (int networkIdx = 0; networkIdx < m_networkInterfaceCnt; ++networkIdx)
            {
                PdhGetFormattedCounterValue(m_arrRecvCounter[networkIdx], valueType, NULL, &counterVal);
                recvBytesValue += counterVal.largeValue;
            }
            value->largeValue = recvBytesValue;
        }
        else if (monitorType == NETWORK_SEND_BYTES)
        {
            PdhCollectQueryData(m_sendQuery);

            long long sendBytesValue = 0;
            for (int networkIdx = 0; networkIdx < m_networkInterfaceCnt; ++networkIdx)
            {
                DWORD CounterType;
                PdhGetFormattedCounterValue(m_arrSendCounter[networkIdx], valueType, &CounterType, &counterVal);
                sendBytesValue += counterVal.largeValue;
            }
            value->largeValue = sendBytesValue;
        }
        else
        {
            PdhCollectQueryData(m_query);
            PdhGetFormattedCounterValue(m_arrCounter[monitorType], valueType | PDH_FMT_NOCAP100, NULL, &counterVal);
            value->largeValue = counterVal.largeValue;
        }
    }
};