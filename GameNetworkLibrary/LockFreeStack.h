#include <iostream>
#include "MemoryPoolTLS.h"
using namespace std;

template <typename DATA>
class CStack
{
private:
	typedef struct _node
	{
		DATA data;
		int counter = 0;
		struct _node* next = nullptr;
	} Node;

	MemoryPoolTLS<Node> m_pool;

	Node* m_top = nullptr;

	alignas(64) 
	int m_size = 0;
public:
	CStack() { }

	void push(DATA data)
	{
		Node* localTop;
		Node* newNode = m_pool.Alloc();
		Node* rmvCountNode = newNode;
		rmvCountNode->data = data;

		SETCOUNT(Node*, newNode, newNode->counter);
		do {
			localTop = m_top;
			rmvCountNode->next = localTop;
		} while ((Node*)InterlockedCompareExchange64((LONG64*)&m_top, (LONG64)newNode, (LONG64)localTop) != localTop);

		InterlockedIncrement((long*)&m_size);
	}

	bool pop(DATA& data)
	{
		Node* delNode;
		Node* rmvCountDelNode;
		Node* nextNode = nullptr;
		do {
			delNode = m_top;
			rmvCountDelNode = (Node*)RMVCOUNT(delNode);
			if (rmvCountDelNode == nullptr)
			{
				if (m_size == 0)
				{
					data = NULL;
					return false;
				}
				continue;
			}
			nextNode = rmvCountDelNode->next;
		} while ((Node*)InterlockedCompareExchange64((LONG64*)&m_top, (LONG64)nextNode, (LONG64)delNode) != delNode);

		data = rmvCountDelNode->data;
		rmvCountDelNode->counter = ISPCOUNT((rmvCountDelNode->counter + 1));
		m_pool.Free(rmvCountDelNode);

		InterlockedDecrement((long*)&m_size);

		return true;
	}
};