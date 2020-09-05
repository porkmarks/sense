#pragma once

#include <cstdint>

#ifndef BITSTREAM_CAPACITY
#	define BITSTREAM_CAPACITY 128 * 8
#endif

class Bitstream
{
public:
	enum { CAPACITY = BITSTREAM_CAPACITY };

	static_assert(CAPACITY % 8 == 0);

	Bitstream(uint8_t* data, uint16_t size, bool clear_rest);

	void clear();
	uint16_t size() const;
	bool write(uint32_t data, uint8_t bits);
	uint32_t read(uint16_t& pos, uint8_t bits);

private:
	uint16_t m_size;
	uint8_t* m_data;
};

inline Bitstream::Bitstream(uint8_t* data, uint16_t size, bool clear_rest)
	: m_size(size)
	, m_data(data)
{
	if (clear_rest && m_size / 8 < CAPACITY / 8)
	{
		size_t offset = size_t(m_size) / 8 + ((m_size & 7) ? 1 : 0);
		size_t sz = CAPACITY / 8 - offset;
		memset(m_data + offset, 0, sz);
	}
}

inline void Bitstream::clear()
{
	memset(m_data, 0, CAPACITY / 8);
	m_size = 0;
}

inline uint16_t Bitstream::size() const
{
	return m_size;
}

inline bool Bitstream::write(uint32_t data, uint8_t bits)
{
	if (bits > CAPACITY - m_size)
		return false;

	while (bits > 0)
	{
		uint8_t byteIndex = m_size >> 3;
		uint8_t bitIndex = m_size & 7;
		uint8_t& byte = m_data[byteIndex];

		byte |= (data & 1) << bitIndex;
		data >>= 1;

		m_size++;
		bits--;
	}

	return true;
}

inline uint32_t Bitstream::read(uint16_t& pos, uint8_t bits)
{
	if (bits > m_size - pos)
		return 0;

	uint32_t data = 0;
	for (uint8_t i = 0; i < bits; i++)
	{
		uint8_t byteIndex = pos >> 3;
		uint8_t bitIndex = pos & 7;
		uint8_t byte = m_data[byteIndex];

		data |= ((byte >> bitIndex) & 1) << i;

		pos++;
	}

	return data;
}
