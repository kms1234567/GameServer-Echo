#pragma once
#include <iostream>

class CRingBuffer
{
public:
	enum {
		BASE_BUFFER_SIZE = 8192, MAX_BUFFER_SIZE = 32768
	};
private:
	char* buffer;
	char* front;
	char* rear;
	int current_buffer_size;
public:
	CRingBuffer(int buffersize)
	{
		buffersize++;

		buffer = new char[buffersize];
		front = buffer;
		rear = buffer;
		current_buffer_size = buffersize;
	}
	CRingBuffer() : CRingBuffer(BASE_BUFFER_SIZE) {}

	bool Resize(int size)
	{
		if ((size <= current_buffer_size) || (size > MAX_BUFFER_SIZE))
		{
			return false;
		}

		size++;

		if (current_buffer_size >= size)
		{
			return false;
		}

		int front_interval = front - buffer;
		int rear_interval = rear - buffer;

		char* new_buffer = new char[size];
		memcpy_s(new_buffer, size, buffer, current_buffer_size);

		delete[]buffer;

		current_buffer_size = size;
		front = new_buffer + front_interval;
		rear = new_buffer + rear_interval;
		buffer = new_buffer;
		return true;
	}

	int GetBufferSize(void)
	{
		return (current_buffer_size - 1);
	}

	int GetUseSize(void)
	{
		char* pRear = rear;
		char* pFront = front;

		if (pRear >= pFront)
			return pRear - pFront;
		else
			return current_buffer_size + (pRear - pFront);
	}

	int GetFreeSize(void)
	{
		char* pRear = rear;
		char* pFront = front;

		if (pRear >= pFront)
			return (current_buffer_size - 1) - (pRear - pFront);
		else
			return pFront - pRear - 1;
	}

	int Enqueue(char* msg, int size)
	{
		int enqueue_byte;
		int space = (buffer + current_buffer_size - 1) - rear;
		int free_size = GetFreeSize();

		if (free_size < size) size = free_size;

		if (space >= size) enqueue_byte = size;
		else enqueue_byte = space;

		memcpy_s(rear + 1, enqueue_byte, msg, enqueue_byte);

		rear = rear + enqueue_byte;
		size -= enqueue_byte;

		if (size != 0)
		{
			memcpy_s(buffer, size, msg + enqueue_byte, size);
			enqueue_byte += size;
			rear = buffer + (size - 1);
		}

		return enqueue_byte;
	}

	int Dequeue(char* msg, int size)
	{
		int dequeue_byte;
		int space = (buffer + current_buffer_size - 1) - front;
		int use_size = GetUseSize();

		if (use_size < size) size = use_size;

		if (space >= size) dequeue_byte = size;
		else dequeue_byte = space;

		memcpy_s(msg, dequeue_byte, front + 1, dequeue_byte);

		front = front + dequeue_byte;
		size -= dequeue_byte;

		if (size != 0)
		{
			memcpy_s(msg + dequeue_byte, size, buffer, size);
			dequeue_byte += size;
			front = buffer + (size - 1);
		}

		return dequeue_byte;
	}
	int Peek(char* msg, int size)
	{
		int dequeue_byte;
		int space = (buffer + current_buffer_size - 1) - front;

		int use_size = GetUseSize();

		if (use_size < size) size = use_size;

		if (space >= size) dequeue_byte = size;
		else dequeue_byte = space;

		memcpy_s(msg, dequeue_byte, front + 1, dequeue_byte);

		size -= dequeue_byte;

		if (size != 0)
		{
			memcpy_s(msg + dequeue_byte, size, buffer, size);
			dequeue_byte += size;
		}
		return dequeue_byte;
	}
	void ClearBuffer(void)
	{
		rear = front;
	}

	int MoveRear(int size)
	{
		int enqueue_byte;
		int space = (buffer + current_buffer_size - 1) - rear;
		int free_size = GetFreeSize();

		if (free_size < size) size = free_size;

		if (space >= size) enqueue_byte = size;
		else enqueue_byte = space;

		size -= enqueue_byte;
		rear += enqueue_byte;

		if (size != 0)
		{
			enqueue_byte += size;
			rear = buffer + (size - 1);
		}

		return enqueue_byte;
	}
	int MoveFront(int size)
	{
		int dequeue_byte;
		int space = (buffer + current_buffer_size - 1) - front;

		int use_size = GetUseSize();

		if (use_size < size) size = use_size;

		if (space >= size) dequeue_byte = size;
		else dequeue_byte = space;

		size -= dequeue_byte;
		front += dequeue_byte;

		if (size != 0)
		{
			dequeue_byte += size;
			front = buffer + (size - 1);
		}
		return dequeue_byte;
	}
	int DirectEnqueueSize(void)
	{
		char* pRear = rear;
		char* pFront = front;

		if ((buffer + current_buffer_size - 1) == pRear)
		{
			return pFront - buffer;
		}
		else
		{
			if (pRear >= pFront)
			{
				return (buffer + current_buffer_size - 1) - pRear;
			}
			else
			{
				return pFront - pRear - 1;
			}
		}
	}
	int DirectDequeueSize(void)
	{
		char* pRear = rear;
		char* pFront = front;

		if ((buffer + current_buffer_size - 1) == pFront)
		{
			return pRear - buffer + 1;
		}
		else
		{
			if (pRear >= pFront)
			{
				return pRear - pFront;
			}
			else
			{
				return (buffer + current_buffer_size - 1) - pFront;
			}
		}
	}
	char* GetBufferPtr(void)
	{
		return buffer;
	}
	char* GetFrontBufferPtr(void)
	{
		char* pFront = front;

		if ((buffer + current_buffer_size - 1) == pFront)
		{
			return buffer;
		}
		else
		{
			return (pFront + 1);
		}
	}
	char* GetRearBufferPtr(void)
	{
		char* pRear = rear;

		if ((buffer + current_buffer_size - 1) == pRear)
		{
			return buffer;
		}
		else
		{
			return (pRear + 1);
		}
	}

	virtual ~CRingBuffer()
	{
		delete[]buffer;
	}
};