#pragma once

#include <stdint.h>
#include <string.h>
#include <assert.h>

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

	void write8(uint8_t data, uint8_t bits);
	uint8_t read8(uint16_t& pos, uint8_t bits);

	void write(uint32_t data, uint8_t bits);
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

const static uint8_t k_left_shift[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };

inline void Bitstream::write8(uint8_t data, uint8_t bits)
{
	if (bits > CAPACITY - m_size)
		return;

	//Little Endian: least significant bit goes first in the stream

	uint8_t dst_byte_index = m_size >> 3;
	uint8_t dst_bit_index = m_size & 7;
	uint8_t dst_mask = k_left_shift[dst_bit_index]; //1 << dst_bit_index
	uint8_t dst_byte = m_data[dst_byte_index];

	m_size += bits;

	uint8_t src_mask = k_left_shift[bits - 1]; //1 << (bits - 1)
	while (src_mask > 0)
	{
		if (data & src_mask)
			dst_byte |= dst_mask;

		src_mask >>= 1;
		dst_mask <<= 1;
		if (dst_mask == 0)
		{
			dst_mask = 1;
			m_data[dst_byte_index] = dst_byte;
			dst_byte = m_data[++dst_byte_index];
		}
	}

	m_data[dst_byte_index] = dst_byte;
}

inline uint8_t Bitstream::read8(uint16_t& pos, uint8_t bits)
{
	if (bits > m_size - pos)
		return 0;

	//Little Endian: least significant bit goes first in the stream

	uint8_t src_byte_index = pos >> 3;
	uint8_t src_bit_index = pos & 7;
	uint8_t src_mask = k_left_shift[src_bit_index];
	uint8_t src = m_data[src_byte_index++];

	pos += bits;

	uint8_t dst = 0;
	uint8_t dst_mask = k_left_shift[bits - 1];
	while (dst_mask > 0)
	{
		if (src & src_mask)
			dst |= dst_mask;

		dst_mask >>= 1;
		src_mask <<= 1;
		if (src_mask == 0)
		{
			src_mask = 1;
			src = m_data[src_byte_index++];
		}
	}

	return dst;
}

inline void Bitstream::write(uint32_t data, uint8_t bits)
{
	if (bits > CAPACITY - m_size)
		return;

	const uint8_t* src_ptr = (const uint8_t*)&data;
	while (bits >= 8)
	{
		write8(*src_ptr++, 8);
		bits -= 8;
	}

	if (bits > 0)
		write8(*src_ptr++, bits);
}

inline uint32_t Bitstream::read(uint16_t& pos, uint8_t bits)
{
	if (bits > m_size - pos)
		return 0;

	uint32_t data = 0;
	uint8_t* src_ptr = (uint8_t*)&data;
	while (bits >= 8)
	{
		*src_ptr++ = read8(pos, 8);
		bits -= 8;
	}

	if (bits > 0)
		*src_ptr++ = read8(pos, bits);

	return data;
}
