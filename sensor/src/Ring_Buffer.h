//
//  TFRingBuffer.h
//
//  Created by Tom Fewster on 25/01/2013.
//
 
#ifndef TFObjectPool_TFRingBuffer_h
#define TFObjectPool_TFRingBuffer_h
 
#include <atomic>
#include <cstddef>
 
template <typename T> class TFRingBuffer {
	T *m_buffer;
	std::atomic<size_t> m_head;
	std::atomic<size_t> m_tail;
	const size_t m_size;
 
	size_t next(size_t current) {
		return (current + 1) % m_size;
	}
 
public:
 
	TFRingBuffer(const size_t size) : m_size(size), m_head(0), m_tail(0) {
		m_buffer = new T[size];
	}
 
	virtual ~TFRingBuffer() {
		delete [] m_buffer;
	}
 
	bool push(const T &object) {
		size_t head = m_head.load(std::memory_order_relaxed);
		size_t nextHead = next(head);
		if (nextHead == m_tail.load(std::memory_order_acquire)) {
			return false;
		}
		m_buffer[head] = object;
		m_head.store(nextHead, std::memory_order_release);
 
		return true;
	}
 
	bool pop(T &object) {
		size_t tail = m_tail.load(std::memory_order_relaxed);
		if (tail == m_head.load(std::memory_order_acquire)) {
			return false;
		}
 
		object = m_buffer[tail];
		m_tail.store(next(tail), std::memory_order_release);
		return true;
	}
};
 
 
#endif