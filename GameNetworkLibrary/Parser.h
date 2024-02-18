#pragma once
#include <Windows.h>
#include <math.h>
using namespace std;

class Parser
{
private:
	enum {
		maxLen = 200,
		maxDataLen = (1024 * 100)
	};

	HANDLE hFile;

	char m_buffer[maxDataLen];

	char m_tableBuffer[maxDataLen];
	char m_fileName[maxLen];
	char m_tableName[maxLen];
public:
	Parser(const char* fileName)
	{
		memcpy_s(m_fileName, sizeof(m_fileName), fileName, strlen(fileName) + 1);
		memset(m_tableName, 0, sizeof(m_tableName));

		hFile = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, NULL);

		DWORD dataReadBytes;
		ReadFile(hFile, m_buffer, sizeof(m_buffer), &dataReadBytes, NULL);
	}

	void Load(const char* tableName)
	{
		if (strcmp(m_tableName, tableName) == 0) return;

		int wordLen;

		char* ptr = m_buffer;
		char* startPtr;
		char* endPtr;

		while (true)
		{
			while (*ptr != ':') { ptr++; }

			startPtr = ++ptr;
			FindWordEndPointer(ptr);
			endPtr = ptr;

			wordLen = endPtr - startPtr;

			memcpy_s(m_tableName, sizeof(m_tableName), startPtr, wordLen);
			m_tableName[wordLen] = '\0';

			if (strcmp(tableName, m_tableName) == 0) break;
		}

		while (*ptr != '{') { ptr++; }
		ptr++;

		startPtr = ptr;

		while (true)
		{
			if (*ptr == '}') break;
			ptr++;
		}

		memcpy_s(m_tableBuffer, sizeof(m_tableBuffer), startPtr, (ptr - startPtr));
	}

	bool GetValue(const char* key, char* value, int cpySize = 4)
	{
		char* ptr = m_tableBuffer;
		char* startPtr;
		char* endPtr;

		int wordLen;
		while (true)
		{
			char cmpKey[maxLen];

			while (!isalpha(*ptr)) { ptr++; }

			startPtr = ptr;
			FindWordEndPointer(ptr);
			endPtr = ptr;

			wordLen = endPtr - startPtr;

			memcpy_s(cmpKey, sizeof(cmpKey), startPtr, wordLen);
			cmpKey[wordLen] = '\0';

			if (strcmp(key, cmpKey) == 0) break;
		}

		while (*ptr != '=') { ptr++; }
		++ptr;

		while (true)
		{
			if (*ptr == '"')
			{
				startPtr = ++ptr;
				FindWordEndPointer(ptr);
				endPtr = ptr;

				wordLen = endPtr - startPtr;

				memcpy_s(value, wordLen, startPtr, wordLen);
				break;
			}
			else if (isdigit(*ptr))
			{
				startPtr = ptr;
				FindWordEndPointer(ptr);
				endPtr = ptr;

				wordLen = endPtr - startPtr;

				int resValue = 0;
				int topNumber = (int)pow(10, wordLen - 1);

				for (int idx = 0; idx < wordLen; ++idx)
				{
					resValue += ((*(startPtr + idx) - '0') * topNumber);
					topNumber /= 10;
				}

				memcpy_s(value, cpySize, &resValue, cpySize);
				break;
			}
			ptr++;
		}

		return true;
	};

private:
	void FindWordEndPointer(char*& ptr)
	{
		while (
			*ptr != ',' && *ptr != 0x09 &&
			*ptr != '"' && *ptr != 0x20 &&
			*ptr != 0x08 && *ptr != 0x0a &&
			*ptr != 0x0d
			)
		{
			ptr++;
		}
	}
};