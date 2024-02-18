#pragma once

#define RMVCOUNT(PTR) ((unsigned long long)PTR & 0x00007FFFFFFFFFFF)
#define ISPCOUNT(CNT) (CNT & 0x000000000001FFFF)
#define SETCOUNT(TYPE, PTR, CNT) PTR = (TYPE)(((unsigned long long)CNT << 47) | (unsigned long long)(PTR))

template <typename T>
class LockFreeMemoryPool
{
private:
	typedef struct st_node
	{
		T data;
		struct st_node* pNextNode;
		int counter = 0;
	} Node;
	Node* m_nodeHead = nullptr;

	DWORD m_tls;
	int m_bucketSize;
	int m_capacity;
	int m_useCnt;
	bool m_placementNew;

	bool m_initFlag = false;
	bool m_init = false;

	template <class T>
	friend class Bucket;

	template <class T>
	friend class MemoryPoolTLS;
private:
	LockFreeMemoryPool() { m_tls = TlsAlloc(); }
	LockFreeMemoryPool(const LockFreeMemoryPool& ref) {}
	LockFreeMemoryPool& operator=(const LockFreeMemoryPool& ref) {}
	~LockFreeMemoryPool()
	{
		Node* ptr = m_nodeHead;

		while (ptr != nullptr)
		{
			Node* delNode = ptr;
			Node* rmvCountDelNode = ((Node*)RMVCOUNT(ptr));
			ptr = rmvCountDelNode->pNextNode;

			if (m_placementNew == false)
				delete rmvCountDelNode;
			else
				free(rmvCountDelNode);
		}

		//TlsFree(m_tls);
	}
public:
	static LockFreeMemoryPool<T>& getInstance() {
		static LockFreeMemoryPool<T> instance;
		return instance;
	}

	void InitializePool(int bucketSize, int allocBucketSize, bool bPlacementNew)
	{
		if (InterlockedExchange8((char*)&m_init, true) == false)
		{
			m_capacity = allocBucketSize;
			m_placementNew = bPlacementNew;
			m_bucketSize = bucketSize;

			for (int bucketCnt = 0; bucketCnt < allocBucketSize; ++bucketCnt)
			{
				Node* newNode = new Node;
				newNode->pNextNode = nullptr;

				if (m_nodeHead == nullptr)
				{
					m_nodeHead = newNode;
				}
				else
				{
					newNode->pNextNode = m_nodeHead;
					m_nodeHead = newNode;
				}
			}

			m_initFlag = true;
		}
		else
		{
			while (!m_initFlag) {}
		}
	}

	T* Alloc()
	{
		Node* allocNode;
		Node* rmvCountAllocNode;

		Node* nextNode;
		do
		{
			allocNode = m_nodeHead;
			rmvCountAllocNode = (Node*)RMVCOUNT(allocNode);
			if (allocNode == nullptr)
			{
				InterlockedIncrement((long*)&m_capacity);
				InterlockedIncrement((long*)&m_useCnt);
				allocNode = new Node;
				allocNode->pNextNode = nullptr;
				return &allocNode->data;
			}

			nextNode = rmvCountAllocNode->pNextNode;
		} while ((Node*)InterlockedCompareExchangePointer((PVOID*)&m_nodeHead, (PVOID)nextNode, (PVOID)allocNode) != allocNode);

		InterlockedIncrement((long*)&m_useCnt);

		return &rmvCountAllocNode->data;
	}


	void Free(T* pBucket)
	{
		Node* localHead;
		Node* freeNode = (Node*)pBucket;
		Node* rmvCountFreeNode = (Node*)pBucket;

		rmvCountFreeNode->counter = ISPCOUNT((rmvCountFreeNode->counter + 1));

		SETCOUNT(Node*, freeNode, (freeNode->counter));

		do {
			localHead = m_nodeHead;
			rmvCountFreeNode->pNextNode = localHead;
		} while ((Node*)InterlockedCompareExchangePointer((PVOID*)&m_nodeHead, (PVOID)freeNode, (PVOID)localHead) != localHead);

		InterlockedDecrement((long*)&m_useCnt);
	}

	int		GetCapacityCount(void) { return m_capacity; }
	int		GetUseCount(void) { return m_useCnt; }
};