#pragma once
#include "MemoryPoolTLS.h"

template <typename DATA>
class CQueue
{
private:
	typedef struct _node
	{
		DATA data;
		int counter = 0;
		struct _node* next;
	}Node;
	Node* m_head;
	Node* m_tail;

	Node* m_endValue;

	MemoryPoolTLS<Node> m_pool;

	alignas(64) 
	int m_size;

	static unsigned long long m_staticEndValue;
public:
	CQueue()
	{
		m_size = 0;

		m_head = m_pool.Alloc();
		m_endValue = (Node*)InterlockedIncrement64((long long*)&m_staticEndValue);
		m_head->next = m_endValue;
		m_tail = m_head;
	}

	void TailRenewal()
	{
		Node* localTail = m_tail;
		Node* curTail = localTail;
		Node* nextTail = ((Node*)RMVCOUNT(localTail))->next;

		while (nextTail != m_endValue)
		{
			curTail = nextTail;
			nextTail = ((Node*)RMVCOUNT(nextTail))->next;
		}

		if (curTail != localTail)
		{
			InterlockedCompareExchangePointer((PVOID*)&m_tail, curTail, localTail);
		}
	}

	void Enqueue(DATA data)
	{
		Node* enqueueNode = m_pool.Alloc();
		enqueueNode->data = data;
		enqueueNode->next = m_endValue;

		SETCOUNT(Node*, enqueueNode, enqueueNode->counter);

		Node* localTail;
		Node* tailNextNode;
		Node* rmvCountLocalTail;

		while (true)
		{
			localTail = m_tail;
			rmvCountLocalTail = (Node*)RMVCOUNT(localTail);
			tailNextNode = rmvCountLocalTail->next;

			if (tailNextNode == m_endValue)
			{
				if (InterlockedCompareExchangePointer((PVOID*)&rmvCountLocalTail->next, enqueueNode, tailNextNode) == tailNextNode)
				{
					InterlockedCompareExchangePointer((PVOID*)&m_tail, enqueueNode, localTail);
					break;
				}
			}
			else
			{
				TailRenewal();
			}
		}

		InterlockedIncrement((long*)&m_size);
	}

	bool Dequeue(DATA& data)
	{
		Node* dequeueNode;
		Node* localHead;
		Node* rmvCountLocalHead;

		while (true)
		{
			localHead = m_head;
			rmvCountLocalHead = (Node*)RMVCOUNT(localHead);
			dequeueNode = rmvCountLocalHead->next;

			if (m_size == 0)
			{
				return false;
			}

			if (localHead == m_tail)
			{
				TailRenewal();
			}
			else if (dequeueNode != m_endValue && InterlockedCompareExchangePointer((PVOID*)&m_head, dequeueNode, localHead) == localHead)
				break;
		}

		Node* rmvCountDequeueNode = (Node*)RMVCOUNT(dequeueNode);
		data = rmvCountDequeueNode->data;

		rmvCountLocalHead->counter = ISPCOUNT((rmvCountLocalHead->counter + 1));
		m_pool.Free(rmvCountLocalHead);

		InterlockedDecrement((long*)&m_size);

		return true;
	}

	void Peek(DATA* dataArr, int size)
	{
		Node* curHead = (Node*)RMVCOUNT(((Node*)RMVCOUNT(m_head))->next);
		for (int cnt = 0; cnt < size; ++cnt)
		{
			dataArr[cnt] = curHead->data;
			curHead = (Node*)RMVCOUNT(curHead->next);
		}
	}

	int GetUseSize() { return m_size; }
};