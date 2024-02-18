#pragma once

template <class T>
class LockFreeMemoryPool;

template <typename T>
class Bucket
{
private:
	typedef struct st_node
	{
	public:
		T data;
		Bucket<T>* pBucket;
	} Node;
	Node* m_dataArr;

	DWORD m_tls;

	bool m_placementNew;

	int m_bucketSize;
	int m_curIdx;

	int m_freeCnt;
public:
	Bucket()
	{
		m_tls = LockFreeMemoryPool<Bucket<T>>::getInstance().m_tls;
		m_bucketSize = LockFreeMemoryPool<Bucket<T>>::getInstance().m_bucketSize;
		m_placementNew = LockFreeMemoryPool<Bucket<T>>::getInstance().m_placementNew;

		InitializeBucket();

		if (m_placementNew)
		{
			m_dataArr = (Node*)malloc(sizeof(Node) * m_bucketSize);
		}
		else
		{
			m_dataArr = (Node*)new Node[m_bucketSize];
		}

		for (int idx = 0; idx < m_bucketSize; ++idx)
		{
			m_dataArr[idx].pBucket = this;
		}
	}

	~Bucket()
	{
		if (m_placementNew)
		{
			free(m_dataArr);
		}
		else
		{
			delete[] m_dataArr;
		}
	}

	T* Alloc()
	{
		Node* retData = (Node*)&m_dataArr[m_curIdx++];

		if (m_curIdx == m_bucketSize)
		{
			Bucket<T>* bucket = LockFreeMemoryPool<Bucket<T>>::getInstance().Alloc();
			bucket->InitializeBucket();
			TlsSetValue(m_tls, bucket);
		}

		if (m_placementNew)
		{
			new(&(retData->data))T;
		}

		return (T*)retData;
	}

	void InitializeBucket()
	{
		m_curIdx = 0;
		m_freeCnt = 0;
	}

	void Free(T* delPtr)
	{
		if (m_placementNew)
		{
			Node* freeNode = (Node*)delPtr;
			freeNode->data.~T();
		}

		if (InterlockedIncrement((long*)&m_freeCnt) == m_bucketSize)
		{
			LockFreeMemoryPool<Bucket<T>>::getInstance().Free(this);
		}
	}

	static Bucket<T>* GetBucketPtr(T* delPtr)
	{
		Node* node = (Node*)delPtr;
		return node->pBucket;
	}
};