#pragma once
#include "MemoryPoolTLS.h"
#include <iostream>

class CPacket
{
private:
	char* m_chpFrontPos;
	char* m_chpRearPos;

	char* m_chpPacketBuffer;
	char* m_chpHeader;
	char* m_chpBuffer;

	int m_iBufferSize;

	bool m_useFlag;

	static MemoryPoolTLS<CPacket> m_pool;

	//alignas(64) 
	int m_refCnt;
	//alignas(64) 
	int m_refTmpCnt;

	//alignas(64) 
	static int allocPacketNum;
	//alignas(64)
	static int freePacketNum;

	friend Bucket<CPacket>;

	enum en_PACKET
	{
		eBUFFER_DEFAULT = 80, eBUFFER_MAX = 8000, eHEADER_SIZE = 32
	};
public:
	bool m_encodeFlag;
private:
	CPacket()
	{
		m_chpPacketBuffer = new char[eBUFFER_DEFAULT + eHEADER_SIZE];

		m_chpBuffer = m_chpPacketBuffer + eHEADER_SIZE;
		m_chpHeader = m_chpPacketBuffer + eHEADER_SIZE;

		m_iBufferSize = eBUFFER_DEFAULT;

		m_chpRearPos = m_chpBuffer;
		m_chpFrontPos = m_chpBuffer;
	}

	CPacket(const CPacket& ref) {}
	CPacket& operator=(const CPacket& ref) {}

	virtual ~CPacket()
	{
		delete[] m_chpPacketBuffer;
	}
public:
	static int GetPacketCapacity()
	{

		return m_pool.GetPoolCapacity();
	}

	static int GetPacketUse()
	{

		return m_pool.GetPoolUse();
	}

	static CPacket* Alloc()
	{
		InterlockedIncrement((long*)&allocPacketNum);

		CPacket* pPacket = m_pool.Alloc();
		pPacket->m_refCnt = 0;
		pPacket->m_refTmpCnt = 0;
		pPacket->m_useFlag = false;
		pPacket->Clear();
		pPacket->m_encodeFlag = false;
		return pPacket;
	}

	void AddRef()
	{
		if (m_useFlag)
		{
			InterlockedIncrement((long*)&m_refTmpCnt);
		}
		else
		{
			InterlockedIncrement((long*)&m_refCnt);
		}
	}

	static void Free(CPacket* freePacket)
	{
		if (InterlockedDecrement((long*)&freePacket->m_refCnt) == 0)
		{
			//printf("%p %p Free\n", freePacket, freePacket->m_chpFrontPos);
			InterlockedIncrement((long*)&freePacketNum);
			m_pool.Free(freePacket);
		}
	}

	void OnReusePacket()
	{
		m_useFlag = true;
	}

	void OffReusePacket()
	{
		if (InterlockedAdd((long*)&m_refCnt, m_refTmpCnt) == 0)
		{
			m_pool.Free(this);
		}
	}

	// Resize
	/*
	bool Resize(int iSize)
	{
		if (GetBufferSize() == eBUFFER_MAX) return false;
		if (iSize < GetBufferSize()) iSize = GetBufferSize() * 2;
		int resize = (iSize > eBUFFER_MAX) ? eBUFFER_MAX : iSize;

		char* new_chpBuffer = new char[resize];
		memcpy_s(new_chpBuffer, resize, m_chpBuffer, m_iBufferSize);

		// Resize를 진행할 때는 로깅이 필요하다. 개발자에게 기본 사이즈가 부족하다는 것을 알려야함.

		m_chpRearPos = new_chpBuffer + (m_chpRearPos - m_chpBuffer);
		m_chpFrontPos = new_chpBuffer + (m_chpFrontPos - m_chpBuffer);

		delete[]m_chpBuffer;
		m_chpBuffer = new_chpBuffer;
		m_iBufferSize = resize;
		return true;
	}
	*/

	void Clear() {
		m_chpFrontPos = m_chpBuffer;
		m_chpRearPos = m_chpBuffer;
		m_chpHeader = m_chpBuffer;
	}

	void ClearHeader() {
		m_chpHeader = m_chpBuffer;
	}

	int GetBufferSize() { return m_iBufferSize; }

	int GetDataSize() { return (m_chpRearPos - m_chpFrontPos); }

	int GetHeaderSize() { return (m_chpBuffer - m_chpHeader); }

	char* GetHeaderPtr() { return m_chpHeader; }

	char* GetBufferPtr() { return m_chpBuffer; }

	int MoveWritePos(int iSize)
	{
		if (iSize <= 0) return 0;
		m_chpRearPos += iSize;

		return iSize;
	}
	int MoveReadPos(int iSize)
	{
		if (iSize <= 0) return 0;
		m_chpFrontPos += iSize;

		return iSize;
	}
	int GetHeader(char* chpDest, int iSize)
	{
		if (GetHeaderSize() < iSize) return 0;

		if (memcpy_s(chpDest, iSize, m_chpHeader, iSize) != 0) return 0;

		m_chpHeader += iSize;
		return iSize;
	}
	int GetData(char* chpDest, int iSize)
	{
		if ((m_chpFrontPos + iSize) > m_chpRearPos)
		{
			throw std::exception::exception();
		}

		if (memcpy_s(chpDest, iSize, m_chpFrontPos, iSize) != 0) return 0;

		m_chpFrontPos += iSize;
		return iSize;
	}
	int PutData(char* chpSrc, int iSize)
	{
		if (memcpy_s(m_chpRearPos, iSize, chpSrc, iSize) != 0) return 0;

		m_chpRearPos += iSize;
		return iSize;
	}
	int PutHeader(char* chpSrc, int size)
	{
		if ((m_chpHeader - size) < (m_chpBuffer - eHEADER_SIZE)) return 0;

		m_chpHeader -= size;

		if (memcpy_s(m_chpHeader, size, chpSrc, size) != 0) return 0;
		return size;
	}

	/*CPacket& operator =  (CPacket& clSrcPacket)
	{
		m_iBufferSize = clSrcPacket.GetBufferSize();

		delete[]m_chpBuffer;
		m_chpBuffer = new char[m_iBufferSize];
		memcpy_s(m_chpBuffer, clSrcPacket.GetDataSize(), clSrcPacket.GetBufferPtr(), clSrcPacket.GetDataSize());

		return (*this);
	}*/

	/* 데이터 넣기 */
	CPacket& operator << (unsigned char byValue)
	{
		memcpy_s(m_chpRearPos, sizeof(byValue), &byValue, sizeof(byValue));
		m_chpRearPos += sizeof(byValue);
		return *this;
	}
	CPacket& operator << (char chValue)
	{
		memcpy_s(m_chpRearPos, sizeof(chValue), &chValue, sizeof(chValue));
		m_chpRearPos += sizeof(chValue);
		return *this;
	}
	CPacket& operator << (short shValue)
	{
		memcpy_s(m_chpRearPos, sizeof(shValue), &shValue, sizeof(shValue));
		m_chpRearPos += sizeof(shValue);
		return *this;
	}
	CPacket& operator << (unsigned short wValue)
	{
		memcpy_s(m_chpRearPos, sizeof(wValue), &wValue, sizeof(wValue));
		m_chpRearPos += sizeof(wValue);
		return *this;
	}
	CPacket& operator << (int iValue)
	{
		memcpy_s(m_chpRearPos, sizeof(iValue), &iValue, sizeof(iValue));
		m_chpRearPos += sizeof(iValue);
		return *this;
	}
	CPacket& operator << (long lValue)
	{
		memcpy_s(m_chpRearPos, sizeof(lValue), &lValue, sizeof(lValue));
		m_chpRearPos += sizeof(lValue);
		return *this;
	}
	CPacket& operator << (float fValue)
	{
		memcpy_s(m_chpRearPos, sizeof(fValue), &fValue, sizeof(fValue));
		m_chpRearPos += sizeof(fValue);
		return *this;
	}
	CPacket& operator << (__int64 iValue)
	{
		memcpy_s(m_chpRearPos, sizeof(iValue), &iValue, sizeof(iValue));
		m_chpRearPos += sizeof(iValue);
		return *this;
	}
	CPacket& operator << (double dValue)
	{
		memcpy_s(m_chpRearPos, sizeof(dValue), &dValue, sizeof(dValue));
		m_chpRearPos += sizeof(dValue);
		return *this;
	}

	/* 데이터 빼기 */
	CPacket& operator >> (unsigned char& byValue)
	{
		/*if ((m_chpFrontPos + sizeof(byValue)) > m_chpRearPos)
		{
			throw std::exception::exception();
		}*/
		memcpy_s(&byValue, sizeof(byValue), m_chpFrontPos, sizeof(byValue));
		m_chpFrontPos += sizeof(byValue);
		return *this;
	}
	CPacket& operator >> (char& chValue)
	{
		/*if ((m_chpFrontPos + sizeof(chValue)) > m_chpRearPos)
		{
			throw std::exception::exception();
		}*/
		memcpy_s(&chValue, sizeof(chValue), m_chpFrontPos, sizeof(chValue));
		m_chpFrontPos += sizeof(chValue);
		return *this;
	}
	CPacket& operator >> (short& shValue)
	{
		/*if ((m_chpFrontPos + sizeof(shValue)) > m_chpRearPos)
		{
			throw std::exception::exception();
		}*/
		memcpy_s(&shValue, sizeof(shValue), m_chpFrontPos, sizeof(shValue));
		m_chpFrontPos += sizeof(shValue);
		return *this;
	}
	CPacket& operator >> (unsigned short& wValue)
	{
		/*if ((m_chpFrontPos + sizeof(wValue)) > m_chpRearPos)
		{
			throw std::exception::exception();
		}*/
		memcpy_s(&wValue, sizeof(wValue), m_chpFrontPos, sizeof(wValue));
		m_chpFrontPos += sizeof(wValue);
		return *this;
	}
	CPacket& operator >> (int& iValue)
	{
		/*if ((m_chpFrontPos + sizeof(iValue)) > m_chpRearPos)
		{
			throw std::exception::exception();
		}*/
		memcpy_s(&iValue, sizeof(iValue), m_chpFrontPos, sizeof(iValue));
		m_chpFrontPos += sizeof(iValue);
		return *this;
	}
	CPacket& operator >> (long& lValue)
	{
		/*if ((m_chpFrontPos + sizeof(lValue)) > m_chpRearPos)
		{
			throw std::exception::exception();
		}*/
		memcpy_s(&lValue, sizeof(lValue), m_chpFrontPos, sizeof(lValue));
		m_chpFrontPos += sizeof(lValue);
		return *this;
	}
	CPacket& operator >> (float& fValue)
	{
		/*if ((m_chpFrontPos + sizeof(fValue)) > m_chpRearPos)
		{
			throw std::exception::exception();
		}*/
		memcpy_s(&fValue, sizeof(fValue), m_chpFrontPos, sizeof(fValue));
		m_chpFrontPos += sizeof(fValue);
		return *this;
	}
	CPacket& operator >> (__int64& iValue)
	{
		/*if ((m_chpFrontPos + sizeof(iValue)) > m_chpRearPos)
		{
			throw std::exception::exception();
		}*/
		memcpy_s(&iValue, sizeof(iValue), m_chpFrontPos, sizeof(iValue));
		m_chpFrontPos += sizeof(iValue);
		return *this;
	}
	CPacket& operator >> (double& dValue)
	{
		/*if ((m_chpFrontPos + sizeof(dValue)) > m_chpRearPos)
		{
			throw std::exception::exception();
		}*/
		memcpy_s(&dValue, sizeof(dValue), m_chpFrontPos, sizeof(dValue));
		m_chpFrontPos += sizeof(dValue);
		return *this;
	}
};