#pragma once
#include "LockFreeMemoryPool.h"
#include "Bucket.h"

template <typename T>
class MemoryPoolTLS
{
	friend class Bucket<T>;
private:
	DWORD m_tls;
	int m_bucketSize;
public:
	MemoryPoolTLS(int allocBucketSize = 0, int bucketSize = 10000, bool bPlacementNew = false)
	{
		m_bucketSize = bucketSize;

		if (!LockFreeMemoryPool<Bucket<T>>::getInstance().m_initFlag)
		{
			LockFreeMemoryPool<Bucket<T>>::getInstance().InitializePool(bucketSize, allocBucketSize, bPlacementNew);
		}
		m_tls = LockFreeMemoryPool<Bucket<T>>::getInstance().m_tls;
	}

	~MemoryPoolTLS()
	{

	}

	T* Alloc()
	{
		Bucket<T>* bucket = (Bucket<T>*)TlsGetValue(m_tls);

		if (bucket == nullptr)
		{
			bucket = (Bucket<T>*)LockFreeMemoryPool<Bucket<T>>::getInstance().Alloc();
			TlsSetValue(m_tls, bucket);

			bucket->InitializeBucket();
		}

		return bucket->Alloc();
	}

	void Free(T* delPtr)
	{
		Bucket<T>* pBucket = Bucket<T>::GetBucketPtr(delPtr);
		pBucket->Free(delPtr);
		/*if (pBucket->Free(delPtr))
		{
			LockFreeMemoryPool<Bucket<T>>::getInstance().Free(pBucket);
		}*/
	}

	int GetPoolCapacity()
	{
		return LockFreeMemoryPool<Bucket<T>>::getInstance().GetCapacityCount() * m_bucketSize;
	}

	int GetPoolUse()
	{
		int useCnt = LockFreeMemoryPool<Bucket<T>>::getInstance().GetUseCount();
		return useCnt * m_bucketSize;
	}
};